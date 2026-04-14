/**
 * @file scabbard.cpp
 * @author Andrew Osterhout (osterhoutan+scabbard@gmail.com)

 * @brief The instrumentation pass for the gpu thread sanitizer
 *
 * @date 2024-09-17
 *
 * @copyright Copyright (c) 2024
 * @license APACHE 2.0 with llvm exceptions
 *
 */

#include "ScabbardPass.hpp"

#include <scabbard/instr-names.def>
#include <scabbard/Enums.hpp>

#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Module.h>
#if __clang_major__ <= 16
#include <llvm/ADT/Triple.h>
#else
#include <llvm/TargetParser/Triple.h>
#endif
#include <llvm/ADT/SetVector.h>
#include <llvm/ADT/SmallSet.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>
// #include <llvm/Transforms/Utils/Cloning.h>

#include <unordered_set>
#include <functional>

#define DEBUG_TYPE "scabbard"

#define EMPTY_LIST_6 {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr}

namespace llvm {
namespace /*<anon>*/ {

using namespace scabbard;

// << ========================================================================================== >>
// <<                                     METADATA HANDLER                                       >>
// << ========================================================================================== >>

/// @brief This class is a helper that will enable the creation of a metadata object in the module's global space.
///        This metadata object outputted into the module will be:
///        - smart enough to not include duplicate locations
///        - be constant at runtime
///        - be indexed by a unique id
///        Each item in the metadata object will consist of:
///        - the src file, line and col associated with the instruction of interest
///        - what ever static data can be collected about the operation in the form of a compound enum.
///        Future plans involve:
///        - a non-static portion that will track the stack trace of the thread in question.
///        - a way to register when in fn's even if they have been inlined (might require an earlier pass)
class MetadataHandler {
  DenseMap<const Instruction*, uint64_t> Instructions;
  // SmallPtrSet<const Instruction, 16ul> Instructions;
  DenseMap<const Twine, uint64_t> UniqueStrings; // Note: using Twine instead of string or StringRef here might not work
  std::string ConcatenatedString;
  size_t UniqueStringId = 0ul;

public:
  /// @brief Insert the metadata associated with an instruction into the set of all
  ///        instructions of interest. (set kept unique)
  /// @param I the instruction to try to insert
  /// @return \c std::pair<size_t,bool> - size_t is the metadata id for the instruction
  ///         bool is \c true if the insertion takes place,
  ///         and \c false if it fails to occur because of an issue or it already
  ///         existed in the metadata set
  inline std::pair<size_t, bool> insert(const Instruction* I);

  /// @brief Call \ref insert , but return the metadata id as a \c ConstantInt rather than the bundled pair.
  /// @param I the instruction to get the metadata id for
  /// @return \c llvm::Constant* - constant int value of the Metadata ID for the instruction.
  inline Constant* getId(const Instruction* I);

  /// @brief Create the module metadata object
  /// @param M the module to inject the metadata object into
  /// @param AddrSpace the integer value associated with the global address space for
  ///        the target architecture (0 for CPU/HOST, 1 for most GPUs/DEVICES)
  /// @return \c llvm::GlobalVariable* - pointer to the module global created holding the metadata.
  GlobalVariable* outputMetadata(Module& M, unsigned AddrSpace);

private:
  /// @brief Add a string to the set of unique strings that will be
  ///        exported as a concatenated array into the module separated with \c \\0
  ///        that will be called \c scabbard_device_strings for use with the RTL.
  ///        The offset to the start of this string in this array will be returned.
  /// @param Str the string to insert
  /// @return \c uint64_t the offset in the global string array to the start of \p Str
  uint64_t registerString(const Twine& Str);

  /// @brief output the global string array
  ///        (should only be done once and only after all required strings are
  ///         registered with this \c MetadataHandler )
  /// @param M the module to add the global string array to
  /// @param AddrSpace the integer value associated with the global address space for
  ///        the target architecture (0 for CPU/HOST, 1 for most GPUs/DEVICES)
  /// @return the global string array already inserted into the module
  GlobalVariable* outputStrings(Module& M, unsigned AddrSpace) const;

  /// @brief Retrieve the source directory & file name from the metadata,
  ///        combine them into a full file path, try to register the string with
  ///        \c registerString, and return the offset to the desired string in
  ///        the global string array as a constant int.
  /// @param File the metadata object containing the file and directory components
  /// @param C (required for generating constants for the module)
  /// @return a constant int of the offset in the global string array to the file name.
  inline ConstantInt* getSourceFile(const DIFile* File, LLVMContext& C);

  /// @brief Get the line number from the source file
  ///        of the instruction in question as a constant.
  /// @param Loc the debug location metadata object for the instruction in question
  /// @param C (required for generating constants for the module)
  /// @return a constant int of the line number
  inline ConstantInt* getSourceLine(const DILocation* Loc, LLVMContext& C) const;

  /// @brief Get the line column index from the source file as a constant
  /// @param Loc the debug location metadata object for the instruction in question
  /// @param C (required for generating constants for the module)
  /// @return a constant int of the line column index
  inline ConstantInt* getSourceCol(const DILocation* Loc, LLVMContext& C) const;

  /// @brief Retrieve the name of the original function containing the instruction in question
  ///        try to register the string with \c registerString,
  ///        and return the offset to the desired string in the global string array
  ///        as a constant int.
  /// @param FnName the name of the function that originally contained the instruction in question
  ///        (usually stored in the as Subprogram linkage name)
  /// @param C (required for generating constants for the module)
  /// @return a constant int of the the offset in the global string array to the function name
  inline ConstantInt* getCalledFn(const StringRef& FnName, LLVMContext& C);
};


/// @brief Class for standard design decisions common to all Scabbard Instrumentation passes
class IScabbardInstrPass {
protected:
public:
  /// @brief Produce a string for log/error messages that gives the source file and location in 
  ///        \c `\"<filepath>\":<line>,<col>` format
  /// @param I instruction to get the location for
  /// @return \c std::string - the formatted location string for log/error messages
  static inline Twine getLocStr(const Instruction& I);

  /// @brief The general kind of memory space of a pointer type in a AMD Device.
  using PtrOrigin = scabbard::InstrData;

};


/// @brief A helper class that holds a reference to the module the pass is working on 
///        and some commonly used types for easy access.
class IRHelper {
protected:
  /// @brief a handy copy of module to use when you just need to get a context or quick query.
  const Module& _M;
  /// @brief a handy pointer to the oft used IR void type.
  Type* const VoidTy = null;
  /// @brief a handy pointer the oft used generic IR pointer type for this module.
  PointerType* const PtrTy = null;
  /// @brief a handy pointer to the oft used IR Integer Type for the RTL's TraceData
  IntegerType* const TraceDataTy = null;
  /// @brief a handy pointer to the oft used IR Integer Type for the RTL's source code location metadata ID
  IntegerType* const LocDataTy = null;
  /// @brief a handy pointer to the oft used IR Integer Type for the RTL's exta info (size_t/uint64_t) type.
  IntegerType* const ExtraDataTy = null;
public:
  IRHelper() = delete;
  IRHelper(Module& M) :
    _M(M),
    VoidTy(Type::getVoidTy(M.getContext())),
    PtrTy(PointerType::get(M.getContext(), 0ull)),
    TraceDataTy(IntegerType::get(M.getContext(), sizeof(InstrData)*8)),
    LocDataTy(IntegerType::get(M.getContext(), 64u)),
    ExtraDataTy(IntegerType::get(M.getContext(), 64u))
    {}
  
  // ConstantInt* getConstInt(const IntegerType* Ty, uint64_t val) const {
  //   return ConstantInt::get((Type*)Ty, APInt(val,Ty->getBitWidth()));
  // }
};


// << ========================================================================================== >>
// <<                                   HOST INSTR IMPL PASS                                     >>
// << ========================================================================================== >>

/// @brief scabbard's host instrumentation pass.
///        It will instrument relevant memory allocation and deallocation events
///        in order to facilitate the use of shadow memory on the device.
///        Additionally, it will modify kernel launches to pass in a thread state
///        object into device code.
///        Finally it will modify global variable allocations to
///        contain shadow memory.
///
class IScabbardHostPass : public IScabbardInstrPass, public IRHelper {

  bool run(Function& F, FunctionAnalysisManager& FAM) {
    // prevent instrumenting functions that should not be instrumented
    if (not isInstrumentableFn(F))
      return false;
    // TODO any prereq work...
    // run the actual implementation that might be modified in inherited classes
    return runImpl(F, FAM);
  }

  /// @brief Ensure that Scabbard's RTL Fn's are defined in this module and
  ///        pointed to by the appropriate member of \c ScabbardHostRTL struct. 
  /// @param M module to insert/inspect.
  inline void registerRTL(Module& M);

  /// @brief Find all global variables that are in unified memory (as known by this TU).
  ///        Store in \c GLobalUnifiedMemVar .
  /// @param M module to search.
  inline void registerGlobalVarsInUnifiedMemory(const Module& M);

protected:

  /// @brief The target triple to reference for OS and Arch variations on the host.
  const Triple _Triple;

  /// @brief A struct used to organize all of the Code and IR elements of 
  ///        Scabbard's RTL into one place.
  struct ScabbardHostRTL {
    llvm::FunctionCallee scabbard_init;
    const std::string scabbard_init_name = SCABBARD_CALLBACK_INIT_NAME;
    llvm::FunctionCallee trace_append$mem;
    const std::string trace_append$mem_name = SCABBARD_HOST_CALLBACK_APPEND_MEM_NAME;
    llvm::FunctionCallee trace_append$mem$cond; 
    const std::string trace_append$mem$cond_name = SCABBARD_HOST_CALLBACK_APPEND_MEM_COND_NAME;
    llvm::FunctionCallee trace_append$alloc;
    const std::string trace_append$alloc_name = SCABBARD_HOST_CALLBACK_APPEND_ALLOC_NAME;
    llvm::FunctionCallee register_job;
    const std::string register_job_name = SCABBARD_CALLBACK_REGISTER_JOB;
    llvm::FunctionCallee register_job_callback;
    const std::string register_job_callback_name = SCABBARD_CALLBACK_REGISTER_JOB_CALLBACK;
    MetadataHandler Metadata;
  };
  ScabbardHostRTL scabbard;

  /// @brief Map of GLobal Variable Names of Variables known to be in Unified memory to what heap they are in. \n 
  ///        (if not in map assume it is in \c UNKNOWN_HEAP ).
  StringMap<PtrOrigin> GlobalUnifiedMemVar;

