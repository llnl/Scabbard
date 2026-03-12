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
#include <llvm/Support/Casting.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>
// #include <llvm/Transforms/Utils/Cloning.h>

#include <unordered_set>

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
class scabbardHostPass {

  /// @brief Instrument a host function for scabbard.
  /// @param F the function who's body will be instrumented.
  /// @param FAM the analysis manager for above function.
  /// @return \c bool - if the function was modified.
  bool run(Function& F, FunctionAnalysisManager& FAM) {}

public:
  /// @brief Instrument a host module for scabbard.
  /// @param M the module who's contents will be instrumented.
  /// @param MAM the analysis manager for above module.
  /// @return \c bool - if the module was modified.
  bool run(Module& M, ModuleAnalysisManager& MAM) {}
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
class scabbardIDevicePass {
public:
  /// @brief The general kind of memory space of a pointer type in a AMD Device.
  enum PtrOrigin {
    /// @brief Could not be determined statically at runtime.
    UNKNOWN = 0,
    /// @brief Device global memory, registered host memory, or managed memory.
    GLOBAL = 1,
    /// @brief Block level shared memory (allocated at kernel launch).
    SHARED = 2,
    /// @brief Thread level local allocated on the stack by alloca instructions.
    LOCAL = 3,
    /// @brief Coming from vendor specific util like additional param ptr, or dispatch ptr.
    SYSTEM = 4,
    /// @brief Determined to be none of the Expected types
    NONE = 5
  };

protected:

  const Module& _M;

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

  /* the following members values are set in the \c initFromModule fn */

  /// @brief pointer to the ubiquitous Opaque Pointer Type for the module.
  PointerType* PtrTy = nullptr;
  // PointerType* GlobalPtrTy = nullptr;
  // PointerType* SharedPtrTy = nullptr;
  // PointerType* LocalPtrTy = nullptr;

  /// @brief collections of the types and fn's defined in scabbard's RTL.
  ///        will be pulled from the module as this pass should only run after
  ///        the rtl gets linked in.
  struct scabbardRTL {
    llvm::FunctionCallee trace_append$mem;
    const std::string trace_append$mem_name = SCABBARD_DEVICE_CALLBACK_APPEND_MEM_NAME;
    llvm::FunctionCallee trace_append$alloc;
    const std::string trace_append$alloc_name = SCABBARD_DEVICE_CALLBACK_APPEND_ALLOC_NAME;
    MetadataHandler Metadata;
  } scabbard;

public:
  scabbardIDevicePass() = delete;
  scabbardIDevicePass(Module& M_) : _M(M_) { initFromModule(M_); }

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
  virtual void registerRTL(Module& M) final {
    auto int64Ty = llvm::IntegerType::get(M.getContext(), 64u);
    scabbard.trace_append$mem = M.getOrInsertFunction(
        scabbard.trace_append$mem_name,
        llvm::FunctionType::get(
            llvm::Type::getVoidTy(M.getContext()),
            llvm::ArrayRef<llvm::Type*>(std::vector<llvm::Type*>{
                PtrTy,
                llvm::IntegerType::get(M.getContext(), sizeof(InstrData) * 8),
                PtrTy, //WARN: This constant 0u might need to be dynamicly decided for host modules
                int64Ty
              }),
            false
          )
      );
    scabbard.trace_append$alloc = M.getOrInsertFunction(
        scabbard.trace_append$alloc_name,
        llvm::FunctionType::get(
            llvm::Type::getVoidTy(M.getContext()),
            llvm::ArrayRef<llvm::Type*>(std::vector<llvm::Type*>{
                PtrTy,
                llvm::IntegerType::get(M.getContext(), sizeof(InstrData) * 8),
                PtrTy,
                int64Ty,
                int64Ty
              }),
            false
          )
      );
  }

  virtual void initFromModule(Module& M) {
    PtrTy = PointerType::get(M.getContext(), 0u); // generic opaque ptr in generic/default address space.
    registerRTL(M);
    // TODO other init stuff we needed the module for...
  }

  /// @brief return if a function is a builtin for/from the device vendor.
  /// @param F the Fn in question.
  /// @return \c bool - \c true if \ref F is a builtin from the device vendor.
  virtual bool isDeviceVendorBuiltin(const Function& F) const {
    return false; // DEFAULT: don't do anything (CUDA/NVPTX impl needs to override)
  }

