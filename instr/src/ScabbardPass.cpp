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
#include <scabbard/Metadata.hpp>

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
#include <bitset>

#define DEBUG_TYPE "scabbard"

#define EMPTY_LIST_6 {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr}

namespace llvm {
namespace /*<anon>*/ {

namespace _DBG {
  
    auto write_module_to_file(const llvm::Module& M, const std::string file_name_mod="") -> std::string {
      std::error_code EC;
      std::string filename = M.getSourceFileName() + "." + file_name_mod + ".ll";
      llvm::raw_fd_ostream OS(filename, EC);
      if (EC) {
          return "<file-not-found>";
      }
      M.print(OS, nullptr);
      return filename;
    }
  } //?namespace _DBG

inline raw_ostream& operator << (raw_ostream& out, const scabbard::InstrData& data);

using namespace scabbard;

class IRHelper;

// << ========================================================================================== >>
// <<                                     METADATA HANDLER                                       >>
// << ========================================================================================== >>

struct DenseMapMetadataKeyInfo : public DenseMapInfo<Instruction*> {
  static unsigned getHashValue(const Instruction *PtrVal) {
    return (unsigned((uintptr_t)PtrVal->getDebugLoc().get()) >> 4) ^
           (unsigned((uintptr_t)PtrVal->getDebugLoc().get()) >> 9);
  }
  static bool isEqual(const Instruction* LHS, const Instruction* RHS) {
    if (LHS == RHS)
      return true;
    const llvm::DILocation *LLoc = LHS->getDebugLoc().get();
    const llvm::DILocation *RLoc = RHS->getDebugLoc().get();
    // No debug info, or "compiler-generated" (line 0): can't say anything.
    if (!LLoc || !RLoc || LLoc->getLine() == 0 || RLoc->getLine() == 0)
      return false;
    if (LLoc->getLine() != RLoc->getLine() || LLoc->getColumn() != RLoc->getColumn())
      return false;
    // Same file? Compare the scope's file, not just the line number.
    if (LLoc->getFile() != RLoc->getFile())   // DIFile nodes are uniqued
      return false;
    // // Same source text inlined at different call sites = different instances.
    // if (LLoc->getInlinedAt() != RLoc->getInlinedAt())
    //   return false;
    return true;
  }
};

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
  DenseMap<const Instruction*, size_t/* , DenseMapMetadataKeyInfo */> Instructions;
  StringMap<size_t> UniqueStrings;
  std::string ConcatenatedString;
  size_t UniqueStringId = 0ul;
  StructType* EntryTy = nullptr;
  /// @brief Global that contains the metadata info
  ///        \em NOTE: this variable is only a temp value that will be replaced durring the finalize step
  ///        with the actual contents, so it will not contain any useful info at runtime.
  GlobalVariable* MetadataVar = nullptr;

public:
  /// @brief Insert the metadata associated with an instruction into the set of all
  ///        instructions of interest. (set kept unique)
  /// @param I the instruction to try to insert
  /// @return \c std::pair<Constant*,bool> - a constant ptr to the metadata entry for the instruction
  ///         bool is \c true if the insertion takes place,
  ///         and \c false if it fails to occur because of an issue or it already
  ///         existed in the metadata set
  inline std::pair<Constant*, bool> insert(const Instruction* I);

  /// @brief Create the module metadata object
  /// @param M the module to inject the metadata object into
  /// @param AddrSpace the integer value associated with the global address space for
  ///        the target architecture (0 for CPU/HOST, 1 for most GPUs/DEVICES)
  /// @param IRH the IRHelper containing common types used to construct the metadata 
  /// @return \c llvm::GlobalVariable* - pointer to the module global created holding the metadata.
  GlobalVariable* initializeMetadata(Module& M, unsigned AddrSpace, IRHelper* IRH);

  /// @brief connect and fill the remaining metadata information.
  /// @param M the module to inject the metadata object into
  void finalize(Module& M);

  /// @brief remove the named globals created by this object.
  ///        (called if no instrumentation occurred in this module)
  void clean();

private:
  /// @brief Insert the metadata associated with an instruction into the set of all
  ///        instructions of interest. (set kept unique)
  /// @param I the instruction to try to insert
  /// @return \c std::pair<size_t,bool> - size_t is the metadata id for the instruction
  ///         bool is \c true if the insertion takes place,
  ///         and \c false if it fails to occur because of an issue or it already
  ///         existed in the metadata set
  inline std::pair<uint64_t, bool> _insert(const Instruction* I);

  /// @brief Helper Fn to quickly create \c getelementptr \c ConstantExpr 's for use in
  ///         creating references to things.
  /// @param GV \c GlobalVariable to get the element ptr from
  /// @param Index the post referencing global location index
  /// @return \c Constant* - ptr to a GEP constant expression
  Constant* getGEP(GlobalVariable* GV, size_t Index) const;

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
  /// @param srcHash64 string representation of the modules hash as a base 64 number (used to unique global var name)
  /// @return the global string array already inserted into the module
  GlobalVariable* outputStrings(Module& M, unsigned AddrSpace, const std::string& srcHash64) const;

  /// @brief Retrieve the source directory & file name from the metadata,
  ///        combine them into a full file path, try to register the string with
  ///        \c registerString, and return the offset to the desired string in
  ///        the global string array as a constant int.
  /// @param File the metadata object containing the file and directory components
  /// @return \c size_t - offset into the combined string to the start of the function name.
  inline size_t getSourceFile(const DIFile* File);

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

  /// @brief Get the default value for the lazy load ID of this metadata entry
  /// @param C (required for generating constants for the module)
  /// @return a constant int of 0 64bits wide
  inline ConstantInt* getLazyID(const DIFile* DFile, const DILocation* DLoc, Module& M) const;

  /// @brief Retrieve the name of the original function containing the instruction in question
  ///        try to register the string with \c registerString,
  ///        and return the offset to the desired string in the global string array
  ///        as a constant int.
  /// @param FnName the name of the function that originally contained the instruction in question
  ///        (usually stored in the as Subprogram linkage name)
  /// @return \c size_t - offset into the combined string to the start of the function name.
  inline size_t getCalledFn(const StringRef& FnName);
};


/// @brief Class for standard design decisions common to all Scabbard Instrumentation passes
class IScabbardInstrPass {

  /// @brief get the ctor's or dtors from the specifed global variable
  /// @param G_torArr global variable containing the array to extract global ctors or dtors from.
  void collect_tors(const GlobalVariable* G_torArr) {
    if (const auto* val = dyn_cast_or_null<ConstantArray>(G_torArr->getInitializer())) {
      for (const auto& _entry : val->operands()) 
        if (const auto* entry = dyn_cast_or_null<ConstantStruct>(_entry.get())) {
          if (const auto _torFn = dyn_cast_or_null<Function>(entry->getAggregateElement(1u))) {
            Module_tors.insert(_torFn);
          }
      }
    }
  }

protected:
  /// @brief set of all ctors and dtors in the module for quick reference
  std::set<const Function*> Module_tors;

public:
  /// @brief Produce a string for log/error messages that gives the source file and location in 
  ///        \c `\"<filepath>\":<line>,<col>` format
  /// @param I instruction to get the location for
  /// @return \c std::string - the formatted location string for log/error messages
  static inline Twine getLocStr(const Instruction& I);

  /// @brief Produce a string for log/error messages that gives the source file and location in 
  ///        \c `\"<filepath>\":<line>,<col>` format
  /// @param V possible instruction to get the location for but still referenced as a \c Value
  /// @return \c std::string - the formatted location string for log/error messages
  static inline Twine getLocStr(const Value* V);

  /// @brief The general kind of memory space of a pointer type in a AMD Device.
  using PtrOrigin = scabbard::InstrData;

  IScabbardInstrPass(Module& M) {
    if (const auto ctors = M.getGlobalVariable("llvm.global_ctors")) { collect_tors(ctors); }
    if (const auto dtors = M.getGlobalVariable("llvm.global_dtors")) { collect_tors(dtors); }
  }
  IScabbardInstrPass() = delete;

};


/// @brief A helper class that holds a reference to the module the pass is working on 
///        and some commonly used types for easy access.
class IRHelper {
protected:
  /// @brief a handy copy of module to use when you just need to get a context or quick query.
  const Module& _M;
  /// @brief a handy pointer to the oft used IR void type.
  Type* const VoidTy = nullptr;
  /// @brief a handy pointer the oft used generic IR pointer type for this module.
  PointerType* const PtrTy = nullptr;
  /// @brief a handy pointer to the oft used IR Integer Type for the RTL's TraceData
  IntegerType* const TraceDataTy = nullptr;
  /// @brief a handy pointer to the oft used IR Integer Type for the RTL's source code location metadata ID
  PointerType* const LocDataTy = nullptr;
  /// @brief a handy pointer to the oft used IR Integer Type for the RTL's exta info (size_t/uint64_t) type.
  IntegerType* const ExtraDataTy = nullptr;
  /// @brief a handy pointer to the oft used IR Integer Type unsigned 64-bit.
  IntegerType* const u64Ty = nullptr;
  /// @brief a handy pointer to the oft used IR Integer Type unsigned 32-bit.
  IntegerType* const u32Ty = nullptr;
  /// @brief a handy pointer to the oft used IR Integer Type unsigned 8-bit.
  IntegerType* const u8Ty = nullptr;
  Type* const DTTy = nullptr;
  StructType* const UserCallbackDataWrapperTy = nullptr;
public:
  IRHelper() = delete;
  IRHelper(Module& M) :
    _M(M),
    VoidTy(Type::getVoidTy(M.getContext())),
    PtrTy(PointerType::getUnqual(M.getContext())),
    TraceDataTy(IntegerType::get(M.getContext(), sizeof(InstrData)*8)),
    LocDataTy(PointerType::get(M.getContext(), 0ull)),
    ExtraDataTy(IntegerType::get(M.getContext(), 64u)),
    u64Ty(IntegerType::get(M.getContext(), 64u)),
    u32Ty(IntegerType::get(M.getContext(), 32u)),
    u8Ty(IntegerType::get(M.getContext(), 8u)),
    DTTy(u64Ty),
    UserCallbackDataWrapperTy(StructType::create(M.getContext(),
          std::array<Type*,2u>{PtrTy, PtrTy},
          "scabbard.instr.userCallbackDataWrapperTy",
          false))
    {}

  friend class MetadataHandler;
  
  protected:
  void setDebugLoc(Instruction* i, const Instruction& I) const;
  void setDebugLoc(Instruction* i, const Instruction* I) const { setDebugLoc(i,*I); }
  Function* cloneWithTrailingArg(Function &F, Type* T, const Twine& arg_name) const;
  void insertHandlingInvokes(CallInst* ci, CallBase& CB, bool InsertBefore=false) const;
  CallBase* addArgumentToCallBase(CallBase *CB, FunctionCallee Fn, Value *NewArg) const;
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
    llvm::FunctionCallee scabbard_close;
    const std::string scabbard_close_name = SCABBARD_CALLBACK_CLOSE_NAME;
    llvm::FunctionCallee trace_append;
    const std::string trace_append_name = SCABBARD_HOST_CALLBACK_APPEND_NAME;
    llvm::FunctionCallee trace_append$cond; 
    const std::string trace_append$cond_name = SCABBARD_HOST_CALLBACK_APPEND_COND_NAME;
    llvm::FunctionCallee trace_append$ext;
    const std::string trace_append$ext_name = SCABBARD_HOST_CALLBACK_APPEND_EXT_NAME;
    llvm::FunctionCallee trace_append$ext$cond;
    const std::string trace_append$ext$cond_name = SCABBARD_HOST_CALLBACK_APPEND_EXT_COND_NAME;
    llvm::FunctionCallee register_job;
    const std::string register_job_name = SCABBARD_CALLBACK_REGISTER_JOB_NAME;
    llvm::FunctionCallee register_job_stub;
    const std::string register_job_stub_name = SCABBARD_CALLBACK_REGISTER_JOB_IN_STUB_NAME;
    llvm::FunctionCallee register_job_callback;
    const std::string register_job_callback_name = SCABBARD_CALLBACK_REGISTER_JOB_CALLBACK_NAME;
    llvm::FunctionCallee register_user_callback;
    const std::string register_user_callback_name = SCABBARD_CALLBACK_REGISTER_USER_CALLBACK_NAME;
    llvm::FunctionCallee register_user_hostFn_launch;
    const std::string register_user_hostFn_launch_name = SCABBARD_CALLBACK_REGISTER_USER_HOST_FN_LAUNCH_NAME;
    llvm::FunctionCallee debug_print_ull;
    const std::string debug_print_ull_name = SCABBARD_HOST_DEBUG_PRINT_ULL_NAME;
    MetadataHandler Metadata;
  } ScabbardRTL;