  /// @brief Fn Type that describes a Function that will instrument a call of a GPU Driver API Fn, 
  ///        and return true if any changes were made to the module durring the process.
  using APIInstrumenterFn_t = std::function<bool(CallInst&,FunctionAnalysisManager&)>;

  /// @brief List of {APIFnName, APIHandlerFn} that will proccess the relevant GPU Driver API calls.
  SmallVector<std::pair<const StringRef,APIInstrumenterFn_t>,8ull> APIInstrumenters;

  /// @brief Returns \c false if the function should not be instrumented. \n
  ///        Used by the top level run method to determine if the pass will run on a fn. \n 
  ///        Override in child classes if you want to change what is instrumentable.
  /// @param F the function to examine
  /// @return \c bool - if \param F should be instrumented or skipped.
  virtual inline bool isInstrumentableFn(const Function& F) const {
    return (not F.isDeclaration()                        // exclude any functions not defined
            && not F.getName().starts_with("llvm.")      // exclude intrinsics
            && not F.getName().starts_with("scabbard.")  // exclude name mangled scabbard rtl functions
            && not F.hasFnAttribute("disable_sanitizer_instrumentation")); // any fn marked as not to be instrumented
  }


  /// @brief Instrument host module for scabbard, implementation details for child classes.
  /// @param M the module to instrument 
  /// @param MAM the analysis manager for the module
  /// @return \c bool - if the module was modified
  virtual bool runImpl(Module& M, ModuleAnalysisManager& MAM) { return false; }

  /// @brief Instrument a host function for scabbard.
  /// @param F the function who's body will be instrumented.
  /// @param FAM the analysis manager for above function.
  /// @return \c bool - if the function was modified.
  virtual bool runImpl(Function& F, FunctionAnalysisManager& FAM);

  /// @brief Determine if a Value of a pointer type points to thread local
  ///          global or shared memory, via static analysis.
  ///        In cases where it cannot be determined statically report it as such.
  ///        (Derived from @jdoerfert@github.com 's GPUSAN function with the same name)
  /// @param LI the loop info analysis object from the parent function.
  /// @param Ptr the Value of some pointer type to be analyzed.
  /// @param Object The object that is the origin of Ptr.
  /// @return \c scabbardIDevicePass::PtrOrigin - the memory space type of the
  ///         ptr provided.
  virtual PtrOrigin getPtrOrigin(LoopInfo& LI, Value* Ptr, const Value** Object) const;

  /// @brief Simple single interface to insert the call to Scabbard's RTL func for any kind of instruction.
  ///        It will determine bassed off of where \c Ptr comes from if it should instrument before instrumenting.
  ///        If it should not instrument it will return \c false.
  /// @param LI The Loop info for the function/module
  /// @param I The instruction being instrumented
  /// @param Ptr The Pointer Operand of above Instruction
  /// @param InstrContext The metadata about where the memory is located and what actions are being done on it.
  /// @param ExtraData If the use of an extra data variant of a scabbard RTL fn should be used. 
  /// @return if the instrumentation actually took place.
  inline bool instrumentInScabbardFunc(LoopInfo& LI, Instruction& I, Value* Ptr, const InstrData InstrContext, const bool ExtraData=false) const;


  /// @brief When a load instruction is found in a fn this is called.
  ///        It will decide if the Fn is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param Load the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  virtual bool instrumentLoadInst(LoopInfo& LI, LoadInst& Load) {
    return instrumentInScabbardFunc(LI, Load, Load.getPointerOperand(), 
                                    InstrData::ON_CPU | InstrData::READ | (Load.isAtomic() ? InstrData::ATOMIC : InstrData::NO));
  }

  /// @brief When a store instruction is found in a fn this is called.
  ///        It will decide if the Fn is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param Store the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  virtual bool instrumentStoreInst(LoopInfo& LI, StoreInst& Store) {
    return instrumentInScabbardFunc(LI, Store, Store.getPointerOperand(),
                                    InstrData::ON_CPU | InstrData::WRITE | (Store.isAtomic() ? InstrData::ATOMIC : InstrData::NO));
  }

  /// @brief When a atomicrmw instruction is found in a fn this is called.
  ///        It will decide if the Fn is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param RMW the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  virtual bool instrumentAtomicRMWInst(LoopInfo& LI, AtomicRMWInst& RMW) {
    return instrumentInScabbardFunc(LI, RMW, RMW.getPointerOperand(), 
                                    InstrData::ON_CPU | InstrData::READ | InstrData::WRITE | InstrData::ATOMIC);
  }

  /// @brief When a cmpxchg instruction is found in a fn this is called.
  ///        It will decide if the Fn is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param CXC the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  virtual bool instrumentCmpXChgInst(LoopInfo& LI, AtomicCmpXchgInst& CXC) {
    return instrumentInScabbardFunc(LI, CXC, CXC.getPointerOperand(),
                                    InstrData::ON_CPU | InstrData::WRITE | InstrData::ATOMIC);
  }

  /// @brief When a fence instruction is found in a fn this is called.
  ///        It will decide if the Fn is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param Fence the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  // bool instrumentFenceInst(LoopInfo& LI, FenceInst& Fence) override {}

  /// @brief When a call instruction is found in a fn this is called.
  ///        It will decide if the Fn is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param Call the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  virtual bool instrumentCallInst(LoopInfo& LI, CallInst& Call) { return false; } // not used, replaced with a definition based approach

  /// @brief Look at all the calls to GPU Driver API functions and handle them as needed.
  /// @param M The module to search in 
  /// @param FAM the analysis manager to pass into the APIInstrumenter Fn. 
  ///            (usually actually a \c llvm::FunctionAnalysisManagerModuleProxy  )
  /// @return \c bool - if any changes were made to the IR.
  virtual bool handleAPICalls(Module& M, FunctionAnalysisManager& FAM) {
    bool changed = false;
    for (auto [APIName, InstrumenterFn] : APIInstrumenters)
      if (Function* APIFn = M.getFunction(APIName))
        for (auto user : APIFn->users())
          if (auto _CI = dyn_cast<CallInst>(user))
            if (APIFn == _CI->getCalledFunction())
              changed |= InstrumenterFn(*_CI, FAM);
  }


public:

  IScabbardHostPass() = delete;
  IScabbardHostPass(Module& M, const Triple& Triple_) : 
    IRHelper(M), 
    _Triple(Triple_) 
    {}
    
  /// @brief Mostly just a Fn to keep programers from using the base host version of the pass rather
  ///        than the impl meant for the specific GPU driver API. 
  ///        ( \em i.e. hip, cuda, openmp, sycl, opencl, \em etc. )
  /// @return \c llvm::StringRef - plain text name of the API being used.
  virtual StringRef getGPUDriverAPIName() = 0;

  /// @brief Instrument a host module for scabbard.
  /// @param M the module who's contents will be instrumented.
  /// @param MAM the analysis manager for above module.
  /// @return \c bool - if the module was modified.
  virtual bool run(Module& M, ModuleAnalysisManager& MAM) final {
    
    // run the actual implementation that might be modified in inherited classes
    bool changed = runImpl(M, MAM);

    // notate all global variables that are known to be in Unified memory at some point (in this module/TU)
    registerGlobalVarsInUnifiedMemory(M);
    // add definitions for our RTL fn's
    registerRTL(M);

    FunctionAnalysisManager& FAM = MAM.getResult<FunctionAnalysisManagerModuleProxy>(M);

    for (Function& F: M.functions())
      changed |= run(F, FAM);

    // instrument the GPU driver API calls as appropriate for the specific API suite.
    changed |= handleAPICalls(M, FAM);
    
    if (changed) //NOTE: this method of metadata will need to be altered to work with instrumenting durring compilation instead of LTO.
      GlobalVariable& MetadataStringVar = scabbard.Metadata.outputMetadata(M, 0ull);
    
    return changed;
  }

};


/// @brief Scabbard instrumentation pass implementation for AMD's HIP API.
class ScabbardHostPassHip : public IScabbardHostPass {
  /// @brief Load up the API Instrumenters into \c IScabbardHostPass::APIInstrumenters
  void registerAPIInstrumenters();
protected:
  CallInst* CreateRTLCall(const CallInst& CI, scabbard::InstrData data, 
                          const Value* Ptr, bool InsertBefore=true) const;
  CallInst* CreateRTLCallEx(const CallInst& CI, scabbard::InstrData data, const Value* Ptr, 
                            const Value* Extra, bool InsertBefore=true) const;
  bool APIInstr_Alloc(CallInst&, FunctionAnalysisManager& FAM, InstrData Data) const;
public:
  ScabbardHostPassHip() = delete;
  ScabbardHostPassHip(Module& M_, const Triple& T_) :
    IScabbardHostPass(M_, T_)
  { registerAPIInstrumenters(); }

  StringRef getGPUDriverAPIName() final { return StringRef("amdhip"); }

};

// << ========================================================================================== >>
// <<                               DEVICE INSTR IMPL PASS INTER                                 >>
// << ========================================================================================== >>

/// @brief scabbard's device instrumentation pass.
///        It will define the default behavior for any make or model of GPU,
///        that can and should be overwritten for the various vendors.
///        It is an abstract/interface so you will need to extend it per vendor.
///
///        It will instrument memory read/write instructions,
///        sync/fence instructions with calls to the rtl,
///        and instrument local statically sized shared memory allocations to
///        involve shadow memory.
///        Current plans dictate the exclusion of instrumenting any memory action
///        on thread local memory.
///
///        Deriving classes will need to implement the following:
///        > TODO...
///
class scabbardIDevicePass : public IScabbardInstrPass, public IRHelper {
public:
  /* /// @brief The general kind of memory space of a pointer type in a AMD Device.
  typedef scabbard::InstrData PtrOrigin; */
  /* enum PtrOrigin {
    /// @brief Could not be determined statically at runtime.
    UNKNOWN = 0,
    /// @brief Device global memory, registered host memory, or managed memory.
    GLOBAL = 1,
    /// @brief Block level shared memory (allocated at kernel launch).
    MANAGED = 2,
    /// @brief Thread level local allocated on the stack by alloca instructions.
    LOCAL = 3,
    /// @brief Coming from vendor specific util like additional param ptr, or dispatch ptr.
    SYSTEM = 4,
    /// @brief Determined to be none of the Expected types
    NONE = 5;
    friend inline InstrData operator | (PtrOrigin l, InstrData r) { return (l & 0b111ul) | r; }
  }; */

protected:

  /// @brief Names of functions that are generated by the language or other
  ///        that should not be instrumented or have their parameters expanded
  static const std::unordered_set<std::string> NO_INSTR_FNS;