  /// @brief Check to see if this function should be instrumented.
  /// @param F the \c llvm::Function to check
  /// @return \c bool - true if it's appropriate to instrument the fn.
  virtual bool isInstramentableFn(const Function& F) const final {
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
    llvm::errs() << "\n[scabbard.device:WARN] unsupported pointer arithmetic, sanitizer results may be invalid: "
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
    llvm::errs() << "\n[scabbard.device:WARN] unsupported pointer arithmetic, sanitizer results may be invalid: "
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
  ///           in this case the \c isInstramentableFn check.)
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
    if (not isInstramentableFn(F))
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
  virtual bool runImpl(Module& M, ModuleAnalysisManager& MAM) {
    bool changed = false;
    FunctionAnalysisManager& FAM = MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
    for (Function& F : M.functions())
      changed |= run(F, FAM);
    return changed;
  }

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
    // initiate the class members based on the contents of the module
    // add JobState ptrs to the end of all non-intrinsic functions.
    changed |= expandFnParams(M);
    // TODO handle conversions of globals
    // TODO any additional prereq work...
    // run the actual implementation that might be modified in inherited classes
    changed |= runImpl(M, MAM);
    // create and insert the ctor and dtor
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
    // TODO determine if instruction should be instrumented (skip locals, and determine if global, shared, or
    // globalshared)
    Function* fn = Load.getFunction();
    Value* threadState = fn->getArg(fn->arg_size() - 1ul);
    auto addrSpace = Load.getPointerAddressSpace();
    auto ci = CallInst::Create(
        scabbard.MemoryAccess_callee[addrSpace],
        std::array<Value*, 5u>{
            threadState, Load.getPointerOperand(),
            Constant::getIntegerValue(Type::getInt64Ty(fn->getContext()),
                                      APInt(Load.getType()->getScalarSizeInBits() / 8ul, 64ul)),
            Constant::getIntegerValue(Type::getInt64Ty(fn->getContext()), APInt(/*AccessType::READ*/ 0ul, 64ul)),
            scabbard.Metadata.getId(&Load)},
        "scabbard." + Load.getName(), &Load);
    Load.setOperand(Load.getPointerOperandIndex(), ci);
    return true;
  }

  /// @brief When a store instruction is found in a fn this is called.
  ///        It will decide if the Fn is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param Store the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  bool instrumentStoreInst(LoopInfo& LI, StoreInst& Store) override {
    Value *PtrOp = Store.getPointerOperand();
    const Value *Object = nullptr;
    PtrOrigin PO = getPtrOrigin(LI, PtrOp, &Object);

    if (PO > PtrOrigin::GLOBAL) // don't instrument if it is not accessible outside of the GPU.
      return false;

    auto [locID, is_inserted] = scabbard.Metadata.insert(&Store);

    if (not is_inserted && locID == 0ull)
      errs() << "\n[scabbard.instr.device.AMD.Store: ERROR] failed to insert instruction into the metadata system!\n";

    Value* V = Store.getPointerOperand();
    llvm::CastInst* castInst = nullptr;
      if (auto ptrTy = llvm::dyn_cast<llvm::PointerType>(V->getType()))
        if (ptrTy->getAddressSpace() != 0u) { //NOTE hard coding this might be an issue
          castInst = llvm::AddrSpaceCastInst::Create(llvm::Instruction::CastOps::AddrSpaceCast, V, scabbard.trace_append$mem.getFunctionType()->getParamType(2u));
          castInst->insertBefore(&Store);
        }

    Function& fn = *Store.getFunction();
    Value* threadState = fn.getArg(fn.arg_size() - 1ul);
    auto addrSpace = Store.getPointerAddressSpace();
    auto ci = CallInst::Create(
        scabbard.trace_append$mem,
        std::array<Value*, 4u>{
            fn.getArg(fn.arg_size()-1),
            ConstantInt::get(
                IntegerType::get(fn.getContext(), sizeof(InstrData) * 8),
                APInt(sizeof(InstrData)*8, data)
              ),
            (castInst ? castInst : V),
            ConstantInt::get(
                IntegerType::get(fn.getContext(), sizeof(size_t) * 8),
                APInt(sizeof(size_t)*8, locID)
              )
          },
        "scabbard." + Store.getName(), &Store);
    return true;

  }

  /// @brief When a atomicrmw instruction is found in a fn this is called.
  ///        It will decide if the Fn is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param RMW the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  bool instrumentAtomicRMWInst(LoopInfo& LI, AtomicRMWInst& RMW) override {}

  /// @brief When a cmpxchg instruction is found in a fn this is called.
  ///        It will decide if the Fn is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param CXC the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  bool instrumentCmpXChgInst(LoopInfo& LI, AtomicCmpXchgInst& CXC) override {}

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
  bool instrumentCallInst(LoopInfo& LI, CallInst& Call) override {}
};


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
    changed = scabbardHostPass().run(M, MAM);
  }
  if (changed)
    return llvm::PreservedAnalyses::none(); // this will have to change after transforms are performed
  return llvm::PreservedAnalyses::all();
}

// << ========================================================================================== >>
// <<                                     EXTRA DEFINTIONS                                       >>
// << ========================================================================================== >>