  /// @brief Map of GLobal Variable Names of Variables known to be in Unified memory to what heap they are in. \n 
  ///        (if not in map assume it is in \c UNKNOWN_HEAP ).
  StringMap<PtrOrigin> GlobalUnifiedMemVar;

  /// @brief Fn Type that describes a Function that will instrument a call of a GPU Driver API Fn, 
  ///        and return true if any changes were made to the module durring the process.
  using APIInstrumenterFn_t = std::function<bool(CallBase&,FunctionAnalysisManager&)>;

  /// @brief List of {APIFnName, APIHandlerFn} that will proccess the relevant GPU Driver API calls.
  SmallVector<std::pair<const StringRef,APIInstrumenterFn_t>,23u> APIInstrumenters;

  /// @brief Returns \c false if the function should not be instrumented. \n
  ///        Used by the top level run method to determine if the pass will run on a fn. \n 
  ///        Override in child classes if you want to change what is instrumentable.
  /// @param F the function to examine
  /// @return \c bool - if \param F should be instrumented or skipped.
  virtual inline bool isInstrumentableFn(const Function& F) const {
    return (not F.getName().starts_with("__hip_module_")    // exclude any module ctor or dtor
            && not F.isDeclaration()                        // exclude any functions not defined
            && not F.getName().starts_with("llvm.")         // exclude intrinsics
            && not F.getName().starts_with("scabbard.")     // exclude name mangled scabbard rtl functions
            && not F.getName().contains("__device_stub__")  // exclude device stubFn's from regular instruction instr
            // any fn marked as not to be instrumented
            && not (F.hasFnAttribute(Attribute::AttrKind::DisableSanitizerInstrumentation)
                    || F.hasFnAttribute(Attribute::AttrKind::NoSanitizeCoverage))
            ); 
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
  /// @return \c IScabbardDevicePass::PtrOrigin - the memory space type of the
  ///         ptr provided.
  virtual PtrOrigin getPtrOrigin(LoopInfo& LI, const Value* Ptr, const Value** Object) const;

  /// @brief Recursive helper for `getPrtOrigin()`.
  ///        it should just contain the body of the switch statement for decisions on what to do when 
  ///        an origin object is one kind of `llvm::Value` object or another.
  ///        Used primarily when looking through a load instruction.
  /// @param Ptr `llvm::Value*` - The next branch to look down.
  /// @param Object `const llvm::Value**` - Where to store the Origin object's ptr if discovered, else it will store nothing.
  /// @param max_level `std::size_t` - max number of `llvm::LoadInst` to recurse through
  /// @return what the root `PtrOrigin` is for this branch
  virtual PtrOrigin _getPtrOriginHelper(const Value* Ptr, const Value** Object, std::size_t max_level) const;

  /// @brief Simple single interface to insert the call to Scabbard's RTL func for any kind of instruction.
  ///        It will determine based off of where \c Ptr comes from if it should instrument before instrumenting.
  ///        If it should not instrument it will return \c false.
  /// @param LI The Loop info for the function/module
  /// @param I The instruction being instrumented
  /// @param Ptr The Pointer Operand of above Instruction
  /// @param InstrContext The metadata about where the memory is located and what actions are being done on it.
  /// @param ExtraData If the use of an extra data variant of a scabbard RTL fn should be used. 
  /// @return if the instrumentation actually took place.
  inline bool instrumentInScabbardFunc(LoopInfo& LI, Instruction& I, 
                                        Value* Ptr, const InstrData InstrContext,
                                        Value* ExtraData=nullptr);


  /// @brief When a load instruction is found in a fn this is called.
  ///        It will decide if the Fn is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param Load the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  virtual bool instrumentLoadInst(LoopInfo& LI, LoadInst& Load) {
    return instrumentInScabbardFunc(LI, Load, Load.getPointerOperand(), 
                                    InstrData::ON_CPU | InstrData::READ 
                                    | (Load.isAtomic() ? InstrData::ATOMIC : InstrData::NO));
  }

  /// @brief When a store instruction is found in a fn this is called.
  ///        It will decide if the Fn is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param Store the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  virtual bool instrumentStoreInst(LoopInfo& LI, StoreInst& Store) {
    return instrumentInScabbardFunc(LI, Store, Store.getPointerOperand(),
                                    InstrData::ON_CPU | InstrData::WRITE 
                                    | (Store.isAtomic() ? InstrData::ATOMIC : InstrData::NO));
  }

  /// @brief When a atomicrmw instruction is found in a fn this is called.
  ///        It will decide if the Fn is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param RMW the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  virtual bool instrumentAtomicRMWInst(LoopInfo& LI, AtomicRMWInst& RMW) {
    return instrumentInScabbardFunc(LI, RMW, RMW.getPointerOperand(), 
                                    InstrData::ON_CPU | InstrData::READ 
                                    | InstrData::WRITE | InstrData::ATOMIC);
  }

  /// @brief When a cmpxchg instruction is found in a fn this is called.
  ///        It will decide if the Fn is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param CXC the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  virtual bool instrumentCmpXChgInst(LoopInfo& LI, AtomicCmpXchgInst& CXC) {
    return instrumentInScabbardFunc(LI, CXC, CXC.getPointerOperand(),
                                    InstrData::ON_CPU | InstrData::WRITE 
                                    | InstrData::READ | InstrData::ATOMIC);
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
    // SmallVector<std::pair<APIInstrumenterFn_t&, CallBase*>> to_process;
    for (auto [APIName, InstrFn] : APIInstrumenters)
      if (Function* APIFn = M.getFunction(APIName)) {
        SmallVector<CallBase*> to_process;
        for (auto user : APIFn->users())
          if (auto _CB = dyn_cast<CallBase>(user))
            if (isInstrumentableFn(*_CB->getFunction()))
              if (APIFn == _CB->getCalledFunction())
                to_process.emplace_back(_CB);
                // changed |= InstrFn(*_CB, FAM);
        for (auto _CB : to_process)
          changed |= InstrFn(*_CB, FAM);
      }
    //             to_process.emplace_back(InstrFn,_CB);
    // for (auto [InstrFn, _CB] : to_process)
    //   changed |= InstrFn(*_CB, FAM);
    return changed;
  }

  /// @brief Instrument in the calls to scabbard.init to occur at the entry point of the program.
  ///        According to the conventions of the Target system.
  ///        For POSIX systems (default) this is the \c main() function. \n
  ///        Create virtual overrides for other Target Arch OS configs.
  /// @param M the module to work in.
  /// @return \c bool - if any changes were made to \param M
  virtual bool instrumentScabbardInit(Module& M) const {
    if (auto mainFn = dyn_cast_or_null<Function>(M.getFunction("main"))) {
      auto firstInst = &(*(mainFn->getEntryBlock().getFirstInsertionPt()));
      auto ci = llvm::CallInst::Create(ScabbardRTL.scabbard_init, "", firstInst);
      setDebugLoc(ci,firstInst);
      return true;
    }
    return false;
  }

  /// @brief Instrument in the calls to scabbard.close to occur before all exit points of the program.
  ///        For POSIX systems \em (default) this is at calls to \c exit()
  ///        and any return instructions in the \c main() function. \n
  ///        Create virtual overrides for other Target Arch OS configs.
  /// @param M the module to work in.
  /// @return \c bool - if any changes were made to \param M
  virtual bool instrumentScabbardClose(Module& M) const {
    bool changed = false;
    if (Function* exitFn = M.getFunction("exit")) {
        for (auto user : exitFn->users())
          if (auto _CI = dyn_cast<CallInst>(user))
            if (exitFn == _CI->getCalledFunction()) {
              auto ci = CallInst::Create(ScabbardRTL.scabbard_close, "", _CI);
              setDebugLoc(ci,_CI);
              changed = true;
            }
    }
    if (auto mainFn = dyn_cast_or_null<Function>(M.getFunction("main"))) {
      SmallVector<ReturnInst*> Returns;
      for (auto& I : instructions(mainFn))
        if (auto* RI = dyn_cast<ReturnInst>(&I)) {
          Returns.push_back(RI);
        }
      for (auto* RI : Returns) {
        auto ci = CallInst::Create(ScabbardRTL.scabbard_close, "", RI);
        setDebugLoc(ci,RI);
      }
      if (not Returns.empty())
        changed = true;
    }
    return changed;
  }


public:

  IScabbardHostPass() = delete;
  IScabbardHostPass(Module& M, const Triple& Triple_) : 
    IScabbardInstrPass(M), IRHelper(M), 
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
    errs() << "\n[scabbard.instr.host:INFO] Instrumenting: \"" << M.getSourceFileName() << "\"\n"; //DEBUG
    errs() << "\n\n[scabbard.instr.host:DBG] Module written to: \"" << _DBG::write_module_to_file(M,"host.preInstr") << "\"\n\n"; //DEBUG

    // initialize metadata object and get a copy of the temp global variable for storing the metadata in.
    GlobalVariable* MetadataVar = ScabbardRTL.Metadata.initializeMetadata(M, 0ull, this);


    // notate all global variables that are known to be in Unified memory at some point (in this module/TU)
    registerGlobalVarsInUnifiedMemory(M);
    // add definitions for our RTL fn's
    registerRTL(M);

    // run the actual implementation that might be modified in inherited classes
    bool changed = runImpl(M, MAM);

    FunctionAnalysisManager& FAM = MAM.getResult<FunctionAnalysisManagerModuleProxy>(M)
                                      .getManager();

    for (Function& F: M.functions())
      changed |= run(F, FAM);

    // instrument the GPU driver API calls as appropriate for the specific API suite.
    changed |= handleAPICalls(M, FAM);
    
    if (changed) //NOTE: this method of metadata will need to be altered to work with instrumenting durring compilation instead of LTO.
      ScabbardRTL.Metadata.finalize(M);
    else
      ScabbardRTL.Metadata.clean();    // cleanup metadata stuff if nothing was added.

    // add in RTL initializer and de-initializers
    if (not instrumentScabbardInit(M)) {
      if (Function* init = dyn_cast_or_null<Function>(ScabbardRTL.scabbard_init.getCallee())) {
        init->eraseFromParent();
      }
    } else 
      changed |= true;
    if (not instrumentScabbardClose(M)) {
      if (Function* close = dyn_cast_or_null<Function>(ScabbardRTL.scabbard_close.getCallee())) {
        close->eraseFromParent();
      }
    } else
      changed |= true;

    errs() << "\n\n[scabbard.instr.host:DBG] Module written to: \"" << _DBG::write_module_to_file(M,"host.postInstr") << "\"\n\n"; //DEBUG

    return changed;
  }

};