  /// @brief Calls to ambagious functions
  ///        (i.e. calls to pass in functions, etc)
  ///        These will need to be collected during function instrumentation,
  ///        But handled after all other functions have been instrumented,
  ///         and likely accounted for in ctors and dtors.
  SmallSetVector<CallBase*, 16> AmbiguousCalls;

  /// @brief if the module contains any alloca instructions
  bool HasAllocas = false;


  /// @brief collections of the types and fn's defined in scabbard's RTL.
  ///        will be pulled from the module as this pass should only run after
  ///        the rtl gets linked in.
  struct scabbardDeviceRTL {
    FunctionCallee trace_append$mem;
    const std::string trace_append$mem_name = SCABBARD_DEVICE_CALLBACK_APPEND_MEM_NAME;
    FunctionCallee trace_append$alloc;
    const std::string trace_append$alloc_name = SCABBARD_DEVICE_CALLBACK_APPEND_ALLOC_NAME;
    MetadataHandler Metadata;
  } scabbard;

public:
  scabbardIDevicePass() = delete;
  scabbardIDevicePass(Module& M) : IRHelper(M) {}

private:
  /// @brief Expand all defined functions to accept a pointer to a \c JobState object.
  ///        Will be run before the function \c run fn from the module \c run fn.
  /// @param M the module to be modified.
  /// @return \c bool - if the Module was changed at all
  inline bool expandFnParams(Module& M) const;

protected:
  /// @brief Get scabbard's RTL defined elements and store easy access versions in
  ///        the class instance.
  ///        \b NOTE: Assumes that the RTL has already been linked into the device module.
  /// @param M the device module to pull the RTL elements from.
  virtual void registerRTL(Module& M) final;

  /// @brief return if a function is a builtin for/from the device vendor.
  /// @param F the Fn in question.
  /// @return \c bool - \c true if \ref F is a builtin from the device vendor.
  virtual bool isDeviceVendorBuiltin(const Function& F) const {
    return false; // DEFAULT: don't do anything (CUDA/NVPTX impl needs to override)
  }

  /// @brief Check to see if this function should be instrumented.
  /// @param F the \c llvm::Function to check
  /// @return \c bool - true if it's appropriate to instrument the fn.
  virtual bool isInstrumentableFn(const Function& F) const final {
    return (not F.isDeclaration()                        // exclude any functions not defined
            && not F.getName().starts_with("llvm.")      // exclude intrinsics
            && not F.getName().starts_with("scabbard.")     // exclude name mangled scabbard rtl functions
            && not NO_INSTR_FNS.count(F.getName().str()) // a manually excluded function (usually c++ builtin)
            && not isDeviceVendorBuiltin(F)              // a device/vendor specific function. (should be intrinsics
            // and -> undeclared)
            && not F.hasFnAttribute("disable_sanitizer_instrumentation")); // any fn marked as not to be instrumented
  }

  /// @brief Return if the address-space of a global variable is in the
  ///        address space specified by the device vendor as shared memory.
  ///        (Abstract must be overriden in implementing class)
  /// @param G the global variable.
  /// @return \c bool - if \c G is in the shared address space.
  virtual bool isSharedGlobal(const GlobalVariable& G) const = 0;

  /// @brief Determine if a Value of a pointer type points to thread local
  ///          global or shared memory, via static analysis.
  ///        In cases where it cannot be determined statically report it as such.
  ///        (Derived from @jdoerfert@github.com 's GPUSAN function with the same name)
  /// @param LI the loop info analysis object from the parent function.
  /// @param Ptr the Value of some pointer type to be analyzed.
  /// @param Object The object that is the origin of Ptr.
  /// @return \c scabbardIDevicePass::PtrOrigin - the memory space type of the
  ///         ptr provided.
  inline PtrOrigin getPtrOrigin(LoopInfo& LI, Value* Ptr, const Value** Object) const;

  /// @brief Simple single interface to insert the call to Scabbard's RTL func for any kind of instruction.
  ///        It will determine bassed off of where \c Ptr comes from if it should instrument before instrumenting.
  ///        If it should not instrument it will return \c false.
  /// @param LI The Loop info for the function/module
  /// @param I The instruction being instrumented
  /// @param Ptr The Pointer Operand of above Instruction
  /// @param ScabbardFn The Specific Scabbard RTL Fn to instrument in
  /// @param InstrContext The metadata about where the memory is located and what actions are being done on it.
  /// @return if the instrumentation actually took place.
  inline bool instrumentInScabbardFunc(LoopInfo& LI, Instruction& I, Value* Ptr, 
                                       FunctionCallee& ScabbardFn, const InstrData InstrContext) const;

  /// @brief Create scabbard's module Constructor (ctor) function for the target arch.
  ///        (Must be defined in child classes,
  ///          \c scabbardIDevice pass marks it abstract)
  ///         Return the function and the calling class will insert it into
  ///         the appropriate global object to register it as a ctor.
  /// @param M the module to create the ctor in and for.
  /// @param MAM the analysis manager for above module.
  /// @param MetadataVar pointer to a global variable that contains the metadata,
  ///        and should have the register metadata RTL fn run with it.
  /// @return \ref llvm::Function \c* - ptr to the ctor fn.
  virtual Function* createCTor(Module& M, ModuleAnalysisManager& MAM, GlobalVariable* MetadataVar) const = 0;

  /// @brief Create scabbard's module deconstructor (dtor) function for the target arch.
  ///        (Must be defined in child classes,
  ///          \c scabbardIDevice pass marks it abstract)
  ///         Return the function and the calling class will insert it into
  ///         the appropriate global object to register it as a dtor.
  /// @param M the module to create the dtor in and for.
  /// @param MAM the analysis manager for above module
  /// @return \ref llvm::Function \c* - ptr to the dtor fn.
  virtual Function* createDTor(Module& M, ModuleAnalysisManager& MAM) const = 0;

  /// @brief When a load instruction is found in a fn this is called
  ///        (abstract, implementing classes must define).
  ///        It will decide if the instruction is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param Load the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  virtual bool instrumentLoadInst(LoopInfo& LI, LoadInst& Load) = 0;

  /// @brief When a store instruction is found in a fn this is called
  ///        (abstract, implementing classes must define).
  ///        It will decide if the instruction is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param Store the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  virtual bool instrumentStoreInst(LoopInfo& LI, StoreInst& Store) = 0;

  /// @brief When a atomicrmw instruction is found in a fn this is called
  ///        (abstract, implementing classes must define).
  ///        It will decide if the instruction is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param RMW the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  virtual bool instrumentAtomicRMWInst(LoopInfo& LI, AtomicRMWInst& RMW) = 0;

  /// @brief When a cmpxchg instruction is found in a fn this is called
  ///        (abstract, implementing classes must define).
  ///        It will decide if the instruction is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param CXC the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  virtual bool instrumentCmpXChgInst(LoopInfo& LI, AtomicCmpXchgInst& CXC) = 0;

  /// @brief When a fence instruction is found in a fn this is called
  ///        (abstract, implementing classes must define).
  ///        It will decide if the instruction is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param Fence the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  virtual bool instrumentFenceInst(LoopInfo& LI, FenceInst& Fence) = 0;

  /// @brief When a call instruction is found in a fn this is called
  ///        (abstract, implementing classes must define).
  ///        It will decide if the instruction is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param Call the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  virtual bool instrumentCallInst(LoopInfo& LI, CallInst& Call) = 0;

  /// @brief When a GetELementPointer instruction is found in a fn this is called.
  ///        It will decide if the instruction is worth instrumenting
  ///        and perform said instrumentation as required.
  ///        By default this does nothing and should be overriden if the \c instrumentAllocaInst
  ///        is overriden in your implementing class,
  ///        or if the instrumented tool uses fake pointers are used instead of shadow memory.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param GEP the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  virtual bool instrumentGEPInst(LoopInfo& LI, GetElementPtrInst& GEP) {
    return false; // DEFAULT: do nothing
  }

  /// @brief When a \c PtrToInt instruction is found in a fn this is called.
  ///        It will decide if the instruction is worth instrumenting
  ///        and perform said instrumentation as required.
  ///        By default this Function raises a warning about unsupported pointer arithmetic,
  ///        but can be overwritten in implementing classes.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param P2I the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  virtual bool instrumentPtrToIntInst(LoopInfo& LI, PtrToIntInst& P2I) {
    llvm::errs() << "\n[scabbard.device:WARN] unsupported pointer arithmetic, Scabbard's results may be invalid: "
                 << P2I.getDebugLoc() << "\n";
    return false; // DEFAULT: do nothing
  }

  /// @brief When a \c IntToPtr instruction is found in a fn this is called.
  ///        It will decide if the instruction is worth instrumenting
  ///        and perform said instrumentation as required.
  ///        By default this Function raises a warning about unsupported pointer arithmetic,
  ///        but can be overwritten in implementing classes.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param I2P the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  virtual bool instrumentIntToPtrInst(LoopInfo& LI, IntToPtrInst& I2P) {
    llvm::errs() << "\n[scabbard.device:WARN] unsupported pointer arithmetic, Scabbard's results may be invalid: "
                 << I2P.getDebugLoc() << "\n";
    return false; // DEFAULT: do nothing
  }

  /// @brief When a alloca instruction is found in a fn this is called
  ///        (does nothing by default, override to change this).
  ///        It will decide if the instruction is worth instrumenting
  ///        and perform said instrumentation as required.
  ///        By default this does nothing, and should be overriden if local stack
  ///        allocations are of interest to the instrumented tool.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param Alloca the instruction in question.
  /// @param V the value associated with the Alloca Inst
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  virtual bool instrumentAllocaInst(LoopInfo& LI, AllocaInst& Alloca, Value& V) {
    return false; // DEFAULT: do nothing
  }

  /// @brief If alloca inst's are instrumented than you will likely need to compensate for it at each
  ///        return inst to preserve memory safety.
  ///        By default this Fn does nothing and should be overriden if the \c instrumentAllocaInst
  ///        is overriden in your implementing class.
  ///        ( \b NOTE: this function is only called once per fn in module, not per
  ///            ret inst like the other instrument fn's.
  ///            This is because you will need to deal with phi shenanigans which is
  ///            just easier done all at once. )
  /// @param LI The loop info / analysis for the parent fn.
  /// @param Allocas List/vector of all alloca's in the parent fn.
  /// @param Returns List/vector of all return instructions in the parent fn.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  virtual bool instrumentReturns(LoopInfo& LI, SmallVector<std::pair<AllocaInst*, Value*>>& Allocas,
                                 SmallVector<ReturnInst*>& Returns) {
    return false; // DEFAULT: do nothing
  }

  /// @brief Instrument a device function Implementation.
  ///         ( \b NOTE: always call \c run instead of \c runImpl so that
  ///           prerequisite work can be done before you do your instrumentation,
  ///           in this case the \c isInstrumentableFn check.)
  /// @param F the function who's body will be instrumented
  /// @param FAM the analysis manager for above function
  /// @return \c bool - if the Function was changed at all
  virtual bool runImpl(Function& F, FunctionAnalysisManager& FAM);

  /// @brief Instrument a device function for scabbard.
  /// @param F the function who's body will be instrumented
  /// @param FAM the analysis manager for above function
  /// @return \c bool - if the Function was changed at all
  virtual bool run(Function& F, FunctionAnalysisManager& FAM) final {
    // prevent instrumenting functions that should not be instrumented
    if (not isInstrumentableFn(F))
      return;
    // TODO any prereq work...
    // run the actual implementation that might be modified in inherited classes
    runImpl(F, FAM);
  }

  /// @brief Instrument a device function Implementation.
  ///         ( \b NOTE: always call \c run instead of \c runImpl so that
  ///           prerequisite work can be done before you do your instrumentation.)
  /// @param M the module who's globals and functions will need to be instrumented.
  /// @param MAM the analysis manager for above module.
  /// @return \c bool - if the Module was changed at all.
  virtual bool runImpl(Module& M, ModuleAnalysisManager& MAM) { return false; }