bool scabbardIDevicePass::runImpl(Function& F, FunctionAnalysisManager& FAM) {
  bool changed = false;
  LoopInfo& LI = FAM.getResult<LoopAnalysis>(F);
  SmallVector<std::pair<AllocaInst*, Value*>> Allocas;
  SmallVector<ReturnInst*> Returns;
  SmallVector<LoadInst*> Loads;
  SmallVector<StoreInst*> Stores;
  SmallVector<CallInst*> Calls;
  SmallVector<GetElementPtrInst*> GEPs;
  SmallVector<AtomicRMWInst*> AtomicRMWs;
  SmallVector<AtomicCmpXchgInst*> CmpXchgs;
  SmallVector<FenceInst*> Fences;
  SmallVector<IntToPtrInst*> Int2Ptrs;
  SmallVector<PtrToIntInst*> Ptr2Ints;

  for (auto& I : instructions(F)) {
    switch (I.getOpcode()) {
    case Instruction::Alloca: {
      AllocaInst& AI = cast<AllocaInst>(I);
      Allocas.push_back({&AI, nullptr});
      break;
    }
    case Instruction::Load:
      Loads.push_back(&cast<LoadInst>(I));
      break;
    case Instruction::Store:
      Stores.push_back(&cast<StoreInst>(I));
      break;
    case Instruction::GetElementPtr:
      GEPs.push_back(&cast<GetElementPtrInst>(I));
      changed = true;
      break;
    case Instruction::Call: {
      auto& CI = cast<CallInst>(I);
      Calls.push_back(&CI);
      if (CI.isIndirectCall())
        AmbiguousCalls.insert(&CI);
      break;
    }
    case Instruction::Ret:
      Returns.push_back(&cast<ReturnInst>(I));
      break;
    case Instruction::AtomicRMW:
      AtomicRMWs.push_back(&cast<AtomicRMWInst>(I));
      break;
    case Instruction::AtomicCmpXchg:
      // TODO handle cmpxchg instructions
      CmpXchgs.push_back(&cast<AtomicCmpXchgInst>(I));
      break;
    case Instruction::Fence: {
      auto& fence = cast<FenceInst>(I);
      if (fence.getSyncScopeID() ==
          /* TODO figure our what scopes are of interest and how they map to block and warp */)
        Fences.push_back(&fence);
      break;
    }
    case Instruction::IntToPtr:
      Int2Ptrs.push_back(&cast<IntToPtrInst>(I));
      break;
    case Instruction::PtrToInt:
      Ptr2Ints.push_back(&cast<PtrToIntInst>(I));
      break;
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
  for (auto* Fence : Fences)
    changed |= instrumentFenceInst(LI, *Fence);
  for (auto* GEP : GEPs)
    instrumentGEPInst(LI, *GEP);
  for (auto* Call : Calls)
    changed |= instrumentCallInst(LI, *Call);
  for (auto& It : Allocas)
    It.second = instrumentAllocaInst(LI, *It.first);

  instrumentReturns(LI, Allocas, Returns);

  return changed;
}

inline bool scabbardIDevicePass::expandFnParams(Module& M) const {
  // expand the signatures of all relevant fn's
  // (this requires cloning and RAUW since llvm ir fn sigs are immutable)
  bool changed = false;
  SmallVector<std::pair<Function*, Function*>, 8ul> to_replace;
  for (auto& OldFn : M.functions()) {
    if (not isInstramentableFn(OldFn)) // skip fn's that shouldn't be instrumented
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
  PtrOrigin PO = UNKNOWN;
  for (auto* Obj : Objects) {
    PtrOrigin ObjPO = HasAllocas ? UNKNOWN : GLOBAL;
    if (isa<AllocaInst>(Obj)) {
      ObjPO = LOCAL;
    } else if (auto* Global = dyn_cast<GlobalVariable>(Obj)) {
      ObjPO = isSharedGlobal(*Global) ? SHARED : GLOBAL;
    } else if (auto* Load = dyn_cast<LoadInst>(Obj)) {
      if (auto* Global = dyn_cast<GlobalVariable>(Load->getPointerOperand())) {
        if (Global->getName().ends_with(".device") 
            || _M.getGlobalVariable(Global->getName().str()+".device"))
          ObjPO = GLOBAL;
        else if (Global->getName().ends_with(".managed") 
            || _M.getGlobalVariable(Global->getName().str()+".managed"))
          ObjPO = GLOBAL; // SHARED; // - using shared for block level shared memory
      }
    } else if (auto* II = dyn_cast<IntrinsicInst>(Obj)) {
      if (II->getIntrinsicID() == Intrinsic::amdgcn_implicitarg_ptr ||
          II->getIntrinsicID() == Intrinsic::amdgcn_dispatch_ptr)
        return SYSTEM;
    } else if (auto* CI = dyn_cast<CallInst>(Obj)) {
      if (auto* Callee = CI->getCalledFunction())
        if (Callee->getName().starts_with("ompx_")) {
          if (Callee->getName().ends_with("_global"))
            ObjPO = GLOBAL;
          else if (Callee->getName().ends_with("_local"))
            ObjPO = LOCAL;
          else if (Callee->getName().ends_with("_shared"))
            ObjPO = SHARED;
        }
    } else if (auto* Arg = dyn_cast<Argument>(Obj)) {
      if (Arg->getParent()->hasFnAttribute("kernel"))
        ObjPO = GLOBAL;
    }
    if (PO == NONE || PO == ObjPO)
      PO = ObjPO;
    else
      return UNKNOWN;
  }
  return PO;
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

} // namespace

#undef DEBUG_TYPE // "scabbard"