/// @brief Scabbard instrumentation pass implementation for AMD's HIP API.
class ScabbardHostPassHip : public IScabbardHostPass {
  /// @brief Load up the API Instrumenters into \c IScabbardHostPass::APIInstrumenters
  void registerAPIInstrumenters();
protected:
  CallInst* CreateRTLCall(CallBase& CB, const InstrData data, 
                          Value* Ptr, const bool InsertBefore=true);
  CallInst* CreateRTLCallEx(CallBase& CB, const InstrData data, Value* Ptr, 
                            Value* Extra, const bool InsertBefore=true);
  bool APIInstr_Alloc(CallBase& CB, const InstrData Data);
  bool APIInstr_Memcpy(CallBase& CB, FunctionAnalysisManager& FAM, const uint CopyDir,
                       Value* Stream=nullptr, const bool IsAsync=false);
  bool APIInstr_Unsupported(const CallInst& CB, const StringRef APIName) const;
  bool APIInstr_LaunchKernel(CallBase& CB);
  AllocaInst* expand_param_args_alloc(const CallBase& CB, AllocaInst& alloc) const;
  AllocaInst* create_or_expand_param_args_alloc(const CallBase& CB, Value* arg) const;
  Constant* getBestLocIDForUserCallback(CallBase& RegCallbackCI);
  bool APIInstr_HostCallback(CallBase& CB);
  bool APIInstr_HostFnLaunch(CallBase& CB);
  bool runImpl(Module& M, ModuleAnalysisManager& MAM) override;
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
class IScabbardDevicePass : public IScabbardInstrPass, public IRHelper {
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
    FunctionCallee trace_append;
    const std::string trace_append_name = SCABBARD_DEVICE_CALLBACK_APPEND_NAME;
    FunctionCallee trace_append$ext;
    const std::string trace_append$ext_name = SCABBARD_DEVICE_CALLBACK_APPEND_EXT_NAME;
    FunctionCallee dbg_print_ull;
    const std::string dbg_print_ull_name = SCABBARD_DEVICE_DEBUG_PRINT_ULL_NAME;
    MetadataHandler Metadata;
  } ScabbardRTL;

public:
  IScabbardDevicePass() = delete;
  IScabbardDevicePass(Module& M) : IScabbardInstrPass(M), IRHelper(M) {}

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
    return (Module_tors.count(&F) == 0                   // exclude any module ctor or dtor
            && not F.isDeclaration()                     // exclude any functions not defined
            && not F.getName().starts_with("llvm.")      // exclude intrinsics
            && not F.getName().starts_with("scabbard.")  // exclude name mangled scabbard rtl functions
            && not NO_INSTR_FNS.count(F.getName().str()) // a manually excluded function (usually c++ builtin)
            && not isDeviceVendorBuiltin(F)              // a device/vendor specific function. (should be intrinsics
            // and -> undeclared)
            // any fn marked as not to be instrumented
            && not (F.hasFnAttribute(Attribute::AttrKind::DisableSanitizerInstrumentation)
                    || F.hasFnAttribute(Attribute::AttrKind::NoSanitizeCoverage))); 
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
  /// @return \c IScabbardDevicePass::PtrOrigin - the memory space type of the
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
                                       FunctionCallee& ScabbardFn, const InstrData InstrContext);

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
  // virtual Function* createCTor(Module& M, ModuleAnalysisManager& MAM, GlobalVariable* MetadataVar) const = 0;

  /// @brief Create scabbard's module deconstructor (dtor) function for the target arch.
  ///        (Must be defined in child classes,
  ///          \c scabbardIDevice pass marks it abstract)
  ///         Return the function and the calling class will insert it into
  ///         the appropriate global object to register it as a dtor.
  /// @param M the module to create the dtor in and for.
  /// @param MAM the analysis manager for above module
  /// @return \ref llvm::Function \c* - ptr to the dtor fn.
  // virtual Function* createDTor(Module& M, ModuleAnalysisManager& MAM) const = 0;

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
      return false;
    // TODO any prereq work...
    // run the actual implementation that might be modified in inherited classes
    return runImpl(F, FAM);
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
    errs() << "\n[scabbard.instr.device:INFO] Instrumenting: \"" << M.getSourceFileName() << "\"\n"; //DEBUG
    bool changed = false;
    // initialize metadata object and get a copy of the temp global variable for storing the metadata in.
    GlobalVariable* MetadataVar = ScabbardRTL.Metadata.initializeMetadata(M, 0ull, this);
    errs() << "\n\n[scabbard.instr.device:DBG] Module written to: \"" << _DBG::write_module_to_file(M,"device.preInstr") << "\"\n\n"; //DEBUG
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
    bool alt_changed = expandFnParams(M);
    // create and insert the ctor and dtor

    FunctionAnalysisManager& FAM = MAM.getResult<FunctionAnalysisManagerModuleProxy>(M)
                                      .getManager();
    for (Function& F : M.functions())
      changed |= run(F, FAM);

    // if (changed) {                                 // if any instrumentation occurred insert the ctor and dtor
    //   Function* CtorFn = this->createCTor(M, MAM); // defined by implementing class
    //   CtorFn->addFnAttr(Attribute::DisableSanitizerInstrumentation);
    //   appendToGlobalCtors(M, CtorFn, 0, nullptr);
    //   Function* DtorFn = this->createDTor(M, MAM); // defined by implementing class
    //   DtorFn->addFnAttr(Attribute::DisableSanitizerInstrumentation);
    //   appendToGlobalDtors(M, CtorFn, 0, nullptr);
    // }
    // TODO handle ambagious calls

    if (changed) //NOTE: this method of metadata will need to be altered to work with instrumenting durring compilation instead of LTO.
      ScabbardRTL.Metadata.finalize(M);
    else
      ScabbardRTL.Metadata.clean();
 

    errs() << "\n\n[scabbard.instr.device:DBG] Module written to: \"" << _DBG::write_module_to_file(M,"device.postInstr") << "\"\n\n"; //DEBUG

    return changed || alt_changed;
  }
};

const std::unordered_set<std::string> IScabbardDevicePass::NO_INSTR_FNS{
    "__ockl_hostcall_internal", "__ockl_dm_alloc", "__cxa_pure_virtual",
    "__cxa_deleted_virtual",    "__assertfail",    "__assert_fail"};

// << ========================================================================================== >>
// <<                                AMD DEVICE INSTR IMPL PASS                                  >>
// << ========================================================================================== >>

/// @brief Instrumentation pass for AMD GPUs.
///        Assuming code was written for a ROCm compatible GPU using ROCm toolchains.
///        Code can be written in any compatible language for ROCm.
///
///         Inherits most of it's functionality form it's parent interface \ref IScabbardDevicePass
///
class ScabbardAMDDevicePass final : public IScabbardDevicePass {
  /// @brief the integer associated with the shared address space on AMD GPUs.
  const uint64_t SHARED_ADDRESS_SPACE = 3ul;

public:
  ScabbardAMDDevicePass(Module& M) 
  : IScabbardDevicePass(M),
    PtrDGTy(PointerType::get(M.getContext(), 1ull))
  {}
  ScabbardAMDDevicePass() = delete;

protected:

  /// @brief the oft used generic IR pointer type in the Devices Global AddressSpace for this module.
  PointerType* const PtrDGTy = nullptr;

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
  // Function* createCTor(Module& M, ModuleAnalysisManager& MAM, GlobalVariable* MetadataVar) const override {}

  /// @brief Create scabbard's module deconstructor (dtor) function for the target arch.
  ///         Return the function and the calling class will insert it into
  ///         the appropriate global object to register it as a dtor.
  /// @param M the module to create the dtor in and for.
  /// @param MAM the analysis manager for above module
  /// @return \ref llvm::Function \c* - ptr to the dtor fn.
  // Function* createDTor(Module& M, ModuleAnalysisManager& MAM) const override {}