public:
  /// @brief Instrument a device module for scabbard.
  ///        Can't be overriden, override \c runImpl to modify instrumentation.
  /// @param M the module who's contents will be instrumented
  /// @param MAM the analysis manager for above module
  /// @return \c bool - if the Module was changed at all
  virtual bool run(Module& M, ModuleAnalysisManager& MAM) final {
    bool changed = false;
    // determine if any local allocations are made in this module
    HasAllocas = [&]() {
      for (const Function& Fn : M)
        for (const auto& I : instructions(Fn))
          if (isa<AllocaInst>(I))
            return true;
      return false;
    }();
    // run any device arch specific tasks before moving on.
    changed |= runImpl(M, MAM);
    registerRTL(M);
    // initiate the class members based on the contents of the module
    // add JobState ptrs to the end of all non-intrinsic functions.
    // TODO handle conversions of globals
    // TODO any additional prereq work...
    // run the actual implementation that might be modified in inherited classes
    changed |= expandFnParams(M);
    // create and insert the ctor and dtor

    FunctionAnalysisManager& FAM = MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
    for (Function& F : M.functions())
      changed |= run(F, FAM);

    if (changed) {                                 // if any instrumentation occurred insert the ctor and dtor
      Function* CtorFn = this->createCTor(M, MAM); // defined by implementing class
      CtorFn->addFnAttr(Attribute::DisableSanitizerInstrumentation);
      appendToGlobalCtors(M, CtorFn, 0, nullptr);
      Function* DtorFn = this->createDTor(M, MAM); // defined by implementing class
      DtorFn->addFnAttr(Attribute::DisableSanitizerInstrumentation);
      appendToGlobalDtors(M, CtorFn, 0, nullptr);
    }
    // TODO handle ambagious calls

    // if it appears as though instrumentation occurred output the metadata object for the module
    if (changed)
      scabbard.Metadata.outputMetadata(M);

    return changed;
  }
};

const std::unordered_set<std::string> scabbardIDevicePass::NO_INSTR_FNS{
    "__ockl_hostcall_internal", "__ockl_dm_alloc", "__cxa_pure_virtual",
    "__cxa_deleted_virtual",    "__assertfail",    "__assert_fail"};

// << ========================================================================================== >>
// <<                                AMD DEVICE INSTR IMPL PASS                                  >>
// << ========================================================================================== >>

/// @brief Instrumentation pass for AMD GPUs.
///        Assuming code was written for a ROCm compatible GPU using ROCm toolchains.
///        Code can be written in any compatible language for ROCm.
///
///         Inherits most of it's functionality form it's parent interface \ref scabbardIDevicePass
///
class scabbardAMDDevicePass final : public scabbardIDevicePass {
  /// @brief the integer associated with the shared address space on AMD GPUs.
  const uint64_t SHARED_ADDRESS_SPACE = 3ul;

public:
  scabbardAMDDevicePass(const Module& M) : scabbardIDevicePass(M) {}
  scabbardAMDDevicePass() = delete;

protected:
  /// @brief Return if the address-space of a global variable is in the
  ///        address space specified by the device vendor as shared memory.
  /// @param G the global variable.
  /// @return \c bool - if \c G is in the shared address space.
  bool isSharedGlobal(const GlobalVariable& G) const override { return G.getAddressSpace() == SHARED_ADDRESS_SPACE; }

  /// @brief Create scabbard's module Constructor (ctor) function for the target arch.
  ///         Return the function and the calling class will insert it into
  ///         the appropriate global object to register it as a ctor.
  /// @param M the module to create the ctor in and for.
  /// @param MAM the analysis manager for above module.
  /// @return \ref llvm::Function \c* - ptr to the ctor fn.
  Function* createCTor(Module& M, ModuleAnalysisManager& MAM, GlobalVariable* MetadataVar) const override {}

  /// @brief Create scabbard's module deconstructor (dtor) function for the target arch.
  ///         Return the function and the calling class will insert it into
  ///         the appropriate global object to register it as a dtor.
  /// @param M the module to create the dtor in and for.
  /// @param MAM the analysis manager for above module
  /// @return \ref llvm::Function \c* - ptr to the dtor fn.
  Function* createDTor(Module& M, ModuleAnalysisManager& MAM) const override {}


  /// @brief When a load instruction is found in a fn this is called.
  ///        It will decide if the Fn is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param Load the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  bool instrumentLoadInst(LoopInfo& LI, LoadInst& Load) override {
    return instrumentInScabbardFunc(LI, Load, Load.getPointerOperand(), 
                                    scabbard.trace_append$mem, 
                                    InstrData::ON_GPU | InstrData::READ | (Load.isAtomic() ? InstrData::ATOMIC : InstrData::NO));
  }

  /// @brief When a store instruction is found in a fn this is called.
  ///        It will decide if the Fn is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param Store the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  bool instrumentStoreInst(LoopInfo& LI, StoreInst& Store) override {
    return instrumentInScabbardFunc(LI, Store, Store.getPointerOperand(), 
                                    scabbard.trace_append$mem, 
                                    InstrData::ON_GPU | InstrData::WRITE | (Store.isAtomic() ? InstrData::ATOMIC : InstrData::NO));
  }

  /// @brief When a atomicrmw instruction is found in a fn this is called.
  ///        It will decide if the Fn is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param RMW the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  bool instrumentAtomicRMWInst(LoopInfo& LI, AtomicRMWInst& RMW) override {
    return instrumentInScabbardFunc(LI, RMW, RMW.getPointerOperand(), 
                                    scabbard.trace_append$mem, 
                                    InstrData::ON_GPU | InstrData::READ | InstrData::WRITE | InstrData::ATOMIC);
  }

  /// @brief When a cmpxchg instruction is found in a fn this is called.
  ///        It will decide if the Fn is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param CXC the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  bool instrumentCmpXChgInst(LoopInfo& LI, AtomicCmpXchgInst& CXC) override {
    return instrumentInScabbardFunc(LI, CXC, CXC.getPointerOperand(), 
                                    scabbard.trace_append$mem, 
                                    InstrData::ON_GPU | InstrData::WRITE | InstrData::ATOMIC);
  }

  /// @brief When a fence instruction is found in a fn this is called.
  ///        It will decide if the Fn is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param Fence the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  bool instrumentFenceInst(LoopInfo& LI, FenceInst& Fence) override {}

  /// @brief When a call instruction is found in a fn this is called.
  ///        It will decide if the Fn is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param Call the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  bool instrumentCallInst(LoopInfo& LI, CallInst& Call) override {
    //TODO figure out what calls to instrument
  }
};


// << ========================================================================================== >>
// <<                                     EXTRA DEFINITIONS                                      >>
// << ========================================================================================== >>


inline Twine IScabbardInstrPass::getLocStr(const Instruction& I) {
  const DILocation& DLoc = *I.getDebugLoc();
  return Twine("\"")
        + DLoc.getDirectory() + "/" + DLoc.getFilename() 
        + "\":" 
        + (Twine) DLoc.getLine()
        + ","
        + (Twine) DLoc.getColumn();
}

// << ================================= HOST PASS DEFINITIONS ================================== >> 

void IScabbardHostPass::registerRTL(Module& M) {
  if (M.getFunction("main") != nullptr)
    scabbard.scabbard_init = M.getOrInsertFunction(
        scabbard.scabbard_init_name,
        FunctionType::get(
            VoidTy,
            {}, // std::array<VoidTyType*,1ull>{VoidTy},
            false
          )
      );
  // host.scabbard_close = M.getOrInsertFunction(
  //     host.scabbard_close_name,
  //     FunctionType::get(
  //         VoidTy,
  //         {}, // std::array<VoidTyType*,1ull>{VoidTy},
  //         false
  //       )
  //   );
  scabbard.trace_append$mem = M.getOrInsertFunction(
      scabbard.trace_append$mem_name,
      FunctionType::get(
          VoidTy,
          std::array<Type*,3ull>{
              TraceDataTy,
              PtrTy, //WARN: This constant 0u might need to be dynamicly decided for host modules
              LocDataTy
            },
          false
        )
    );
  scabbard.trace_append$mem$cond = M.getOrInsertFunction(
      scabbard.trace_append$mem$cond_name,
      FunctionType::get(
          VoidTy,
          std::array<Type*,3ull>{
              TraceDataTy,
              PtrTy, //WARN: This constant 0u might need to be dynamicly decided for host modules
              LocDataTy
            },
          false
        )
    );
  scabbard.trace_append$alloc = M.getOrInsertFunction(
      scabbard.trace_append$alloc_name,
      FunctionType::get(
          VoidTy,
          std::array<Type*,4ull>{
              TraceDataTy,
              PtrTy, //WARN: This constant 0u might need to be dynamicly decided for host modules
              LocDataTy,
              ExtraDataTy
            },
          false
        )
    );
  scabbard.register_job = M.getOrInsertFunction(
      scabbard.register_job_name,
      FunctionType::get(
          PtrTy,
          std::array<Type*,1ull>{PtrTy},
          false
        )
    );
  scabbard.register_job_callback = M.getOrInsertFunction(
      scabbard.register_job_callback_name,
      FunctionType::get(
          VoidTy,
          std::array<Type*,3ull>{
              PtrTy, PtrTy, LocDataTy
            },
          false
        )
    );
}

inline void scabbardHostPass::registerGlobalVarsInUnifiedMemory(const Module& M) {
  auto get_next = [=](const llvm::Value* V) -> const llvm::Value* {
    if (const auto* CE = llvm::dyn_cast_or_null<llvm::ConstantExpr>(V)) {
      for (const auto& U : CE->operands()) {
        const llvm::Value* res = get_next(U.get());
        if (res != nullptr)
          return res;
      }
    } else if (const auto* GV = llvm::dyn_cast_or_null<llvm::GlobalValue>(V)) {
      return GV;
    }
    return nullptr;
  };
  auto checkFn = [&](const Function* Fn, const scabbard::InstrData HeapLoc) -> void {
    for (const auto u : Fn->users())
      if (const auto* call = llvm::dyn_cast_or_null<llvm::CallInst>(u.get())) {
        if (const auto* global = llvm::dyn_cast_or_null<llvm::GlobalVariable>(get_next(call->getArgOperand(1)))) {
          this->GlobalUnifiedMemVar.insert(std::make_pair(global->getName(), HeapLoc));
        }
      }
  };

  if (const auto* hFn = dyn_cast<Function>(M.getFunction("hipHostRegister"))) {
    checkFn(hFn, HOST_HEAP);
  }
  if (const auto* hFn = dyn_cast<Function>(M.getFunction("hipHostMalloc"))) {
    checkFn(hFn, HOST_HEAP);
  }
  if (const auto* hFn = dyn_cast<Function>(M.getFunction("hipHostAlloc"))) {
    checkFn(hFn, HOST_HEAP);
  }
  if (const auto* dFn = dyn_cast<Function>(M.getFunction("__hipRegisterVar"))) {
    checkFn(hFn, DEVICE_HEAP);
  }
  if (const auto* mFn = dyn_cast<Function>(M.getFunction("__hipRegisterManagedVar"))) {
    checkFn(hFn, MANAGED_MEM);
  }
} 
  
bool IScabbardHostPass::runImpl(Function& F, FunctionAnalysisManager& FAM) {
  bool changed = false;
  LoopInfo& LI = FAM.getResult<LoopAnalysis>(F);
  // SmallVector<std::pair<AllocaInst*, Value*>> Allocas; // used for fakPtr/ShadowMem impl (not cur impl)
  // SmallVector<ReturnInst*> Returns;    // used for fakPtr/ShadowMem impl (not cur impl)
  SmallVector<LoadInst*> Loads;
  SmallVector<StoreInst*> Stores;
  // SmallVector<CallInst*> Calls;          // moved to separate processing step that does not require searching.
  // SmallVector<GetElementPtrInst*> GEPs;  // used for fakPtr/ShadowMem impl (not cur impl)
  SmallVector<AtomicRMWInst*> AtomicRMWs;
  SmallVector<AtomicCmpXchgInst*> CmpXchgs;
  // SmallVector<IntToPtrInst*> Int2Ptrs;  // used for fakPtr/ShadowMem impl (not cur impl)
  // SmallVector<PtrToIntInst*> Ptr2Ints; // used for fakPtr/ShadowMem impl (not cur impl)

  for (auto& I : instructions(F)) {
    switch (I.getOpcode()) {
    // case Instruction::Alloca: {
    //   AllocaInst& AI = cast<AllocaInst>(I);
    //   Allocas.push_back({&AI, nullptr});
    //   break;
    // }
    case Instruction::Load:
      Loads.push_back(&cast<LoadInst>(I));
      break;
    case Instruction::Store:
      Stores.push_back(&cast<StoreInst>(I));
      break;
    // case Instruction::GetElementPtr:
    //   GEPs.push_back(&cast<GetElementPtrInst>(I));
    //   changed = true;
    //   break;
    // case Instruction::Call: {
    //   auto& CI = cast<CallInst>(I);
    //   Calls.push_back(&CI);
    //   if (CI.isIndirectCall())
    //     AmbiguousCalls.insert(&CI);
    //   break;
    // }
    // case Instruction::Ret:
    //   Returns.push_back(&cast<ReturnInst>(I));
    //   break;
    case Instruction::AtomicRMW:
      AtomicRMWs.push_back(&cast<AtomicRMWInst>(I));
      break;
    case Instruction::AtomicCmpXchg:
      // TODO handle cmpxchg instructions
      CmpXchgs.push_back(&cast<AtomicCmpXchgInst>(I));
      break;
    // case Instruction::IntToPtr:
    //   Int2Ptrs.push_back(&cast<IntToPtrInst>(I));
    //   break;
    // case Instruction::PtrToInt:
    //   Ptr2Ints.push_back(&cast<PtrToIntInst>(I));
    //   break;
    default:
      break;
    }
  }

  // for (auto* P2I : Ptr2Ints)
  //   changed |= instrumentPtrToIntInst(LI, *P2I);
  // for (auto* I2P : Int2Ptrs)
  //   changed |= instrumentIntToPtrInst(LI, *I2P);
  for (auto* Load : Loads)
    changed |= instrumentLoadInst(LI, *Load);
  for (auto* Store : Stores)
    changed |= instrumentStoreInst(LI, *Store);
  for (auto* RMW : AtomicRMWs)
    changed |= instrumentAtomicRMWInst(LI, *RMW);
  for (auto* CMP : CmpXchgs)
    changed |= instrumentCmpXChgInst(LI, *CMP);
  // for (auto* Fence : Fences)
  //   changed |= instrumentFenceInst(LI, *Fence);
  // for (auto* GEP : GEPs)
  //   instrumentGEPInst(LI, *GEP);
  // for (auto* Call : Calls)
  //   changed |= instrumentCallInst(LI, *Call);
  // for (auto& It : Allocas)
  //   It.second = instrumentAllocaInst(LI, *It.first);

  // changed |= instrumentReturns(LI, Allocas, Returns);

  return changed;
}


namespace HostPtrOriginHelpers {
using namespace llvm;
using PtrOrigin = IScabbardInstrPass::PtrOrigin;

const Value* throughConstExpr(const Value* V) {
  if (const auto* CE = dyn_cast_or_null<ConstantExpr>(V)) {
    for (const auto& U : CE->operands()) {
      const llvm::Value* res = throughConstExpr(U.get());
      if (res != nullptr)
        return res;
    }
  } else if (const auto* I = dyn_cast_or_null<Instruction>(V)) {
    return I;
  }
  return nullptr;
}

// typedef std::function<scabbard::InstrData(const Value&,const CallInst&)> CallCheck_t;
using CallCheck_t = std::function<PtrOrigin(const Value&,const CallInst&)>;

CallCheck_t BASE_CHECK = [&](const Value& V, const CallInst& C) -> PtrOrigin {
  return (((&V) == throughConstExpr(C.getArgOperand(0))) ? PtrOrigin::UNKNOWN_HEAP : PtrOrigin::NONE); // compare ptr's and hope llvm does not make copies of IR objects
};

CallCheck_t ALWAYS_HOST = [=](const Value&, const CallInst&) -> PtrOrigin { return PtrOrigin::HOST_HEAP; };
CallCheck_t ALWAYS_DEVICE = [=](const Value&, const CallInst&) -> PtrOrigin { return PtrOrigin::DEVICE_HEAP; };
CallCheck_t ALWAYS_MANAGED = [=](const Value&, const CallInst&) -> PtrOrigin { return PtrOrigin::MANAGED_MEM; };
CallCheck_t ALWAYS_LOCAL = [=](const Value&, const CallInst&) -> PtrOrigin { return PtrOrigin::LOCAL; };
CallCheck_t ALWAYS_UNKNOWN_HEAP = [=](const Value&, const CallInst&) -> PtrOrigin { return PtrOrigin::UNKNOWN_HEAP; };


const StringMap<CallCheck_t> funcsOfInterest {
    { "hipMalloc", ALWAYS_DEVICE },
    {
      "hipMemcpy", 
      [&](const Value& V, const CallInst& C) -> PtrOrigin {
        using namespace llvm;
        const Value* _V = &V;
        if (auto* TrTy = dyn_cast<ConstantInt>(C.getArgOperand(3)))
          // get which 
          switch (TrTy->getSExtValue()) {
            case 1: // H->D
              return (throughConstExpr(C.getArgOperand(0)) == _V) ? PtrOrigin::DEVICE_HEAP : PtrOrigin::LOCAL;
            case 2: // D->H
              return (throughConstExpr(C.getArgOperand(1)) == _V) ? PtrOrigin::DEVICE_HEAP : PtrOrigin::LOCAL;
            case 3: // D->D
              return PtrOrigin::DEVICE_HEAP;
            case 0: // H->H
              return PtrOrigin::UNKNOWN_HEAP;
            default: // Unknown
              errs() << "\n\n[scabbard.instr.host:ERROR] `hipMemcpy`'s `hipMemcpyKind` argument was not an expected value! "
                        "("<< IScabbardHostPass::getLocStr(C) <<")\n\n";
          }
        errs() << "\n\n[scabbard.instr.host:ERROR] `hipMemcpy`'s `hipMemcpyKind` argument was not a constant value!"
                  "("<< IScabbardHostPass::getLocStr(C) <<")\n\n";
        LLVM_BUILTIN_UNREACHABLE;
      }
    },
    { "hipHostRegister", ALWAYS_HOST },
    { "hipHostMalloc", ALWAYS_HOST },
    { "hipHostAlloc", ALWAYS_HOST },
    { "hipMallocManaged", ALWAYS_MANAGED },
    { "hipMemAdvise", BASE_CHECK },
  };
} //? namespace HostPtrOriginHelpers