  /// @brief When a load instruction is found in a fn this is called.
  ///        It will decide if the Fn is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param Load the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  bool instrumentLoadInst(LoopInfo& LI, LoadInst& Load) override {
    return instrumentInScabbardFunc(LI, Load, Load.getPointerOperand(), 
                                    ScabbardRTL.trace_append, 
                                    InstrData::ON_GPU | InstrData::READ 
                                    | (Load.isAtomic() ? InstrData::ATOMIC : InstrData::NO));
  }

  /// @brief When a store instruction is found in a fn this is called.
  ///        It will decide if the Fn is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param Store the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  bool instrumentStoreInst(LoopInfo& LI, StoreInst& Store) override {
    return instrumentInScabbardFunc(LI, Store, Store.getPointerOperand(), 
                                    ScabbardRTL.trace_append, 
                                    InstrData::ON_GPU | InstrData::WRITE 
                                    | (Store.isAtomic() ? InstrData::ATOMIC : InstrData::NO));
  }

  /// @brief When a atomicrmw instruction is found in a fn this is called.
  ///        It will decide if the Fn is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param RMW the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  bool instrumentAtomicRMWInst(LoopInfo& LI, AtomicRMWInst& RMW) override {
    return instrumentInScabbardFunc(LI, RMW, RMW.getPointerOperand(), 
                                    ScabbardRTL.trace_append, 
                                    InstrData::ON_GPU | InstrData::READ 
                                    | InstrData::WRITE | InstrData::ATOMIC);
  }

  /// @brief When a cmpxchg instruction is found in a fn this is called.
  ///        It will decide if the Fn is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param CXC the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  bool instrumentCmpXChgInst(LoopInfo& LI, AtomicCmpXchgInst& CXC) override {
    return instrumentInScabbardFunc(LI, CXC, CXC.getPointerOperand(), 
                                    ScabbardRTL.trace_append, 
                                    InstrData::ON_GPU | InstrData::WRITE 
                                    | InstrData::READ | InstrData::ATOMIC);
  }

  /// @brief When a fence instruction is found in a fn this is called.
  ///        It will decide if the Fn is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param Fence the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  bool instrumentFenceInst(LoopInfo& LI, FenceInst& Fence) override { return false; }

  /// @brief When a call instruction is found in a fn this is called.
  ///        It will decide if the Fn is worth instrumenting
  ///        and perform said instrumentation as required.
  /// @param LI The loop info / analysis for the parent fn.
  /// @param Call the instruction in question.
  /// @return \c bool - if any changes were made to the instruction, parent fn, or module.
  bool instrumentCallInst(LoopInfo& LI, CallInst& Call) override {
    //TODO figure out what calls to instrument
    return false;
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

inline Twine IScabbardInstrPass::getLocStr(const Value* V) {
  if (const Instruction* I = dyn_cast<Instruction>(V)) {
    return IScabbardInstrPass::getLocStr(*I);
  }
  return Twine(V->getName());
}

void IRHelper::setDebugLoc(Instruction* i, const Instruction& I) const {
  if (I.getDebugLoc())
    i->setDebugLoc(I.getDebugLoc());
  else if (DISubprogram *SP = I.getFunction()->getSubprogram()) {
    i->setDebugLoc(DILocation::get(I.getContext(), 0, 0, SP));
  }
}

void IRHelper::insertHandlingInvokes(CallInst* ci, CallBase& CB, bool InsertBefore) const {
  if (InsertBefore)
    ci->insertBefore(&CB);
  else if (InvokeInst* II = dyn_cast<InvokeInst>(&CB)) {
    ci->insertBefore(II->getNormalDest()->getFirstNonPHI());
  } else
    ci->insertAfter(&CB);
}

Function* IRHelper::cloneWithTrailingArg(Function &F, Type* T, const Twine& ARG_NAME) const {
  Module &M = *F.getParent();
  FunctionType *OT = F.getFunctionType();
  SmallVector<Type *> Ps(OT->param_begin(), OT->param_end());
  Ps.push_back(T);
  auto *NT = FunctionType::get(OT->getReturnType(), Ps, /*vararg*/false);

  Function *NF = Function::Create(NT, F.getLinkage(), F.getAddressSpace(), "", &M);
  NF->copyAttributesFrom(&F);          // CC (amdgpu_kernel), fn attrs, param attrs
  NF->setComdat(F.getComdat());
  SmallVector<std::pair<unsigned, MDNode *>> MDs;
  F.getAllMetadata(MDs);               // !dbg subprogram etc.
  for (auto &[K, N] : MDs) NF->addMetadata(K, *N);

  NF->takeName(&F);                    // keep the SAME symbol name
  NF->splice(NF->begin(), &F);         // move the body
  for (auto OA = F.arg_begin(), NA = NF->arg_begin(); OA != F.arg_end(); ++OA, ++NA) {
    NA->takeName(&*OA);
    OA->replaceAllUsesWith(&*NA);
  }
  NF->getArg(NF->arg_size() - 1u)->setName(ARG_NAME);
  return NF;                           // caller handles RAUW/erase
}

CallBase* IRHelper::addArgumentToCallBase(CallBase* CB, FunctionCallee Fn, Value* NewArg) const {
    SmallVector<Value*> Args(CB->arg_begin(), CB->arg_end());
    Args.push_back(NewArg);

    CallBase* NewCB = nullptr;
    if (auto II = dyn_cast<InvokeInst>(CB)) {
      NewCB = InvokeInst::Create(Fn, II->getNormalDest(), II->getUnwindDest(), Args, CB->getName(), CB);
    } else if (auto CI = dyn_cast<CallInst>(CB)) {
      NewCB = CallInst::Create(Fn, Args, CB->getName(), CB);
    }

    NewCB->setCallingConv(CB->getCallingConv());
    NewCB->setAttributes(CB->getAttributes());
    NewCB->copyMetadata(*CB);

    CB->replaceAllUsesWith(NewCB);
    CB->eraseFromParent();

    return NewCB;
}

bool ScabbardHostPassHip::runImpl(Module& M, ModuleAnalysisManager& MAM) {
  SmallVector<std::pair<Function*,CallBase*>> Stubs;
  Function* LaunchFn = M.getFunction("hipLaunchKernel");
  for (auto U : LaunchFn->users())
    if (auto* CB = dyn_cast<CallBase>(U)) {
      if (CB->getCalledFunction() != LaunchFn) continue;
      Function* F = CB->getFunction();
      if (F->getName().contains("__device_stub__"))
        Stubs.emplace_back(F,CB);
    }


  for (auto [Stub,Launch] : Stubs) {
      Function* NS = cloneWithTrailingArg(*Stub, DTTy, "scabbard.rtl.dt");
      
      auto* BufSlot = create_or_expand_param_args_alloc(*Launch,Launch->getArgOperand(5u)->stripPointerCasts());

      IRBuilder<> DTSB(BufSlot->getNextNode());
      DTSB.CreateStore(NS->getArg(NS->arg_size() - 1u), BufSlot);

      IRBuilder<> LB(Launch);
      auto tmp = ScabbardRTL.register_job_stub.getFunctionType()->getReturnType();
      auto* regCI = LB.CreateCall(ScabbardRTL.register_job_stub,{Launch->getArgOperand(7u),BufSlot});
      auto [locID, isNew] = ScabbardRTL.Metadata.insert(Launch);
      auto* cbCI = CallInst::Create(ScabbardRTL.register_job_callback,{regCI,Launch->getArgOperand(7u),locID});
      insertHandlingInvokes(cbCI,*Launch);
      setDebugLoc(cbCI,Launch);

      // RAUW Stub -> erase
      SmallVector<std::pair<CallBase*,Value*>> to_expand;
      for (Use& U : Stub->uses()) 
        if (auto *CB = dyn_cast<CallBase>(U.getUser())) {
          if (CB->isCallee(&U))
           to_expand.emplace_back(CB,NS);
        } else if (auto* GA = dyn_cast<GlobalAlias>(U.getUser())) {
          for (User* UR : GA->users())
            if (auto *CB = dyn_cast<CallBase>(UR)) {
              if (CB->isCallee(&U))
                to_expand.emplace_back(CB,GA);
            }
        }
      for (auto [CB, Fn] : to_expand) 
        addArgumentToCallBase(CB, FunctionCallee(NS->getFunctionType(),Fn), LB.getInt64(0u));
      Stub->replaceAllUsesWith(NS);
      Stub->eraseFromParent();
    }

  return Stubs.size() > 0u;
}

// << ================================= HOST PASS DEFINITIONS ================================== >> 

void IScabbardHostPass::registerRTL(Module& M) {
  if (M.getFunction("main") != nullptr)
    ScabbardRTL.scabbard_init = M.getOrInsertFunction(
        ScabbardRTL.scabbard_init_name,
        FunctionType::get(
            VoidTy,
            {}, // std::array<VoidTyType*,1ull>{VoidTy},
            false
          )
      );
  ScabbardRTL.scabbard_close = M.getOrInsertFunction(
      ScabbardRTL.scabbard_close_name,
      FunctionType::get(
          VoidTy,
          {}, // std::array<VoidTyType*,1ull>{VoidTy},
          false
        )
    );
  ScabbardRTL.trace_append = M.getOrInsertFunction(
      ScabbardRTL.trace_append_name,
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
  ScabbardRTL.trace_append$cond = M.getOrInsertFunction(
      ScabbardRTL.trace_append$cond_name,
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
  ScabbardRTL.trace_append$ext = M.getOrInsertFunction(
      ScabbardRTL.trace_append$ext_name,
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
  ScabbardRTL.trace_append$ext$cond = M.getOrInsertFunction(
      ScabbardRTL.trace_append$ext$cond_name,
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
  ScabbardRTL.register_job = M.getOrInsertFunction(
      ScabbardRTL.register_job_name,
      FunctionType::get(
          DTTy,//PtrTy,
          std::array<Type*,1ull>{PtrTy},
          false
        )
    );
  ScabbardRTL.register_job_stub = M.getOrInsertFunction(
      ScabbardRTL.register_job_stub_name,
      FunctionType::get(
          DTTy,//PtrTy,
          std::array<Type*,2ull>{PtrTy,PtrTy},
          false
        )
    );
  ScabbardRTL.register_job_callback = M.getOrInsertFunction(
      ScabbardRTL.register_job_callback_name,
      FunctionType::get(
          VoidTy,
          std::array<Type*,3ull>{
              PtrTy, PtrTy, LocDataTy
            },
          false
        )
    );
  ScabbardRTL.register_user_callback = M.getOrInsertFunction(
      ScabbardRTL.register_user_callback_name,
      FunctionType::get(
          u32Ty,
          std::array<Type*,5ull>{
              PtrTy, PtrTy, PtrTy, u32Ty, LocDataTy
            },
          false
        )
    );
  ScabbardRTL.register_user_hostFn_launch = M.getOrInsertFunction(
      ScabbardRTL.register_user_hostFn_launch_name,
      FunctionType::get(
          u32Ty,
          std::array<Type*,4ull>{
              PtrTy, PtrTy, PtrTy, LocDataTy
            },
          false
        )
    );
  ScabbardRTL.debug_print_ull = M.getOrInsertFunction(
      ScabbardRTL.debug_print_ull_name,
      FunctionType::get(
          VoidTy,
          std::array<Type*,1ull>{
              u64Ty
            },
          false
        )
    );
}

bool IsGlobalVarOnIgnoreList(const GlobalVariable* GV) {
  return (GV->hasAttribute(Attribute::AttrKind::DisableSanitizerInstrumentation)
            || GV->hasAttribute(Attribute::AttrKind::NoSanitizeCoverage));
          
}

inline void IScabbardHostPass::registerGlobalVarsInUnifiedMemory(const Module& M) {
  std::function<const Value*(const Value*)> get_next = [&](const Value* V) -> const llvm::Value* {
    if (const auto* CE = dyn_cast_or_null<ConstantExpr>(V)) {
      for (const auto& U : CE->operands()) {
        const Value* res = get_next(U.get());
        if (res != nullptr)
          return res;
      }
    } else if (const auto* GV = dyn_cast_or_null<GlobalValue>(V)) {
      return GV;
    }
    return nullptr;
  };
  auto checkFn = [&,this](const Function* Fn, const scabbard::InstrData HeapLoc) -> void {
    for (const auto _u : Fn->users())
      if (const auto* call = dyn_cast_or_null<CallInst>(_u)) {
        if (const auto* global = dyn_cast_or_null<GlobalVariable>(get_next(call->getArgOperand(1)))) {
          if (not IsGlobalVarOnIgnoreList(global))
            this->GlobalUnifiedMemVar.insert(std::make_pair(global->getName(), HeapLoc));
        }
      }
  };

  if (const auto* hFn = M.getFunction("hipHostRegister")) {
    checkFn(hFn, HOST_HEAP);
  }
  if (const auto* hFn = M.getFunction("hipHostMalloc")) {
    checkFn(hFn, HOST_HEAP);
  }
  if (const auto* hFn = M.getFunction("hipHostAlloc")) {
    checkFn(hFn, HOST_HEAP);
  }
  if (const auto* dFn = M.getFunction("__hipRegisterVar")) {
    checkFn(dFn, DEVICE_HEAP);
  }
  if (const auto* mFn = M.getFunction("__hipRegisterManagedVar")) {
    checkFn(mFn, MANAGED_MEM);
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

CallCheck_t BASE_CHECK = [](const Value& V, const CallInst& C) -> PtrOrigin {
  return (((&V) == throughConstExpr(C.getArgOperand(0u))) ? PtrOrigin::UNKNOWN_HEAP : PtrOrigin::NONE); // compare ptr's and hope llvm does not make copies of IR objects
};

CallCheck_t ALWAYS_HOST = [](const Value&, const CallInst&) -> PtrOrigin { return PtrOrigin::HOST_HEAP; };
CallCheck_t ALWAYS_DEVICE = [](const Value&, const CallInst&) -> PtrOrigin { return PtrOrigin::DEVICE_HEAP; };
CallCheck_t ALWAYS_MANAGED = [](const Value&, const CallInst&) -> PtrOrigin { return PtrOrigin::MANAGED_MEM; };
CallCheck_t ALWAYS_LOCAL = [](const Value&, const CallInst&) -> PtrOrigin { return PtrOrigin::LOCAL; };
CallCheck_t ALWAYS_UNKNOWN_HEAP = [](const Value&, const CallInst&) -> PtrOrigin { return PtrOrigin::UNKNOWN_HEAP; };


const StringMap<CallCheck_t> funcsOfInterest {
    { "hipMalloc", ALWAYS_DEVICE },
    {
      "hipMemcpy", 
      [](const Value& V, const CallInst& C) -> PtrOrigin {
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

IScabbardInstrPass::PtrOrigin IScabbardHostPass::_getPtrOriginHelper(const Value* Obj, const Value** Object, std::size_t max_level) const {
  using namespace HostPtrOriginHelpers;
  if (not max_level) return NONE;
  PtrOrigin ObjPO = NONE;
  bool is_runtime_cond = false;
  switch (Obj->getValueID()) {
    case Value::GlobalVariableVal: {
      GlobalVariable* GV = (GlobalVariable*) Obj;
      if (IsGlobalVarOnIgnoreList(GV))
        return NEVER;
      auto res = GlobalUnifiedMemVar.find(GV->getName());
      if (res == GlobalUnifiedMemVar.end()) {
        ObjPO = UNKNOWN_HEAP;
        is_runtime_cond = true;
        break;
      }
      ObjPO = res->second;
      *Object = Obj;
      break;
    }
    case Value::ArgumentVal:
      ObjPO = UNKNOWN_HEAP;
      is_runtime_cond = true;
      *Object = Obj;
      break;
    case Instruction::Load + Value::InstructionVal: {
        if (max_level == 0) {
          is_runtime_cond = true;
          ObjPO = UNKNOWN_HEAP;
          break;
        }
        const LoadInst* Load = (LoadInst*) Obj;
#       if (__llvm_major__ >= 17)
        const Value* LPtr = getUnderlyingObjectAggressiveLoad->getPointerOperand());
        #       else
        const Value* LPtr = getUnderlyingObject(Load->getPointerOperand());
#       endif
        const AllocaInst* alloca = findAllocaForValue(LPtr);
        PtrOrigin res = _getPtrOriginHelper(LPtr, Object, max_level-1);
        if (res != NONE) 
          ObjPO = res; 
        break;
      }
    case Instruction::Call + Value::InstructionVal: {
      CallInst* CI = (CallInst*) Obj;
      if (auto* Callee = CI->getCalledFunction()) {
        if (Callee->getName().starts_with("ompx_")) {
          if (Callee->getName().ends_with("_global"))
            ObjPO = DEVICE_HEAP;
          else if (Callee->getName().ends_with("_local"))
            ObjPO = LOCAL;
          else if (Callee->getName().ends_with("_shared"))
            ObjPO = MANAGED_MEM;
        }
        if (ObjPO != NONE)
          *Object = Obj;
      }
      break;
    }
    case Instruction::Alloca + Value::InstructionVal: {
      AllocaInst* AI = (AllocaInst*) Obj;
      bool any_of_interest = false;
      //check if this is used in a driver func of interest
      for (const auto U : AI->users())
        if (const CallInst* CI = llvm::dyn_cast_or_null<llvm::CallInst>(U)) {
          auto p = funcsOfInterest.find(CI->getCalledFunction()->getName());
          if (p != funcsOfInterest.end()) {
            PtrOrigin res = p->second(*Obj,*CI);
              if (res != NONE) {
                ObjPO = res;
                any_of_interest = true;
                break;
              }
            }
        }
      *Object = Obj;
      break;
    }
    default:
      break;
  }
  if (is_runtime_cond)
    ObjPO |= _RUNTIME_CONDITIONAL;
  return ObjPO;
}

IScabbardInstrPass::PtrOrigin IScabbardHostPass::getPtrOrigin(LoopInfo& LI, const Value* Ptr, const Value** Object) const {
  // derived from
  // https://github.com/jdoerfert/llvm-project/blob/b416d0c996bc01aeb6708c715bfe5e53bcac998d/llvm/lib/Transforms/Instrumentation/GPUSan.cpp#L592
  SmallVector<const Value*> Objects;
  getUnderlyingObjects(Ptr, Objects, &LI, 16u);
  if (Object && Objects.size() == 1)
    *Object = Objects.front();
  PtrOrigin PO = NONE;
  bool is_runtime_cond = false;
  for (auto* Obj : Objects) {
    PtrOrigin ObjPO = NONE;
    switch (Obj->getValueID()) {
      case Value::GlobalVariableVal: {
        const GlobalVariable* GV = (GlobalVariable*) Obj;
        if (IsGlobalVarOnIgnoreList(GV))
          return NEVER;
        auto res = GlobalUnifiedMemVar.find(GV->getName());
        if (res == GlobalUnifiedMemVar.end()) {
          ObjPO = UNKNOWN_HEAP;
          is_runtime_cond = true;
          break;
        }
        ObjPO = res->second;
        break;
      }
      case Value::ArgumentVal:
        ObjPO = UNKNOWN_HEAP;
        is_runtime_cond = true;
        break;
      case Instruction::Load + Value::InstructionVal: {
        const LoadInst* Load = (LoadInst*) Obj;
#       if (__llvm_major__ >= 17)
          const Value* LPtr = getUnderlyingObjectAggressiveLoad->getPointerOperand());
#       else
          const Value* LPtr = getUnderlyingObject(Load->getPointerOperand());
#       endif
        if (not LPtr) {
          ObjPO = UNKNOWN_HEAP;
          is_runtime_cond = true;
          break;
        }
        PtrOrigin res = _getPtrOriginHelper(LPtr, Object, 6u);
        if (res & _RUNTIME_CONDITIONAL) {
          res &= ~_RUNTIME_CONDITIONAL;
          is_runtime_cond = true;
        }
        if (res != NONE) 
          ObjPO = res; 
        break;
      }
      case Instruction::Call + Value::InstructionVal: {
        const CallInst* CI = (CallInst*) Obj;
        if (auto* Callee = CI->getCalledFunction()) {
          if (Callee->getName().starts_with("ompx_")) {
            if (Callee->getName().ends_with("_global"))
              ObjPO = DEVICE_HEAP;
            else if (Callee->getName().ends_with("_local"))
              ObjPO = LOCAL;
            else if (Callee->getName().ends_with("_shared"))
              ObjPO = MANAGED_MEM;
        }
       }
        break;
      }
      case Instruction::Alloca + Value::InstructionVal: 
        ObjPO = LOCAL;
        break;
      default:
        break;
    }
    if (PO == NONE || (PO == UNKNOWN_HEAP && ObjPO != NONE))
      PO = ObjPO;
  }
  if (is_runtime_cond)
    PO |= _RUNTIME_CONDITIONAL;
  return PO;
}

bool IScabbardHostPass::instrumentInScabbardFunc(LoopInfo& LI, Instruction& I, Value* Ptr, 
                                                   const InstrData InstrContext, Value* ExtraData) {
  Value* PtrOp = Ptr;
  const Value *Object = nullptr;
  PtrOrigin PO = getPtrOrigin(LI, PtrOp, &Object);

  if (PO == LOCAL)
    return false;

  if (PO == UNKNOWN_HEAP) // mark memory to be determined at runtime if it is in Unified Memory
    PO |= _RUNTIME_CONDITIONAL;

  auto [locID, is_inserted] = ScabbardRTL.Metadata.insert(&I);

  if (not is_inserted && locID == 0ull)
    errs() << "\n[scabbard.instr.host.metadata:WARN] Failed to insert instruction into the metadata system!"
              "\n[scabbard.instr.host.metadata:WARN]   -> make sure debug info is turned on (`-g`)\n";

  CallInst* ci = nullptr;
  if (ExtraData == nullptr)
    ci = CallInst::Create(
        (PO & UNKNOWN_HEAP) ? ScabbardRTL.trace_append$cond : ScabbardRTL.trace_append,
        std::array<Value*, 3ull>{
            ConstantInt::get(TraceDataTy, APInt(sizeof(InstrData)*8, InstrContext | PO)),
            Ptr,
            locID
          },
        "ScabbardRTL." + I.getName(), 
        /* insertBefore= */ &I
      );
  else 
    ci = CallInst::Create(
        (PO & UNKNOWN_HEAP) ? ScabbardRTL.trace_append$ext$cond : ScabbardRTL.trace_append$ext,
        std::array<Value*, 4ull>{
            ConstantInt::get(TraceDataTy, APInt(sizeof(InstrData)*8, InstrContext | PO)),
            Ptr,
            locID,
            ExtraData
          },
        "ScabbardRTL." + I.getName(), 
        /* insertBefore= */ &I
      );
  
  setDebugLoc(ci,I);
  
  return true;
}

CallInst* ScabbardHostPassHip::CreateRTLCall(CallBase& CB, const InstrData Data, 
                                             Value* Ptr, const bool InsertBefore) {
  auto [locID, is_inserted] = ScabbardRTL.Metadata.insert(&CB);
  if (not is_inserted && locID == 0ull)
    errs() << "\n[scabbard.instr.host.metadata.amdhip:WARN] Failed to insert instruction into the metadata system!"
              "\n[scabbard.instr.host.metadata.amdhip:WARN]   -> make sure debug info is turned on (`-g`)\n";
  auto ci = CallInst::Create(
        ScabbardRTL.trace_append,
        std::array<Value*, 3ull>{
            ConstantInt::get(TraceDataTy, APInt(sizeof(InstrData)*8, 
                              InstrData::ON_HOST | Data)),
            Ptr,
            locID
          },
        Twine("scabbard.") + (InsertBefore ? "pre." : "post.") + CB.getName()
      );
  insertHandlingInvokes(ci,CB,InsertBefore);
  setDebugLoc(ci,CB);
  return ci;
}

CallInst* ScabbardHostPassHip::CreateRTLCallEx(CallBase& CB, const InstrData Data, 
                                              Value* Ptr, Value* Extra, const bool InsertBefore) {
  auto [locID, is_inserted] = ScabbardRTL.Metadata.insert(&CB);
  if (not is_inserted && locID == 0ull)
    errs() << "\n[scabbard.instr.host.metadata.amdhip:WARN] Failed to insert instruction into the metadata system!"
              "\n[scabbard.instr.host.metadata.amdhip:WARN]   -> make sure debug info is turned on (`-g`)\n";
  auto ci = CallInst::Create(
        ScabbardRTL.trace_append$ext,
        std::array<Value*, 4ull>{
            ConstantInt::get(TraceDataTy, APInt(sizeof(InstrData)*8, 
                              InstrData::ON_HOST | Data | InstrData::_OPT_USED)),
            Ptr,
            locID,
            Extra
          },
        Twine("scabbard.") + (InsertBefore ? "pre." : "post.") + CB.getName()
      );
  if (ci == nullptr) return nullptr;
  insertHandlingInvokes(ci,CB,InsertBefore);
  setDebugLoc(ci,CB);
  return ci;
}


/// @brief Get the value that contains the mem address of the pointer pointer
///        (used to get the mem address of the ptr value of hipMalloc)
///       Not safe to use outside of specific use
Value* get_ptr_from_ptr(Value* V) {
  V = getUnderlyingObject(V); //NOTE: might change this to aggressive
  switch (V->getValueID()) {
  case Instruction::Alloca + Value::InstructionVal:
    return V;
  case Value::ArgumentVal:
    return (V->getType()->isPointerTy()) ? V : nullptr;
  case Value::ConstantExprVal: {
      const ConstantExpr* CE = (const ConstantExpr*) V;
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

bool ScabbardHostPassHip::APIInstr_Alloc(CallBase& CI, const InstrData Data) {
  if (auto ptr = get_ptr_from_ptr(CI.getArgOperand(0ull))) {
    auto _li = new LoadInst(PtrTy, ptr, "scabbard.instr.tmp."+CI.getName(),false, &CI);
    CallInst* ci = CreateRTLCallEx(CI, InstrData::ON_HOST | Data,
                                    _li, CI.getArgOperand(1ull), false);
    auto li = new LoadInst(PtrTy, ptr, "scabbard.instr.allocLoad."+CI.getName(), false, ci);
    _li->replaceAllUsesWith(li);
    _li->eraseFromParent();
    return true;
  }
  errs() << "\n[scabbard.instr.host.amdhip:ERR] could not backtrack `"<< CI.getCalledFunction()->getName() <<"` ptr parameter (" 
          << IScabbardHostPass::getLocStr(CI) << ")\n";  
  return false;
}

bool ScabbardHostPassHip::APIInstr_Memcpy(CallBase& CI, FunctionAnalysisManager& FAM, 
                                          const uint CopyDir, Value* Stream, bool IsAsync) {
  CallInst* SyncCall = nullptr;
  if (not IsAsync) // register memcpy as a sync event if it is not an Async call (inserted before )
    SyncCall = CreateRTLCall(CI, SYNC_EVENT, 
                             ((Stream) ? Stream : ConstantPointerNull::get(PtrTy)), false);
  const Value* ReadOrigin = nullptr, * WriteOrigin = nullptr;
  LoopInfo& LI = FAM.getResult<LoopAnalysis>(*CI.getFunction());
  InstrData ReadData = getPtrOrigin(LI, CI.getArgOperand(1ull), &ReadOrigin);
  InstrData WriteData = getPtrOrigin(LI, CI.getArgOperand(0ull), &WriteOrigin);
  // switch (CopyDir) { // not supper relevant as it could only help hint at heap type,
  //   case 0: // H->H  //  but managed memory can pretend to be either H or D so it is not helpful
  //   case 1: // H->D
  //   case 2: // D->H
  //   case 3: // D->D
  //   case 4: // default (driver decides what is happening)
  //   case 1024: // D->D no CU
  //   default: // Unknown value
  //     break;
  // }
  auto [locID, is_inserted] = ScabbardRTL.Metadata.insert(&CI);
  if (not is_inserted && locID == 0ull)
    errs() << "\n[scabbard.instr.host.metadata.amdhip:WARN] Failed to insert instruction into the metadata system!"
              "\n[scabbard.instr.host.metadata.amdhip:WARN]   -> make sure debug info is turned on (`-g`)\n";
  CallInst* ReadCall = nullptr, * WriteCall = nullptr;
  if (ReadData != InstrData::NO) {
    ReadCall  = CallInst::Create(
                  (ReadData & UNKNOWN_HEAP) ? ScabbardRTL.trace_append$ext$cond : ScabbardRTL.trace_append$ext,
                  std::array<Value*, 4ull>{
                      ConstantInt::get(TraceDataTy, APInt(sizeof(InstrData)*8, 
                                                          ReadData | ON_HOST | READ_EVENT | _OPT_DATA
                                                          | ((IsAsync) ? InstrData::ASYNC : InstrData::NONE))),
                      CI.getArgOperand(1ull),
                      locID,
                      CI.getArgOperand(2ull)
                    },
                  Twine("scabbard.") + CI.getName() + ".read"
                );
      insertHandlingInvokes(ReadCall,*((SyncCall) ? SyncCall : &CI));
      setDebugLoc(ReadCall,CI);
    }
  if (WriteData != InstrData::NO) {
    WriteCall = CallInst::Create(
                  (WriteData & UNKNOWN_HEAP) ? ScabbardRTL.trace_append$ext$cond : ScabbardRTL.trace_append$ext,
                  std::array<Value*, 4ull>{
                      ConstantInt::get(TraceDataTy, APInt(sizeof(InstrData)*8, 
                                                          WriteData | ON_HOST | WRITE_EVENT | _OPT_DATA
                                                          | ((IsAsync) ? InstrData::ASYNC : InstrData::NONE))),
                      CI.getArgOperand(0ull),
                      locID,
                      CI.getArgOperand(2ull)
                    },
                  Twine("scabbard.") + CI.getName() + ".write"
                );
    insertHandlingInvokes(WriteCall, *((ReadCall) ? ReadCall : ((SyncCall) ? SyncCall : &CI)));
    setDebugLoc(WriteCall,CI);
  }
  return SyncCall || ReadCall || WriteCall;
}

AllocaInst* ScabbardHostPassHip::expand_param_args_alloc(const CallBase& CB, AllocaInst& alloc) const {
  static std::size_t next_id = 0u;
  std::string id = std::to_string(next_id++);
  auto oldAllocTy = alloc.getAllocatedType();
  size_t old_size = 0ul;
  AllocaInst* NewAlloc = nullptr;
  if (auto arrTy = dyn_cast<ArrayType>(oldAllocTy)) { // case: alloc array type
    old_size = arrTy->getNumElements();
    NewAlloc = new AllocaInst(
                          ArrayType::get(PtrTy,old_size+1u),
                          0u,
                          ConstantInt::get(u32Ty, APInt(32u, 1u)),
                          Twine("scabbard.instr.kl.paramArrayAlloc.") + id,
                          &alloc
                        );
  } else if (auto C = dyn_cast<ConstantInt>(alloc.getArraySize())) { // case: built in alloc array concept
    old_size = C->getSExtValue();
    NewAlloc = new AllocaInst(
                          PtrTy,
                          0u,
                          ConstantInt::get(u32Ty, APInt(32u, old_size+1u)),
                          Twine("scabbard.instr.kl.paramArrayAlloc.") + id,
                          &alloc
                        );
  } else {  // case: unknown size 
    LLVM_DEBUG(errs() << "\n[scabbard.instr.host.amdhip:ERROR] could not determine size of alloc instr!\n";);
    old_size = 1;
  }
  auto PtrTy = PointerType::get(alloc.getContext(),0ul);
  // auto newAllocTy = ArrayType::get(PtrTy, old_size+1);
  NewAlloc->setAlignment(alloc.getAlign());
  setDebugLoc(NewAlloc,alloc); //might cause issues after alloc is deleted
  alloc.replaceAllUsesWith(NewAlloc); //DBG: does not seem to be working
  alloc.eraseFromParent();
  auto DTAlloc = new AllocaInst(
            DTTy, 0u, ConstantInt::get(u32Ty, APInt(32u, 1u)),
            "scabbard.instr.kl.DTAlloc."+id,
            NewAlloc
          );
  setDebugLoc(DTAlloc, CB);
  // find where to put the GEP and store instructions (After last Store instruction into NewAlloc)
  Instruction* InsertAfter = NewAlloc;
  for (auto U : NewAlloc->users())
    if (auto GEP = dyn_cast<GetElementPtrInst>(U))
      InsertAfter = GEP;
  if (InsertAfter != NewAlloc)
    for (auto U : InsertAfter->users())
      if (auto SI = dyn_cast<StoreInst>(U))
        InsertAfter = SI;
  
  auto DTParamPtr = GetElementPtrInst::CreateInBounds(
            u8Ty,
            NewAlloc, 
            std::array<Value*,1>{ConstantInt::get(u64Ty, APInt(64u, old_size*8u))},
            "scabbard.instr.kl.DTParamPtr."+id
          );
  setDebugLoc(DTParamPtr, CB);
  DTParamPtr->insertAfter(InsertAfter);
  auto DTAllocStore = new StoreInst(DTAlloc, DTParamPtr, DTParamPtr->getNextNonDebugInstruction());
  setDebugLoc(DTAllocStore, CB);
  return DTAlloc;
}

AllocaInst* ScabbardHostPassHip::create_or_expand_param_args_alloc(const CallBase& CB, Value* ARG) const {
  AllocaInst* dtAlloc = nullptr;
  if (auto argElmPtr = dyn_cast<GetElementPtrInst>(ARG)) { // case: >=2 function parameter length
    if (auto argAlloc = dyn_cast<AllocaInst>(argElmPtr->getPointerOperand())) {
      dtAlloc = expand_param_args_alloc(CB, *argAlloc);
    } else {
      errs() << "\n[scabbard.instr.host.amdhip:ERROR] kernel launch user args could not be traced to param args construct allocation\n";
    }
  } else if (auto argAlloc = dyn_cast<AllocaInst>(ARG)) { // case: single or zero function parameter length
    dtAlloc = expand_param_args_alloc(CB, *argAlloc);
  } else {
    errs() << "\n[scabbard.instr.host.amdhip:DBG] kernel launch user args are not loaded from local frame\n```\n";
    ARG->print(errs());
    errs() << "\n```\n\n";
  }
  if (dtAlloc == nullptr) {
    errs() << "\n[scabbard.instr.host.amdhip:ERROR] could not instrument kernel call (instrumentation failed)\n";
  }
  return dtAlloc;
}


bool ScabbardHostPassHip::APIInstr_LaunchKernel(CallBase& CB) {
  // instrument in `scabbard.trace.register_job` before this function and instrument in `scabbard.trace.register_job_callback` after this function call
  auto regFn = CallInst::Create(
      ScabbardRTL.register_job.getFunctionType(),
      ScabbardRTL.register_job.getCallee(),
      std::array<Value*,1ull>{CB.getArgOperand(7ull)},
      "",&CB
    );
  setDebugLoc(regFn,CB); //might cause issues if in a device stub
  auto [locID, is_inserted] = ScabbardRTL.Metadata.insert(&CB);
  if (not is_inserted && locID == 0ull)
    errs() << "\n[scabbard.instr.host.metadata.amdhip:WARN] Failed to insert instruction into the metadata system!"
              "\n[scabbard.instr.host.metadata.amdhip:WARN]   -> make sure debug info is turned on (`-g`)\n";
  // auto loc = ((CI.getDebugLoc()) // hip generated device stubs have no debug location data so must accommodate
  //               ? metadata.trace(F, CI.getDebugLoc(), ModuleType::HOST) 
  //               : MetadataHandler::get_hipAPI_loc());
  auto regCbFn = CallInst::Create(
      ScabbardRTL.register_job_callback.getFunctionType(),
      ScabbardRTL.register_job_callback.getCallee(),
      std::array<Value*,3ull>{
          regFn,
          CB.getArgOperand(7ull),
          locID,
        }
    );
  insertHandlingInvokes(regCbFn,CB);
  setDebugLoc(regCbFn,CB); //might cause issues if in a device stub
  //TODO? modify the type of the last operand (should be a global or function pass)
  // trace back args var and expand it to include the pointer to the DeviceTracker that is returned as the result of `scabbard.trace.register_job` as the last parameter
  AllocaInst* dtAlloc = create_or_expand_param_args_alloc(CB, CB.getArgOperand(5ull)->stripPointerCasts());
  auto dtStore = new StoreInst(regFn, dtAlloc, &CB);
  setDebugLoc(dtStore,CB); //might cause issues if in a device stub
  auto dbg_ci=CallInst::Create(ScabbardRTL.debug_print_ull,{regFn},"",&CB); setDebugLoc(dbg_ci,CB); //DEBUG
  // auto dtParamStore = new StoreInst(dtAlloc, dtAlloc, &CB);
  // setDebugLoc(dtParamStore,CB); //might cause issues if in a device stub
  return true;
}

Constant* ScabbardHostPassHip::getBestLocIDForUserCallback(CallBase& CB) {
  if (const auto* UsrFn = dyn_cast<Function>(CB.getArgOperand(1u))) {
    for (auto& i : instructions(*UsrFn))
      if (i.getDebugLoc())
        return ScabbardRTL.Metadata.insert(&i).first;
  }
  return ScabbardRTL.Metadata.insert(&CB).first;
}

bool ScabbardHostPassHip::APIInstr_HostCallback(CallBase& CB) {
  CallBase* cb = nullptr;
  if (auto _II = dyn_cast<InvokeInst>(&CB)) {
    cb = InvokeInst::Create(
            ScabbardRTL.register_user_callback,
            _II->getNormalDest(), _II->getUnwindDest(),
            std::array<Value*,5u>{CB.getArgOperand(0u), CB.getArgOperand(1u), 
                                  CB.getArgOperand(2u), CB.getArgOperand(3u),
                                  getBestLocIDForUserCallback(CB)},
            Twine("scabbard.instr.usrcallbackdata.0")+CB.getName(),
            &CB
          );
  } else
    cb = CallInst::Create(
            ScabbardRTL.register_user_callback.getFunctionType(),
            ScabbardRTL.register_user_callback.getCallee(),
            std::array<Value*,5u>{CB.getArgOperand(0u), CB.getArgOperand(1u), 
                                  CB.getArgOperand(2u), CB.getArgOperand(3u),
                                  getBestLocIDForUserCallback(CB)},
            Twine("scabbard.instr.usrcallbackdata.0")+CB.getName(),
            &CB
          );
  CB.replaceAllUsesWith(cb);
  setDebugLoc(cb,CB);
  CB.eraseFromParent();
  return true;
}

bool ScabbardHostPassHip::APIInstr_HostFnLaunch(CallBase& CB) {
  CallBase* cb = nullptr;
  if (auto _II = dyn_cast<InvokeInst>(&CB)) {
    cb = InvokeInst::Create(
            ScabbardRTL.register_user_hostFn_launch,
            _II->getNormalDest(), _II->getUnwindDest(),
            std::array<Value*,4u>{CB.getArgOperand(0u),
                                  CB.getArgOperand(1u), 
                                  CB.getArgOperand(2u),
                                  getBestLocIDForUserCallback(CB)},
            Twine("scabbard.instr.usrHostFnLaunch.0")+CB.getName(),
            &CB
          );
  } else
    cb = CallInst::Create(
            ScabbardRTL.register_user_hostFn_launch.getFunctionType(),
            ScabbardRTL.register_user_hostFn_launch.getCallee(),
            std::array<Value*,4u>{CB.getArgOperand(0u),
                                  CB.getArgOperand(1u), 
                                  CB.getArgOperand(2u),
                                  getBestLocIDForUserCallback(CB)},
            Twine("scabbard.instr.usrHostFnLaunch.0")+CB.getName(),
            &CB
          );
  CB.replaceAllUsesWith(cb);
  setDebugLoc(cb,CB);
  CB.eraseFromParent();
  return true;
}


bool ScabbardHostPassHip::APIInstr_Unsupported(const CallInst& CB, const StringRef APIName) const {
  errs() << "\n[scabbard.instr.host.amdhip:WARN] WARNING: Scabbard does not support the following HIP API: `" 
         << APIName << "`"
            "\n[scabbard.instr.host.amdhip:WARN]          -> Scabbard's Results may be invalid! "
            "\n[scabbard.instr.host.amdhip:WARN]  SrcLoc: "
         << IScabbardHostPass::getLocStr(CB) << "\n";
  return false;
}



void ScabbardHostPassHip::registerAPIInstrumenters() { //TODO: change CallInst to CallBase and handle invoke paths
  // APIInstrumenters = /* (std::vector<std::pair<const std::string,APIInstrumenterFn_t>>) */{
  APIInstrumenters = {
    {
      "hipStreamSynchronize",
      [this](CallBase& CB, FunctionAnalysisManager& FAM) -> bool {
        return CreateRTLCall(CB, InstrData::SYNC_EVENT, 
                             CB.getArgOperand(0ull), false) ? true : false;
      }
    },
    {
      "hipDeviceSynchronize", 
      [this](CallBase& CB, FunctionAnalysisManager& FAM) -> bool { 
        return CreateRTLCall(CB, InstrData::SYNC_EVENT,
                             ConstantPointerNull::get(PtrTy), false) ? true : false;
      }
    },
    {
      "hipMalloc", 
      [this](CallBase& CB, FunctionAnalysisManager& FAM) -> bool { return APIInstr_Alloc(CB,ALLOCATE|DEVICE_HEAP); }
    },
    {
      "hipExtMallocWithFlags", 
      [this](CallBase& CB, FunctionAnalysisManager& FAM) -> bool { return APIInstr_Alloc(CB,ALLOCATE|DEVICE_HEAP); }
    },
    {
      "hipHostAlloc", 
      [this](CallBase& CB, FunctionAnalysisManager& FAM) -> bool { return APIInstr_Alloc(CB,ALLOCATE|HOST_HEAP); }
    },
    {
      "hipHostMalloc", 
      [this](CallBase& CB, FunctionAnalysisManager& FAM) -> bool { return APIInstr_Alloc(CB,ALLOCATE|HOST_HEAP); }
    },
    {
      "hipMallocManaged", 
      [this](CallBase& CB, FunctionAnalysisManager& FAM) -> bool { return APIInstr_Alloc(CB,ALLOCATE|MANAGED_MEM); }
    },
    {
      "hipHostRegister", 
      [this](CallBase& CB, FunctionAnalysisManager& FAM) -> bool {
        return CreateRTLCallEx(CB, InstrData::ALLOCATE | InstrData::HOST_HEAP,
                              CB.getArgOperand(0ull), CB.getArgOperand(1ull), false) ? true : false;
      }
    },
    {
      "hipFree",
      [this](CallBase& CB, FunctionAnalysisManager& FAM) -> bool {
        return CreateRTLCall(CB, InstrData::FREE_EVENT | InstrData::UNKNOWN_HEAP,
                              CB.getArgOperand(0ull), true) ? true : false;
      }
    },
    {
      "hipFreeHost",
      [this](CallBase& CB, FunctionAnalysisManager& FAM) -> bool {
        return CreateRTLCall(CB, InstrData::FREE_EVENT | InstrData::HOST_HEAP,
                              CB.getArgOperand(0ull), true) ? true : false;
      }
    },
    {
      "hipMemcpy",
      [this](CallBase& CB, FunctionAnalysisManager& FAM) -> bool {
        if (auto* CpyDir = dyn_cast<ConstantInt>(CB.getArgOperand(3ull))) {
          return APIInstr_Memcpy(CB, FAM, CpyDir->getSExtValue()); 
        }
        errs() << "\n[scabbard.instr.host.amdhip:WARN] hipMemcpyKind was not a const value"
                  " --unsupported by scabbard-- ("
               << IScabbardHostPass::getLocStr(CB.getArgOperand(3ull)) << ")\n";
        return false;
      }
    },
    {
      "hipMemcpyWithStream",
      [this](CallBase& CB, FunctionAnalysisManager& FAM) -> bool {
        if (auto* CpyDir = dyn_cast<ConstantInt>(CB.getArgOperand(3ull))) {
          return APIInstr_Memcpy(CB, FAM, CpyDir->getSExtValue(), CB.getArgOperand(4ull)); 
        }
        errs() << "\n[scabbard.instr.host.amdhip:WARN] hipMemcpyKind was not a const value"
                  " --unsupported by scabbard-- ("
               << IScabbardHostPass::getLocStr(CB.getArgOperand(3ull)) << ")\n";
        return false;
      }
    },
    {
      "hipMemcpyHtoD",
      [this](CallBase& CB, FunctionAnalysisManager& FAM) -> bool { return APIInstr_Memcpy(CB,FAM,1u); }
    },
    {
      "hipMemcpyDtoH",
      [this](CallBase& CB, FunctionAnalysisManager& FAM) -> bool { return APIInstr_Memcpy(CB,FAM,2u); }
    },
    {
      "hipMemcpyDtoD",
      [this](CallBase& CB, FunctionAnalysisManager& FAM) -> bool { return APIInstr_Memcpy(CB,FAM,3u); }
    },
    {
      "hipMemcpyAsync",
      [this](CallBase& CB, FunctionAnalysisManager& FAM) -> bool {
        if (auto* CpyDir = dyn_cast<ConstantInt>(CB.getArgOperand(3ull))) {
          return APIInstr_Memcpy(CB,FAM, CpyDir->getSExtValue(), CB.getArgOperand(4ull), true);
        }
        errs() << "\n[scabbard.instr.host.amdhip:WARN] hipMemcpyKind was not a const value"
                  " --unsupported by scabbard-- ("
               << IScabbardHostPass::getLocStr(CB.getArgOperand(3ull)) << ")\n";
        return false;
      }
    },
    {
      "hipMemcpyHtoDAsync",
      [this](CallBase& CB, FunctionAnalysisManager& FAM) -> bool { 
        return APIInstr_Memcpy(CB,FAM,1u,CB.getArgOperand(3ull),true); 
      }
    },
    {
      "hipMemcpyDtoHAsync",
      [this](CallBase& CB, FunctionAnalysisManager& FAM) -> bool { 
        return APIInstr_Memcpy(CB,FAM,2u,CB.getArgOperand(3ull),true); 
      }
    },
    {
      "hipMemcpyDtoDAsync",
      [this](CallBase& CB, FunctionAnalysisManager& FAM) -> bool { 
        return APIInstr_Memcpy(CB,FAM,3u,CB.getArgOperand(3ull),true); 
      }
    },
    {
      "hipLaunchKernel",
      [this](CallBase& CB, FunctionAnalysisManager& FAM) -> bool { return APIInstr_LaunchKernel(CB); }
    },
    {
      "hipExtLaunchKernel",
      [this](CallBase& CB, FunctionAnalysisManager& FAM) -> bool { return APIInstr_LaunchKernel(CB); }
    },
    {
      "hipStreamAddCallback",
      [this](CallBase& CB, FunctionAnalysisManager& FAM) -> bool { return APIInstr_HostCallback(CB); }
    },
    {
      "hipLaunchHostFunc",
      [this](CallBase& CB, FunctionAnalysisManager& FAM) -> bool { return APIInstr_HostFnLaunch(CB); }
    }
  };
  // APIInstrumenters = _APIInstrumenters;
}


// << ================================== DEVICE HELPER CODE ==================================== >> 

void IScabbardDevicePass::registerRTL(Module& M) {
  ScabbardRTL.trace_append = M.getOrInsertFunction(
      ScabbardRTL.trace_append_name,
      FunctionType::get(
          VoidTy,
          std::array<Type*,4u>{
              DTTy,
              TraceDataTy,
              PtrTy,
              LocDataTy
            },
          false
        )
    );
  ScabbardRTL.trace_append$ext = M.getOrInsertFunction(
      ScabbardRTL.trace_append$ext_name,
      FunctionType::get(
          VoidTy,
          std::array<Type*,5u>{
              DTTy,
              TraceDataTy,
              PtrTy,
              LocDataTy,
              ExtraDataTy
            },
          false
        )
    );
  ScabbardRTL.dbg_print_ull = M.getOrInsertFunction(
      ScabbardRTL.dbg_print_ull_name,
      FunctionType::get(
          VoidTy,
          std::array<Type*,1u>{u64Ty},
          false
        )
    );
}

bool IScabbardDevicePass::runImpl(Function& F, FunctionAnalysisManager& FAM) {
  bool changed = false;
  LoopInfo& LI = FAM.getResult<LoopAnalysis>(F);
  if (F.getName().contains("force_kernel")) {auto ii=&*F.getEntryBlock().getFirstNonPHIOrDbgOrAlloca();IRBuilder<> IRB(ii);auto dbg_ci=CallInst::Create(ScabbardRTL.dbg_print_ull,{IRB.getInt64(F.arg_size())},"",ii); setDebugLoc(dbg_ci,ii);}//DEBUG
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

  //setDebugLoc(CallInst::Create(ScabbardRTL.dbg_print_ull,std::array<Value*,1u>{F.getArg(F.arg_size()-1u)},"",F.getEntryBlock().getFirstNonPHIOrDbgOrLifetime()),F.getEntryBlock().getFirstNonPHIOrDbgOrLifetime()); //DEBUG


  return changed;
}

inline bool IScabbardDevicePass::expandFnParams(Module& M) const {
  // expand the signatures of all relevant fn's
  // (this requires cloning and RAUW since llvm ir fn sigs are immutable)
  SmallVector<Function*> to_expand;
  for (auto& Fn : M)
    if (isInstrumentableFn(Fn)) // only expand instrumentable fn's
      to_expand.push_back(&Fn);
  // SmallVector<std::pair<Function*, Function*>> to_replace;
  for (auto* OldFn : to_expand) {
    auto NewFn = cloneWithTrailingArg(*OldFn, DTTy, "scabbard.rtl.dt");

    SmallVector<std::tuple<CallBase*,Value*,Argument*>> cb_to_replace;
    for (auto& U : OldFn->uses())
      if (auto CB = dyn_cast<CallBase>(U.getUser())) {
        auto Caller = CB->getCaller();
        if (CB->isCallee(&U))
          cb_to_replace.emplace_back(CB,NewFn,Caller->getArg(Caller->arg_size()-1u));
      } else if (auto GA = dyn_cast<GlobalAlias>(U.getUser())) {
        for (auto& _U : GA->uses())
          if (auto CB = dyn_cast<CallBase>(_U.getUser())) {
            auto Caller = CB->getCaller();
            if (CB->isCallee(&_U))
              cb_to_replace.emplace_back(CB,GA,Caller->getArg(Caller->arg_size()-1u));
          }
      }

    for (auto [CB, TargFn, Arg] : cb_to_replace)
      addArgumentToCallBase(CB, FunctionCallee(NewFn->getFunctionType(), TargFn), Arg);

    OldFn->replaceAllUsesWith(NewFn);
    OldFn->eraseFromParent();
  }
  // return if changes were made
  return to_expand.size() > 0u;
}

IScabbardDevicePass::PtrOrigin IScabbardDevicePass::getPtrOrigin(LoopInfo& LI, Value* Ptr, const Value** Object) const {
  // derived from
  // https://github.com/jdoerfert/llvm-project/blob/b416d0c996bc01aeb6708c715bfe5e53bcac998d/llvm/lib/Transforms/Instrumentation/GPUSan.cpp#L592
  SmallVector<const Value*> Objects;
  getUnderlyingObjects(Ptr, Objects, &LI);
  if (Object && Objects.size() == 1)
    *Object = Objects.front();
  PtrOrigin PO = NONE;
  for (auto* Obj : Objects) {
    PtrOrigin ObjPO = HasAllocas ? LOCAL : UNKNOWN_HEAP;
    switch (Obj->getValueID()) {
      case Value::GlobalVariableVal:
        if (IsGlobalVarOnIgnoreList((GlobalVariable*)Obj))
          break; // maybe set to never and return instead?
        ObjPO = DEVICE_HEAP;
        break;
      case Value::ArgumentVal: {
        Argument* Arg = (Argument*) Obj;
        if (Arg->getType()->isPointerTy())
          ObjPO = UNKNOWN_HEAP;
        break;
      }
      case Instruction::Alloca + Value::InstructionVal:
        ObjPO = LOCAL;
        break;
      case Instruction::Load + Value::InstructionVal: {
        LoadInst* Load = (LoadInst*) Obj;
        if (auto* Global = dyn_cast<GlobalVariable>(Load->getPointerOperand())) {
          if (IsGlobalVarOnIgnoreList(Global))
            break; // maybe set to never and return instead?
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
        // if (auto* II = dyn_cast<IntrinsicInst>(CI)) { // check if call to intrinsic fn
        //   if (II->getIntrinsicID() == Intrinsic::amdgcn_implicitarg_ptr ||
        //     II->getIntrinsicID() == Intrinsic::amdgcn_dispatch_ptr)
        //   return NEVER; //SYSTEM;
        // }
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
    if (PO == NONE || (PO <= UNKNOWN_HEAP && ObjPO >= UNKNOWN_HEAP))
      PO = ObjPO;
  }
  return PO;
}

bool IScabbardDevicePass::instrumentInScabbardFunc(LoopInfo& LI, Instruction& I, Value* Ptr, 
                                                   FunctionCallee& ScabbardFn, const InstrData InstrContext) {
    Value* PtrOp = Ptr;
    PointerType* ptrTy = cast<PointerType>(Ptr->getType());
    std::uint32_t ptrTyAddrSp = ptrTy->getAddressSpace();
    if (not (ptrTyAddrSp <= 1u        //PTR in: Generic (0), Global (1)
              || ptrTyAddrSp == 4u))  //        OR Constant (4) address-space to even be considered
      return false;                   //        these are the addresspaces that can be in unified memory
    const Value *Object = nullptr;
    PtrOrigin PO = getPtrOrigin(LI, PtrOp, &Object);

    if (PO == LOCAL) // don't instrument if it is not accessible outside of the GPU.
      return false;

    auto [locID, is_inserted] = ScabbardRTL.Metadata.insert(&I);

    if (not is_inserted && locID == 0ull)
      errs() << "\n[scabbard.instr.device.AMD.I: ERROR] failed to insert instruction into the metadata system!\n";

    // if Ptr is not in the expected address space (0: global) we will need to cast it's Addrspace first.
    //    This is only a LLVM formality and does not cost any compute at runtime.
    llvm::CastInst* castInst = nullptr;
    if (ptrTyAddrSp != 0u) //NOTE: hard coding this might be an issue
      castInst = AddrSpaceCastInst::Create(llvm::Instruction::CastOps::AddrSpaceCast, Ptr, 
                                            ScabbardFn.getFunctionType()->getParamType(2u),
                                          "scabbard.addrCast." + I.getName(),  /* insertBefore= */ &I);

    Function& fn = *I.getFunction();
    Value* kernelDeviceTracker = fn.getArg(fn.arg_size() - 1ul);
    auto ci = CallInst::Create(
                  ScabbardFn,
                  std::array<Value*, 4u>{
                      kernelDeviceTracker,
                      ConstantInt::get(TraceDataTy, APInt(sizeof(InstrData)*8, InstrContext | PO)),
                      (castInst ? castInst : Ptr),
                      locID
                    },
                  "scabbard." + I.getName(), 
                  /* insertBefore= */ &I
                );
    setDebugLoc(ci,I);
    return true;
  }

// << ========================== Metadata Handler Extra Definitions ============================ >>

inline std::pair<Constant*, bool> MetadataHandler::insert(const Instruction* I) {
  auto [locID, is_new] = _insert(I);
  return std::make_pair(getGEP(MetadataVar,locID), is_new);
}

inline std::pair<uint64_t, bool> MetadataHandler::_insert(const Instruction* I) {
  auto res = Instructions.insert(std::make_pair(I, Instructions.size()));
  return std::make_pair(res.first->second, res.second);
}

Constant* MetadataHandler::getGEP(GlobalVariable* GV, size_t Index) const {
  return ConstantExpr::getGetElementPtr(GV->getValueType(), GV, //NOTE: replace inbounds if necessary
                        std::array<Constant*,2u>{
                          ConstantInt::get(EntryTy->getTypeAtIndex(2u), //u64
                                            APInt(64u, 0ull)),  //index through global var ptr
                          ConstantInt::get(EntryTy->getTypeAtIndex(2u), //u64
                                            APInt(64u, Index))  //index into the array
                        }, /*isInbounds=*/false);
}

// inline Constant* MetadataHandler::getId(const Instruction* I) {
//   auto res = Instructions.insert(std::make_pair(I, Instructions.size()));
//   if (not res.second && res.first != Instructions.end()) // case failed to insert
//     return nullptr;
//   return Constant::getIntegerValue(Type::getInt64Ty(I->getContext()), APInt(res.first->second, 64ul));
// }
void x64(const std::string& input, char* output) {
  const char C_SAFE_BASE64_MAP[66u] = 
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789$__";
  std::uint64_t value = std::hash<std::string>{}(input);
  for (std::size_t i = 0u; i < 11u; i++) {
    // Extract 6 bits at a time starting from the least significant bits
    uint8_t index = (uint8_t)(value & 0x3Fu);
    output[10u - i] = C_SAFE_BASE64_MAP[index];
    value >>= 6u;
  }
  output[12u] = '\0';
}

GlobalVariable* MetadataHandler::initializeMetadata(Module& M, unsigned AddrSpace, IRHelper* IRH) {
  EntryTy = StructType::create(std::array<Type*,5>{
                                  IRH->u64Ty, IRH->PtrTy, IRH->PtrTy, IRH->u64Ty, IRH->u64Ty
                                }, "scabbard.metadata.entryTy", false);
  const auto ArrTy = ArrayType::get(EntryTy, UINT32_MAX); // temp type
  const auto Arr = ConstantArray::get(ArrTy, {});         // temp contents
  MetadataVar = new GlobalVariable(M, cast<Type>(ArrTy), /*IsConstant=*/true, GlobalValue::ExternalLinkage, Arr,
                                    "scabbard.metadata.tmp", nullptr, GlobalValue::NotThreadLocal, AddrSpace);
  MetadataVar->addAttribute(Attribute::AttrKind::DisableSanitizerInstrumentation);
  MetadataVar->addAttribute(Attribute::AttrKind::NoSanitizeCoverage);
  return MetadataVar;
}

void MetadataHandler::finalize(Module& M) {
  char _srcHash64[16u] = "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
  x64(M.getSourceFileName(),_srcHash64);
  std::string srcHash64(_srcHash64);
  SmallVector<Constant*, 16ul> Contents(Instructions.size());
  auto arr = ConstantDataArray::getString(M.getContext(), "<unknownFile>:<unknownFn>");
  auto stringsVar = new GlobalVariable(M, ArrayType::get(IntegerType::get(M.getContext(),8u), UINT32_MAX), 
                                        /*IsConstant=*/true, GlobalValue::ExternalLinkage,
                                        arr, "scabbard.metadata.strings.tmp", nullptr, GlobalValue::NotThreadLocal, 
                                        MetadataVar->getAddressSpace());
  for (const auto& [I, ID] : Instructions) {
    const DebugLoc& dLoc = I->getDebugLoc();
    const DILocalScope* dScope = (dLoc) ? dLoc->getScope() : nullptr;
    if (dScope) {
      const DISubprogram* dSubPro = dScope->getSubprogram();
      if (dSubPro) {
        const DIFile* dFile = dScope->getFile();
        if (dFile) {
          Contents[ID] = ConstantStruct::get(
                          EntryTy, {getLazyID(dFile, dLoc.get(), M),
                                    getGEP(stringsVar, getSourceFile(dFile)), 
                                    getGEP(stringsVar, getCalledFn(dSubPro->getName())),
                                    getSourceLine(dLoc.get(), M.getContext()), getSourceCol(dLoc.get(), M.getContext())});
        } else
          Contents[ID] = ConstantStruct::get(
                          EntryTy, {getLazyID(nullptr, dLoc.get(), M),
                                    getGEP(stringsVar, registerString(Twine("<UNKNOWN_SRC_FILE>{LLVM_IR_Module=\"")
                                                                      + M.getSourceFileName() + "\"}")), 
                                    getGEP(stringsVar, getCalledFn(dSubPro->getName())),
                                    getSourceLine(dLoc.get(), M.getContext()), getSourceCol(dLoc.get(), M.getContext())});
        
      } else
        Contents[ID] = ConstantStruct::get(
                          EntryTy, {getLazyID(nullptr, dLoc.get(), M),
                                    getGEP(stringsVar, registerString(Twine("<UNKNOWN_SRC_FILE>{LLVM_IR_Module=\"")
                                                                      + M.getSourceFileName() + "\"}")), 
                                    getGEP(stringsVar, registerString(Twine("<UNKNOWN_SRC_FN>{LLVM_IR_SCOPE=\"")
                                                                        + dScope->getName() + "\"}")),
                                    getSourceLine(dLoc.get(), M.getContext()), getSourceCol(dLoc.get(), M.getContext())});
    } else 
      Contents[ID] = ConstantStruct::get(
                      EntryTy, {getLazyID(nullptr, nullptr, M),
                                getGEP(stringsVar, registerString(Twine("<UNKNOWN_SRC_FILE>{Module=\"")
                                                                  + M.getSourceFileName() + "\"}")), 
                                getGEP(stringsVar, registerString(Twine("<UNKNOWN_SRC_FN>{LLVM_IR_Fn=\"")
                                                                    + I->getFunction()->getName() + "\"}")),
                                Constant::getIntegerValue(EntryTy->getTypeAtIndex(3ul), APInt(64ul, 0ul)), 
                                Constant::getIntegerValue(EntryTy->getTypeAtIndex(4ul), APInt(64ul, 0ul))});
  }

  auto _strVar = outputStrings(M, MetadataVar->getAddressSpace(), srcHash64);
  stringsVar->replaceAllUsesWith(_strVar);  // replace with completed variable
  stringsVar->eraseFromParent();            // cleanup old temp
  
  const auto ArrTy = ArrayType::get(EntryTy, Instructions.size());
  const auto Arr = ConstantArray::get(ArrTy, Contents);
  auto _MetadataVar = new GlobalVariable(M, cast<Type>(ArrTy), /*IsConstant=*/true,  GlobalValue::ExternalLinkage, //GlobalValue::AvailableExternallyLinkage,
                                          Arr, Twine("scabbard.metadata.")+srcHash64, nullptr,
                                          GlobalValue::NotThreadLocal, MetadataVar->getAddressSpace());
  MetadataVar->replaceAllUsesWith(_MetadataVar);  // replace with completed version
  MetadataVar->eraseFromParent();                // cleanup old temp
  _MetadataVar->addAttribute(Attribute::AttrKind::DisableSanitizerInstrumentation);
  _MetadataVar->addAttribute(Attribute::AttrKind::NoSanitizeCoverage);
  _strVar->addAttribute(Attribute::AttrKind::DisableSanitizerInstrumentation);
  _strVar->addAttribute(Attribute::AttrKind::NoSanitizeCoverage);
  appendToUsed(M, {_MetadataVar,_strVar});       // register them as used variables
}

uint64_t MetadataHandler::registerString(const Twine& Str) {
  const auto& It = UniqueStrings.insert({Str.str(), ConcatenatedString.size()});
  if (It.second) {
    ConcatenatedString += Str.str();
    ConcatenatedString.push_back('\0');
  }
  return It.first->second;
}

GlobalVariable* MetadataHandler::outputStrings(Module& M, unsigned AddrSpace, const std::string& srcHash64) const {
  auto Arr = ConstantDataArray::getString(M.getContext(), ConcatenatedString);
  return new GlobalVariable(M, cast<Type>(Arr->getType()), /*IsConstant=*/true, GlobalValue::ExternalLinkage, Arr,
                            Twine("scabbard.metadata.strings.")+srcHash64, 
                            nullptr, GlobalValue::NotThreadLocal, AddrSpace);
}

inline size_t MetadataHandler::getSourceFile(const DIFile* File/* , LLVMContext& C */) {
  auto dir = File->getDirectory();
  auto localPath = File->getFilename();

  // size_t offset = 
  return registerString((dir.ends_with("/")) ? dir + localPath : dir + "/" + localPath);

  // return cast<ConstantInt>(Constant::getIntegerValue(IntegerType::get(C,64ul), APInt(64ul,offset)));
}

inline ConstantInt* MetadataHandler::getSourceLine(const DILocation* Loc, LLVMContext& C) const {
  return cast<ConstantInt>(Constant::getIntegerValue(EntryTy->getTypeAtIndex(3u), APInt(64ul,Loc->getLine())));
}

inline ConstantInt* MetadataHandler::getSourceCol(const DILocation* Loc, LLVMContext& C) const {
  return cast<ConstantInt>(Constant::getIntegerValue(EntryTy->getTypeAtIndex(4u), APInt(64ul,Loc->getColumn())));
}

inline ConstantInt* MetadataHandler::getLazyID(const DIFile* DFile, const DILocation* DLoc, Module& M) const {
  auto FilePath = ((DFile)
                    ? ((DFile->getDirectory().ends_with("/")) 
                        ? DFile->getDirectory() + DFile->getFilename() 
                        : DFile->getDirectory() + "/" + DFile->getFilename())
                    : Twine("<UNKNOWN_SRC_FILE>{Module=\"") + M.getSourceFileName() + "\"}") + "\0";
  std::size_t line = 0ul, col = 0ul;
  if (DLoc) {
    line = DLoc->getLine();
    col = DLoc->getColumn();
  }
  return cast<ConstantInt>(Constant::getIntegerValue(EntryTy->getTypeAtIndex(0u), 
                                                      APInt(64ul, SrcMetadata::Hash{}(FilePath.str(),line,col))));
}

inline size_t MetadataHandler::getCalledFn(const StringRef& FnName/* , LLVMContext& C */) {
  // uint64_t offset = 
  return registerString(FnName);
  // return cast<Constant>(Constant::getIntegerValue(IntegerType::get(C,64ul), APInt(64ul,offset)));
}

void MetadataHandler::clean() {
  MetadataVar->eraseFromParent();
  EntryTy->setName(""); // clearing name is closest thing to removing a type without recalculating the context.
}

inline raw_ostream& operator << (raw_ostream& out, const scabbard::InstrData& data) {
  std::bitset<16> bs(data);
    return (out << std::string((data & InstrData::_RUNTIME_CONDITIONAL) ? "RT_COND, " : "")
                << std::string((data & InstrData::ON_DEVICE) ? "ON_DEVICE, " : "")
                << std::string((data & InstrData::ON_HOST) ? "ON_HOST, " : "")
                << std::string((data & InstrData::UNKNOWN_HEAP) ? "UNKNOWN_HEAP, " : "")
                << std::string((data & InstrData::DEVICE_HEAP) ? "DEVICE_HEAP, " : "")
                << std::string((data & InstrData::HOST_HEAP) ? "HOST_HEAP, " : "")
                << std::string((data & InstrData::ATOMIC) ? "ATOMIC, " : "")
                << std::string((data & InstrData::MANAGED_MEM) ? "MANAGED_MEM, " : "")
                << std::string((data & InstrData::READ) ? "READ, " : "")
                << std::string((data & InstrData::WRITE) ? "WRITE, " : "")
                << std::string((data & InstrData::ALLOCATE) ? "ALLOCATE, " : "")
                << std::string((data & InstrData::FREE) ? "FREE, " : "")
                << std::string((data & InstrData::LAUNCH_EVENT) ? "KERNEL_LAUNCH, " : "")
                << std::string((data & InstrData::SYNC_EVENT) ? "SYNC_EVENT, " : "")
                << std::string((data & InstrData::FREE) ? "FREE, " : "")
                << std::string((data & InstrData::_OPT_USED) ? "OPT_DATA, " : "")
                << std::string((data & InstrData::ASYNC) ? "ASYNC_OP, " : ""));
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
    changed = ScabbardAMDDevicePass(M).run(M, MAM);
  } else if (not IS_LTO and not target.isAMDGPU()) { // most likely the host arch
    changed = ScabbardHostPassHip(M, target).run(M, MAM);
  }
  if (changed)
    return PreservedAnalyses::none(); // this will have to change after transforms are performed
  return PreservedAnalyses::all();
}

} // namespace llvm

#undef DEBUG_TYPE // "scabbard"