inline IScabbardInstrPass::PtrOrigin scabbardHostPass::getPtrOrigin(LoopInfo& LI, Value* Ptr, const Value** Object) const {
  // derived from
  // https://github.com/jdoerfert/llvm-project/blob/b416d0c996bc01aeb6708c715bfe5e53bcac998d/llvm/lib/Transforms/Instrumentation/GPUSan.cpp#L592
  using namespace HostPtrOriginHelpers;
  SmallVector<const Value*> Objects;
  getUnderlyingObjects(Ptr, Objects, &LI);
  if (Object && Objects.size() == 1)
    *Object = Objects.front();
  PtrOrigin PO = NONE;
  for (auto* Obj : Objects) {
    PtrOrigin ObjPO = NONE;
    switch (Obj->getValueID()) {
      case Value::GlobalVariableVal: {
        GlobalVariable* GV = (GlobalVariable*) Obj;
        auto res = GlobalUnifiedMemVar.find(GV->getName());
        ObjPO = ((res != GlobalUnifiedMemVar.end()) ? res.second : UNKNOWN_HEAP); 
        //TODO remove globals not known to be on device or managed
        break;
      }
      case Value::ArgumentVal:
        ObjPO = UNKNOWN_HEAP;
        break;
      case Instruction::Load + Value::InstructionVal: {
        LoadInst* Load = (LoadInst*) Obj;
        if (auto* Global = dyn_cast<GlobalVariable>(Load->getPointerOperand())) {
          if (Global->getName().ends_with(".device") 
              || _M.getGlobalVariable(Global->getName().str()+".device"))
            ObjPO = DEVICE_HEAP;
          else if (Global->getName().ends_with(".managed") 
              || _M.getGlobalVariable(Global->getName().str()+".managed"))
            ObjPO = MANAGED_MEM;
        }
        break;
      }
      case Instruction::Call + Value::InstructionVal: {
        CallInst* CI = (CallInst*) Obj;
        if (auto* Callee = CI->getCalledFunction())
        if (Callee->getName().starts_with("ompx_")) {
          if (Callee->getName().ends_with("_global"))
            ObjPO = DEVICE_HEAP;
          else if (Callee->getName().ends_with("_local"))
            ObjPO = LOCAL;
          else if (Callee->getName().ends_with("_shared"))
            ObjPO = MANAGED_MEM;
        }
        break;
      }
      case Instruction::Alloca + Value::InstructionVal: {
        AllocaInst* AI = (AllocaInst*) Obj;
        //check if this is used in a hipMalloc
        PtrOrigin PosPO = NONE;
        for (const auto& U : AI.uses())
          if (const auto* CI = llvm::dyn_cast_or_null<llvm::CallInst>(U.getUser())) {
            auto p = funcsOfInterest.find(CI->getCalledFunction()->getName().str());
            if (p != funcsOfInterest.end() && (auto res = p->second(I,*CI)))
              PosPO = res;
          }
        if (PosPO == NONE)
          return LOCAL;
        ObjPO = PosPO;
        break;
      }
      default:
        break;
    }
    if (PO == NONE || (PO == UNKNOWN_HEAP && ObjPO > UNKNOWN_HEAP))
      PO = ObjPO;
  }
  return PO;
}

bool scabbardHostPass::instrumentInScabbardFunc(LoopInfo& LI, Instruction& I, Value* Ptr, 
                                                   const InstrData InstrContext, const Value* ExtraData) const {
  Value* PtrOp = Ptr;
  const Value *Object = nullptr;
  PtrOrigin PO = getPtrOrigin(LI, PtrOp, &Object);

  if (PO > NO) // don't instrument if it is Known to be not in Unified Memory.
    return false;

  if (PO == UNKNOWN_HEAP) // mark memory to be determined at runtime if it is in Unified Memory
    PO |= _RUNTIME_CONDITIONAL;

  auto [locID, is_inserted] = scabbard.Metadata.insert(&I);

  if (not is_inserted && locID == 0ull)
    errs() << "\n[scabbard.instr.host.metadata: ERROR] failed to insert instruction into the metadata system!\n";

  if (ExtraData == nullptr)
    auto _ = CallInst::Create(
        scabbard.trace_append$mem,
        std::array<Value*, 3ull>{
            ConstantInt::get(TraceDataTy, APInt(sizeof(InstrData)*8, InstrContext | PO)),
            Ptr,
            ConstantInt::get(LocDataTy, APInt(sizeof(size_t)*8, locID))
          },
        "scabbard." + I.getName(), 
        /* insertBefore= */ &I
      );
  else 
    auto _ = CallInst::Create(
        scabbard.trace_append$alloc,
        std::array<Value*, 4ull>{
            ConstantInt::get(TraceDataTy, APInt(sizeof(InstrData)*8, InstrContext | PO)),
            Ptr,
            ConstantInt::get(LocDataTy, APInt(sizeof(size_t)*8, locID)),
            ExtraData
          },
        "scabbard." + I.getName(), 
        /* insertBefore= */ &I
      );

  
  return true;
}

CallInst* ScabbardHostPassHip::CreateRTLCall(const CallInst& CI, scabbard::InstrData Data, 
                                            const Value* Ptr, bool InsertBefore) const {
  auto [locID, is_inserted] = scabbard.Metadata.insert(CI);
  if (not is_inserted && locID == 0ull)
    errs() << "\n[scabbard.instr.host.metadata.amdhip: ERROR] failed to insert instruction into the metadata system!\n";
  auto ci = CallInst::Create(
        scabbard.trace_append$mem,
        std::array<Value*, 3ull>{
            ConstantInt::get(TraceDataTy, APInt(sizeof(InstrData)*8, 
                              InstrData::ON_HOST | Data)),
            Ptr,
            ConstantInt::get(LocDataTy, APInt(sizeof(size_t)*8, locID))
          },
        Twine("scabbard.") + (InsertBefore ? "pre." : "post.") + CI.getName()
      );
  if (InsertBefore)
    ci->insertBefore(&CI);
  else
    ci->insertAfter(&CI);
  ci->setDebugLoc(CI.getDebugLoc());
  return ci;
}

CallInst* ScabbardHostPassHip::CreateRTLCallEx(const CallInst& CI, scabbard::InstrData Data, const Value* Ptr, 
                                              const Value* Extra, bool InsertBefore) const {
  auto [locID, is_inserted] = scabbard.Metadata.insert(CI);
  if (not is_inserted && locID == 0ull) {
    errs() << "\n[scabbard.instr.host.metadata.amdhip: ERROR] failed to insert instruction into the metadata system!\n";
    return nullptr;
  }
  auto ci = CallInst::Create(
        scabbard.trace_append$alloc,
        std::array<Value*, 4ull>{
            ConstantInt::get(TraceDataTy, APInt(sizeof(InstrData)*8, 
                              InstrData::ON_HOST | Data | InstrData::_OPT_USED)),
            Ptr,
            ConstantInt::get(LocDataTy, APInt(sizeof(size_t)*8, locID))
          },
        Twine("scabbard.") + (InsertBefore ? "pre." : "post.") + CI.getName()
      );
  if (ci == nullptr) return nullptr;
  if (InsertBefore)
    ci->insertBefore(&CI);
  else
    ci->insertAfter(&CI);
  ci->setDebugLoc(CI.getDebugLoc());
  return ci;
}


/// @brief Get the value that contains the mem address of the pointer pointer
///        (used to get the mem address of the ptr value of hipMalloc)
///       Not safe to use outside of specific use
const Value* get_ptr_from_ptr(const Value* V) {
  switch (V->getValueID()) {
  case Instruction::Alloca + Value::InstructionVal:
    return V;
  case Value::ArgumentVal:
    return (V->getType()->isPointerTy()) ? V : nullptr;
  case Value::ConstantExpr: {
      const ConstExpr* CE = (ConstExpr*) V;
      switch (CE->getOpcode()) {
      case Instruction::BitCast:
        return get_ptr_from_ptr(CE->getOperand(0ull));
      default:
        return nullptr;
      }
    }
  case Instruction::BitCast + Value::InstructionVal: {
      const BitCastInst* bci = (BitCastInst*) V;
      return get_ptr_from_ptr(bci->getOperand(0ull));
    }
  default:
    return nullptr;
  }
  errs() << "\n[scabbard.host.amdhip:ERR] unreachable section `get_ptr_from_ptr()`\n";
}

bool ScabbardHostPassHip::APIInstr_Alloc(CallInst&, FunctionAnalysisManager& FAM, InstrData Data) const {
  if (auto ptr = get_ptr_from_ptr(_CI->getArgOperand(0ull))) {
    return CreateRTLCallEx(*_CI, InstrData::ON_HOST | Data,
                            ptr, _CI->getArgOperand(1ull), false) ? true : false;
  }
  errs() << "\n[scabbard.instr.host.amdhip:ERR] could not backtrack `hipMalloc` ptr parameter (`" 
          << _CI->getArgOperand(0ull) << "`)\n";  
  return false;
}

void ScabbardHostPassHip::registerAPIInstrumenters() {
  APIInstrumenters = SmallVector<std::pair<const StringRef,APIInstrumenter_t>,24ull>{
    {
      "hipStreamSynchronize", 
      [this](auto _CI, auto FAM) -> bool { 
        return CreateRTLCall(*_CI, InstrData::SYNC_EVENT, 
                             _CI->getArgOperand(0ull), false) ? true : false;
      }
    },
    {
      "hipDeviceSynchronize", 
      [this](auto _CI, auto FAM) -> bool { 
        return CreateRTLCall(*_CI, InstrData::SYNC_EVENT,
                             ConstantPointerNull::get(PtrTy), false) ? true : false;
      }
    },
    {
      "hipMalloc", 
      [this](auto _CI, auto FAM) -> bool { return APIInstr_Alloc(*_CI,FAM,ALLOCATE|DEVICE_HEAP); }
    },
    {
      "hipExtMallocWithFlags", 
      [this](auto _CI, auto FAM) -> bool { return APIInstr_Alloc(*_CI,FAM,ALLOCATE|DEVICE_HEAP); }
    },
    {
      "hipHostAlloc", 
      [this](auto _CI, auto FAM) -> bool { return APIInstr_Alloc(*_CI,FAM,ALLOCATE|HOST_HEAP); }
    },
    {
      "hipHostMalloc", 
      [this](auto _CI, auto FAM) -> bool { return APIInstr_Alloc(*_CI,FAM,ALLOCATE|HOST_HEAP); }
    },
    {
      "hipMallocManaged", 
      [this](auto _CI, auto FAM) -> bool { return APIInstr_Alloc(*_CI,FAM,ALLOCATE|MANAGED_MEM); }
    },
    {
      "hipHostRegister", 
      [this](auto _CI, auto FAM) -> bool {
        return CreateRTLCallEx(*_CI, InstrData::ALLOCATE | InstrData::HOST_HEAP,
                              _CI->getArgOperand(0ull), _CI->getArgOperand(1ull), false) ? true : false;
      }
    },
    {
      "hipFree",
      [this](auto _CI, auto FAM) -> bool {
        return CreateRTLCall(*_CI, InstrData::FREE_EVENT | InstrData::UNKNOWN_HEAP,
                              _CI->getArgOperand(0ull), true) ? true : false;
      }
    },
    {
      "hipFreeHost",
      [this](auto _CI, auto FAM) -> bool {
        return CreateRTLCall(*_CI, InstrData::FREE_EVENT | InstrData::HOST_HEAP,
                              _CI->getArgOperand(0ull), true) ? true : false;
      }
    },
  };
}


// << ================================== DEVICE HELPER CODE ==================================== >> 

void ScabbardIDevicePass::registerRTL(Module& M) {
  scabbard.trace_append$mem = M.getOrInsertFunction(
      scabbard.trace_append$mem_name,
      FunctionType::get(
          VoidTy,
          std::array<Type*,4ull>{
              PtrTy,
              TraceDataTy,
              PtrTy, //WARN: This constant 0u might need to be dynamicly decided for host modules
              LocDataTy
            },
          false
        )
    );
  scabbard.trace_append$alloc = M.getOrInsertFunction(
      scabbard.trace_append$alloc_name,
      FunctionType::get(
          VoidTy,
          std::array<Type*,5ull>{
              PtrTy,
              TraceDataTy,
              PtrTy,
              LocDataTy,
              ExtraDataTy
            }),
          false
        )
    );
}

bool scabbardIDevicePass::runImpl(Function& F, FunctionAnalysisManager& FAM) {
  bool changed = false;
  LoopInfo& LI = FAM.getResult<LoopAnalysis>(F);
  // SmallVector<std::pair<AllocaInst*, Value*>> Allocas; // for fakPtr/ShadowMem impl (not cur impl)
  // SmallVector<ReturnInst*> Returns; // for fakPtr/ShadowMem impl (not cur impl)
  SmallVector<LoadInst*> Loads;
  SmallVector<StoreInst*> Stores;
  // SmallVector<CallInst*> Calls;  // not currently used on the device
  // SmallVector<GetElementPtrInst*> GEPs; // for fakPtr/ShadowMem impl (not cur impl)
  SmallVector<AtomicRMWInst*> AtomicRMWs;
  SmallVector<AtomicCmpXchgInst*> CmpXchgs;
  // SmallVector<FenceInst*> Fences;  // for device side data race check (not cur impl)
  // SmallVector<IntToPtrInst*> Int2Ptrs; // for fakPtr/ShadowMem impl (not cur impl)
  // SmallVector<PtrToIntInst*> Ptr2Ints; // for fakPtr/ShadowMem impl (not cur impl)

  for (auto& I : instructions(F)) {
    switch (I.getOpcode()) {
    // case Instruction::Alloca: {
    //   AllocaInst& AI = cast<AllocaInst>(I);
    //   Allocas.push_back({&AI, nullptr});
    //   break;
    // }
    case Instruction::Load:
      Loads.push_back(&cast<LoadInst>(I));
      break;
    case Instruction::Store:
      Stores.push_back(&cast<StoreInst>(I));
      break;
    // case Instruction::GetElementPtr:
    //   GEPs.push_back(&cast<GetElementPtrInst>(I));
    //   changed = true;
    //   break;
    // case Instruction::Call: {
    //   auto& CI = cast<CallInst>(I);
    //   Calls.push_back(&CI);
    //   if (CI.isIndirectCall())
    //     AmbiguousCalls.insert(&CI);
    //   break;
    // }
    // case Instruction::Ret:
    //   Returns.push_back(&cast<ReturnInst>(I));
    //   break;
    case Instruction::AtomicRMW:
      AtomicRMWs.push_back(&cast<AtomicRMWInst>(I));
      break;
    case Instruction::AtomicCmpXchg:
      // TODO handle cmpxchg instructions
      CmpXchgs.push_back(&cast<AtomicCmpXchgInst>(I));
      break;
    // case Instruction::Fence: {
    //   auto& fence = cast<FenceInst>(I);
    //   if (fence.getSyncScopeID() ==
    //       /* TODO figure our what scopes are of interest and how they map to block and warp */)
    //     Fences.push_back(&fence);
    //   break;
    // }
    // case Instruction::IntToPtr:
    //   Int2Ptrs.push_back(&cast<IntToPtrInst>(I));
    //   break;
    // case Instruction::PtrToInt:
    //   Ptr2Ints.push_back(&cast<PtrToIntInst>(I));
    //   break;
    default:
      break;
    }
  }

  for (auto* P2I : Ptr2Ints)
    changed |= instrumentPtrToIntInst(LI, *P2I);
  for (auto* I2P : Int2Ptrs)
    changed |= instrumentIntToPtrInst(LI, *I2P);
  for (auto* Load : Loads)
    changed |= instrumentLoadInst(LI, *Load);
  for (auto* Store : Stores)
    changed |= instrumentStoreInst(LI, *Store);
  for (auto* RMW : AtomicRMWs)
    changed |= instrumentAtomicRMWInst(LI, *RMW);
  for (auto* CMP : CmpXchgs)
    changed |= instrumentCmpXChgInst(LI, *CMP);
  // for (auto* Fence : Fences)
  //   changed |= instrumentFenceInst(LI, *Fence);
  // for (auto* GEP : GEPs)
  //   instrumentGEPInst(LI, *GEP);
  // for (auto* Call : Calls)
  //   changed |= instrumentCallInst(LI, *Call);
  // for (auto& It : Allocas)
  //   It.second = instrumentAllocaInst(LI, *It.first);

  // changed |= instrumentReturns(LI, Allocas, Returns);

  return changed;
}

inline bool scabbardIDevicePass::expandFnParams(Module& M) const {
  // expand the signatures of all relevant fn's
  // (this requires cloning and RAUW since llvm ir fn sigs are immutable)
  bool changed = false;
  SmallVector<std::pair<Function*, Function*>, 8ul> to_replace;
  for (auto& OldFn : M.functions()) {
    if (not isInstrumentableFn(OldFn)) // skip fn's that shouldn't be instrumented
      continue;
    std::string old_name = OldFn.getName().str();
    OldFn.setName(old_name + "__old__scabbard_instr_replaced__old__");
    auto oldParamTys = OldFn.getFunctionType()->params();
    std::vector<Type*> paramTys(oldParamTys.begin(), oldParamTys.end());
    paramTys.push_back((Type*)PtrTy);
    auto fn_callee =
        M.getOrInsertFunction(old_name,
                              FunctionType::get(OldFn.getFunctionType()->getReturnType(), ArrayRef<Type*>(paramTys),
                                                OldFn.getFunctionType()->isVarArg()),
                              OldFn.getAttributes());
    Function* NewFn = dyn_cast<Function>(fn_callee.getCallee()); // new function (OldFn is old function)
    NewFn->setCallingConv(OldFn.getCallingConv());
    NewFn->setLinkage(OldFn.getLinkage());

    to_replace.push_back(std::tuple(&OldFn, NewFn));
    changed |= true;
    // if (OldFn.isDeclaration())
    //   return NewFn;

    ValueToValueMapTy vMap;
    for (size_t i = 0; i < OldFn.arg_size(); ++i)
      vMap[OldFn.getArg(i)] = NewFn->getArg(i);
    SmallVector<ReturnInst*, 8> rets;
    //?NOTE: this might be used wrong.  Double check results in testing to make sure it works correctly
    //?     if wrong likely due to not creating vMap properly
    CloneFunctionInto(NewFn, &OldFn, vMap, CloneFunctionChangeType::LocalChangesOnly, rets);
    // provide metadata for the added argument
    // if (NewFn->isDeclaration()) return NewFn; //skip this for external functions
    // auto* subPMD = NewFn->getSubprogram();
    // auto retMDNs = subPMD->getRetainedNodes();
    // std::vector<Metadata*> arg_metadata;
    // for (auto arg : retMDNs) arg_metadata.push_back(arg);
    // arg_metadata.push_back(
    //     DILocalVariable::get(
    //       NewFn->getContext(),
    //       subPMD,
    //       MDString::get(NewFn->getContext(), "SCABBARD_DT"),
    //       subPMD->getFile(),
    //       subPMD->getLine(),
    //       device.DeviceTrackerPtrTy_metadata,
    //       arg_metadata.size(),
    //       DINode::DIFlags::FlagObjectPointer,
    //       dwarf::MemorySpace::DW_MSPACE_LLVM_constant,
    //       8u,
    //       nullptr
    //     )
    //   );
    // subPMD->replaceRetainedNodes(DINodeArray(MDTuple::get(NewFn->getContext(),arg_metadata)));
    // // amend function type metadata
    // std::vector<Metadata*> fnTy_metadata;
    // auto fnTyMDs = subPMD->getType();
    // for (auto type : subPMD->getType()->getTypeArray()) fnTy_metadata.push_back(type);
    // fnTy_metadata.push_back(device.DeviceTrackerPtrTy_metadata);
    // subPMD->replaceType(
    //   DISubroutineType::get(
    //       NewFn->getContext(),
    //       fnTyMDs->getFlags(),
    //       fnTyMDs->getCC(),
    //       DITypeRefArray(MDTuple::get(NewFn->getContext(), fnTy_metadata))
    //     )
    //   );
  }
  // replace all references to overriden fn's (special case for calls to the fn to patch in carried param)
  SmallPtrSet<CallInst*, 8u> to_remove;
  // modify to pass device tracker through as last parameter to all functions defined in this module'
  for (auto& tr : to_replace) {
    Function *OldFn, *NewFn; // MDNode* NewFnMDN;
    // std::tie(OldFn, NewFn, NewFnMDN) = tr;
    std::tie(OldFn, NewFn) = tr;
    for (auto u : OldFn->users()) {
      // if (auto CI = dyn_cast<CallInst>(u.getUser())) {
      if (auto CI = dyn_cast<CallInst>(u)) {
        Function* iFn = CI->getFunction();
        if (iFn->getName().ends_with("__old__scabbard_instr_replaced__old__"))
          continue;

        SmallVector<Value*, 4u> operands;
        for (auto& op : CI->args())
          operands.push_back(op.get());
        operands.push_back(iFn->getArg(iFn->arg_size() - 1));
        auto ci = CallInst::Create(NewFn->getFunctionType(), NewFn, ArrayRef<Value*>(operands));
        if (CI->isTailCall())
          ci->setTailCallKind(CI->getTailCallKind());
        if (CI->canReturnTwice())
          ci->setCanReturnTwice();
        ci->insertBefore(CI);
        CI->replaceAllUsesWith(ci);
        ci->setDebugLoc(CI->getDebugLoc());
        ci->setCallingConv(CI->getCallingConv());
        ci->setDebugLoc(CI->getDebugLoc());
        to_remove.insert(CI);

      } else {
        LLVM_DEBUG(errs() << "\n[scabbard.instr.device:DBG] overwritten device function used in non-call instruction!\n";);
      }
    }
    for (auto ci : to_remove)
      ci->eraseFromParent();
    to_remove.clear();
  }
  // remove OldFn from module
  for (auto& tr : to_replace) {
    Function *OldFn, *NewFn;
    std::tie(OldFn, NewFn) = tr;
    OldFn->replaceAllUsesWith(NewFn);
    OldFn->eraseFromParent();
  }
  // clear the list so we're ready for reuse
  to_replace.clear();

  // return if changes were made
  return changed;
}

scabbardIDevicePass::PtrOrigin scabbardIDevicePass::getPtrOrigin(LoopInfo& LI, Value* Ptr, const Value** Object) const {
  // derived from
  // https://github.com/jdoerfert/llvm-project/blob/b416d0c996bc01aeb6708c715bfe5e53bcac998d/llvm/lib/Transforms/Instrumentation/GPUSan.cpp#L592
  SmallVector<const Value*> Objects;
  getUnderlyingObjects(Ptr, Objects, &LI);
  if (Object && Objects.size() == 1)
    *Object = Objects.front();
  PtrOrigin PO = NONE;
  for (auto* Obj : Objects) {
    PtrOrigin ObjPO = HasAllocas ? LOCAL : UNKNOWN_HEAP; //TODO investigate what this means
    switch (Obj->getValueID()) {
      case Value::GlobalVariableVal:
        ObjPO = isSharedGlobal(*((GlobalVariable*) Obj)) ? MANAGED_MEM : DEVICE_HEAP;
        break;
      case Value::ArgumentVal: {
        Argument* Arg = (Argument*) Obj;
        if (Arg->getParent()->hasFnAttribute("kernel"))
          ObjPO = UNKNOWN_HEAP;
        break;
      }
      case Instruction::Alloca + Value::InstructionVal:
        ObjPO = LOCAL;
        break;
      case Instruction::Load + Value::InstructionVal: {
        LoadInst* Load = (LoadInst*) Obj;
        if (auto* Global = dyn_cast<GlobalVariable>(Load->getPointerOperand())) {
          if (Global->getName().ends_with(".device") 
              || _M.getGlobalVariable(Global->getName().str()+".device"))
            ObjPO = DEVICE_HEAP;
          else if (Global->getName().ends_with(".managed") 
              || _M.getGlobalVariable(Global->getName().str()+".managed"))
            ObjPO = MANAGED_MEM; // - using shared for block level shared memory
        }
        break;
      }
      case Instruction::Call + Value::InstructionVal: {
        CallInst* CI = (CallInst*) Obj;
        if (auto* II = dyn_cast<IntrinsicInst>(CI)) { // check if call to intrinsic fn
          if (II->getIntrinsicID() == Intrinsic::amdgcn_implicitarg_ptr ||
            II->getIntrinsicID() == Intrinsic::amdgcn_dispatch_ptr)
          return NEVER; //SYSTEM;
        }
        if (auto* Callee = CI->getCalledFunction())
          if (Callee->getName().starts_with("ompx_")) {
            if (Callee->getName().ends_with("_global"))
              ObjPO = DEVICE_HEAP;
            else if (Callee->getName().ends_with("_local"))
              ObjPO = LOCAL;
            else if (Callee->getName().ends_with("_shared"))
              ObjPO = MANAGED_MEM;
          }
        break;
      }
      default:
        break;
    }
    if (PO == NONE || (PO == UNKNOWN_HEAP && ObjPO > UNKNOWN_HEAP))
      PO = ObjPO;
  }
  return PO;
}

bool scabbardIDevicePass::instrumentInScabbardFunc(LoopInfo& LI, Instruction& I, Value* Ptr, 
                                                   FunctionCallee& ScabbardFn, const InstrData InstrContext) const {
    Value* PtrOp = Ptr;
    const Value *Object = nullptr;
    PtrOrigin PO = getPtrOrigin(LI, PtrOp, &Object);

    if (PO == NO) // don't instrument if it is not accessible outside of the GPU.
      return false;

    auto [locID, is_inserted] = scabbard.Metadata.insert(&I);

    if (not is_inserted && locID == 0ull)
      errs() << "\n[scabbard.instr.device.AMD.I: ERROR] failed to insert instruction into the metadata system!\n";

    // if Ptr is not in the expected address space (0: global) we will need to cast it's Addrspace first.
    //    This is only a LLVM formality and does not cost any compute at runtime.
    llvm::CastInst* castInst = nullptr;
    if (auto ptrTy = dyn_cast<llvm::PointerType>(Ptr->getType()))
      if (ptrTy->getAddressSpace() != 0u) //NOTE: hard coding this might be an issue
        castInst = AddrSpaceCastInst::Create(llvm::Instruction::CastOps::AddrSpaceCast, Ptr, 
                                              ScabbardFn.getFunctionType()->getParamType(2u),
                                            "scabbard.addrCast." + I.getName(),  /* insertBefore= */ &I);

    Function& fn = *I.getFunction();
    Value* kernelDeviceTracker = fn.getArg(fn.arg_size() - 1ul);
    CallInst::Create(
        ScabbardFn,
        std::array<Value*, 4u>{
            kernelDeviceTracker,
            ConstantInt::get(TraceDataTy, APInt(sizeof(InstrData)*8, InstrContext | PO)),
            (castInst ? castInst : Ptr),
            ConstantInt::get(LocDataTy, APInt(sizeof(size_t)*8, locID))
          },
        "scabbard." + I.getName(), 
        /* insertBefore= */ &I
      );
    return true;
  }

// << ========================== Metadata Handler Extra Definitions ============================ >>

inline std::pair<size_t, bool> MetadataHandler::insert(const Instruction* I) {
  auto res = Instructions.insert(std::make_pair(I, Instructions.size()));
  return std::make_pair(res.first->second, res.second);
}

inline Constant* MetadataHandler::getId(const Instruction* I) {
  auto res = Instructions.insert(std::make_pair(I, Instructions.size()));
  if (not res.second && res.first != Instructions.end()) // case failed to insert
    return nullptr;
  return Constant::getIntegerValue(Type::getInt64Ty(I->getContext()), APInt(res.first->second, 64ul));
}

GlobalVariable* MetadataHandler::outputMetadata(Module& M, unsigned AddrSpace) {
  const auto EntryTy = StructType::get(M.getContext(), {}, false);
  SmallVector<Constant*, 16ul> Contents(Instructions.size());
  for (const auto& [I, ID] : Instructions) {
    const DILocation* dLoc = I->getDebugLoc().get();
    const DIFile* dFile = dLoc->getScope()->getFile();
    Contents[ID] = ConstantStruct::get(
        EntryTy, {getSourceFile(dFile, M.getContext()), getCalledFn(dLoc->getSubprogramLinkageName(), M.getContext()),
                  getSourceLine(dLoc, M.getContext()), getSourceCol(dLoc, M.getContext())});
  }

  outputStrings(M);

  const auto ArrTy = ArrayType::get(EntryTy, Instructions.size());
  const auto Arr = ConstantArray::get(ArrTy, Contents);
  return new GlobalVariable(M, cast<Type>(ArrTy), /*IsConstant=*/true, GlobalValue::ExternalLinkage, Arr,
                            "scabbard.device.metadata", nullptr, GlobalValue::NotThreadLocal, AddrSpace);
}

uint64_t MetadataHandler::registerString(const Twine& Str) {
  const auto& It = UniqueStrings.insert({Str, ConcatenatedString.size()});
  if (It.second) {
    ConcatenatedString += Str.str();
    ConcatenatedString.push_back('\0');
  }
  return It.first->second;
}

GlobalVariable* MetadataHandler::outputStrings(Module& M, unsigned AddrSpace) const {
  const auto Arr = ConstantDataArray::getString(M.getContext(), ConcatenatedString);
  return new GlobalVariable(M, cast<Type>(Arr->getType()), /*IsConstant=*/true, GlobalValue::ExternalLinkage, Arr,
                            "scabbard.device.metadata_strings", nullptr, GlobalValue::NotThreadLocal, AddrSpace);
}

ConstantInt* MetadataHandler::getSourceFile(const DIFile* File, LLVMContext& C) {
  auto dir = File->getDirectory();
  auto localPath = File->getFilename();

  uint64_t offset = registerString((dir.ends_with("/")) ? dir + localPath : dir + "/" + localPath);

  return cast<ConstantInt>(Constant::getIntegerValue(IntegerType::get(C, 64ul), APInt(offset, 64ul)));
}

ConstantInt* MetadataHandler::getSourceLine(const DILocation* Loc, LLVMContext& C) const {
  return cast<ConstantInt>(Constant::getIntegerValue(IntegerType::get(C, 64ul), APInt(Loc->getLine(), 64ul)));
}

ConstantInt* MetadataHandler::getSourceCol(const DILocation* Loc, LLVMContext& C) const {
  return cast<ConstantInt>(Constant::getIntegerValue(IntegerType::get(C, 64ul), APInt(Loc->getColumn(), 64ul)));
}

ConstantInt* MetadataHandler::getCalledFn(const StringRef& FnName, LLVMContext& C) {
  uint64_t offset = registerString(FnName);
  return cast<ConstantInt>(Constant::getIntegerValue(IntegerType::get(C, 64ul), APInt(offset, 64ul)));
}

} // namespace <anon>

// << ========================================================================================== >>
// <<                                      TOP LEVEL PASS                                        >>
// << ========================================================================================== >>

// ScabbardPass::ScabbardPass()
//   : /* IF NEEDED */
// {}

PreservedAnalyses ScabbardPass::run(Module& M, ModuleAnalysisManager& MAM) {
  // llvm::errs() << "\n[scabbard.instr.run:DBG] running instrumentation pass\n"; //DEBUG
  const Triple target(M.getTargetTriple());
  bool changed = false;

  if (IS_LTO and target.isAMDGPU()) { // checks for both amdgcn & r600 arch(s) (might need to restrict this to just amdgcn with
                                      // `isAMDGCN()`)
    changed = scabbardAMDDevicePass(M).run(M, MAM);
  } else if (not IS_LTO and not target.isAMDGPU()) { // most likely the host arch
    changed = scabbardHostPass(M, target).run(M, MAM);
  }
  if (changed)
    return PreservedAnalyses::none(); // this will have to change after transforms are performed
  return PreservedAnalyses::all();
}

} // namespace llvm

#undef DEBUG_TYPE // "scabbard"