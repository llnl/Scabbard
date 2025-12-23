/**
 * @file DepTrace_HOST.cpp
 * @author osterhoutan (osterhoutan+scabbard@gmail.com)
 * @brief A object that is meant to allow the specific dep trace that scabbard requires to occur
 * @version alpha 0.0.1
 * @date 2023-05-16
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "DepTrace.hpp"

#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/Casting.h>
#include <llvm/IR/Constants.h>

#include <unordered_map>

namespace scabbard {
  namespace instr {

    const llvm::Value* throughConstExpr(const llvm::Value* V) {
      if (const auto* CE = llvm::dyn_cast_or_null<llvm::ConstantExpr>(V)) {
        for (const auto& U : CE->operands()) {
          const llvm::Value* res = throughConstExpr(U.get());
          if (res != nullptr)
            return res;
        }
      } else if (const auto* I = llvm::dyn_cast_or_null<llvm::Instruction>(V)) {
        return I;
      }
      return nullptr;
    }

    typedef std::function<bool(const llvm::Value&,const llvm::CallInst&)> CallCheck_t;

    CallCheck_t BASE_CHECK = [](const llvm::Value& V, const llvm::CallInst& C) -> bool {
      return (&V) == throughConstExpr(C.getArgOperand(0)); // compare ptr's and hope llvm does not make copies of IR objects
    };

    CallCheck_t ALWAYS = [](const llvm::Value&, const llvm::CallInst&) -> bool { return true; };
    CallCheck_t NEVER = [](const llvm::Value&, const llvm::CallInst&) -> bool { return false; };


    const std::unordered_map<std::string, CallCheck_t> funcsOfInterest = {
        { "hipMalloc", ALWAYS },
        {
          "hipMemcpy", 
          [](const llvm::Value& V, const llvm::CallInst& C) -> bool {
            const llvm::Value* _V = &V;
            if (auto* TrTy = llvm::dyn_cast_or_null<llvm::ConstantInt>(C.getArgOperand(3))) {
              // get which 
              switch (TrTy->getSExtValue()) {
                case 1: // H->D
                  return throughConstExpr(C.getArgOperand(0)) == _V;
                case 2: // D->H
                  return throughConstExpr(C.getArgOperand(1)) == _V;
                case 3: // D->D
                  return (throughConstExpr(C.getArgOperand(0)) == _V) || (throughConstExpr(C.getArgOperand(1)) == _V);
                case 0: // H->H
                default: // Unknown
                  return false;
              }
            }
            llvm::errs() << "\n[scabbard::ERROR] `hipMemcpy`'s `hipMemcpyKind` was not as expected\n";
            LLVM_BUILTIN_UNREACHABLE;
          }
        },
        { "hipHostRegister", ALWAYS },
        { "hipMallocManaged", ALWAYS },
        { "hipMemAdvise", BASE_CHECK },
      };

    // << ------------------------------------------------------------------------------------------ >> 

    const llvm::Value* get_next(const llvm::Value* V) {
      // llvm::dbgs() << "\n[scabbard.host:DBG_INFO] traversing `";
      // V->print(llvm::dbgs());
      // llvm::dbgs() << "`\n";
      if (const auto* CE = llvm::dyn_cast_or_null<llvm::ConstantExpr>(V)) {
        for (const auto& U : CE->operands()) {
          const llvm::Value* res = get_next(U.get());
          if (res != nullptr)
            return res;
        }
      } else if (const auto* GV = llvm::dyn_cast_or_null<llvm::GlobalValue>(V)) {
        // llvm::dbgs() << "\n[scabbard.host:DBG_INFO] traversing SUCCESS found a global !!\n";
        return GV;
      }
      // llvm::dbgs() << "\n[scabbard.host:DBG_ERROR] traversing FAILURE could not find a global or viable path !!\n";
      return nullptr;
    }

    template<>
    DepTrace<HOST>::DepTrace(const llvm::Module& M)
      : DepTrace()
    {
      if (const auto* F = llvm::dyn_cast_or_null<llvm::Function>(M.getFunction("__hip_module_ctor"))) {
        // llvm::dbgs() << "\n[scabbard.host:DBG_INFO] this module contains a __hip_module_ctor ctor function\n\n";
        for (const auto& bb : *F)
          for (const auto& i : bb)
            if (const auto* call = llvm::dyn_cast_or_null<llvm::CallInst>(&i)) {
              if (call->getCalledFunction()->getName() == "__hipRegisterVar") {
                // llvm::dbgs() << "\n[scabbard.host:DBG_INFO] found a `__hipRegisterVar` call!\n";
                if (const auto* global = llvm::dyn_cast_or_null<llvm::GlobalVariable>(get_next(call->getArgOperand(1)))) {
                  globalDeviceMem.insert(std::make_pair(global->getName(),global));
                  // llvm::dbgs() << "\n[scabbard.host:DBG_INFO] added `" << global->getName() << "` to list of global device mem.\n";
                } else {
                  llvm::dbgs() << "\n[scabbard.host:DBG_WARN] Consistency Error, `__hip_module_ctor` contains a call to `__hipRegisterVar` that is not a global variable\n\n";
                }
              } else if (call->getCalledFunction()->getName() == "__hipRegisterManagedVar") {
                // llvm::dbgs() << "\n[scabbard.host:DBG_INFO] found a `__hipRegisterManagedVar` call!\n";
                if (const auto* global = llvm::dyn_cast_or_null<llvm::GlobalVariable>(get_next(call->getArgOperand(1)))) {
                  globalManagedMem.insert(std::make_pair(global->getName(),global));
                  // llvm::dbgs() << "\n[scabbard.host:DBG_INFO] added `" << global->getName() << "` to list of global managed mem.\n";
                } else {
                  llvm::dbgs() << "\n[scabbard.host:DBG_WARN] Consistency Error, `__hip_module_ctor` contains a call to `__hipRegisterManagedVar` that is not a global variable\n\n";
                }
              }
            }
      } else {   // this module has no HIP components
        llvm::dbgs() << "\n[scabbard.host:DBG_WARN] module that contains no hip components\n\n";
      }
    }


    // << ------------------------------------------------------------------------------------------ >> 

    template<>
    template<>
    InstrData DepTrace<HOST>::getInstrData(const llvm::StoreInst& I) const
    {
#     ifdef __SCABBARD_TRACE_HOST_WRITE_TO_GPU_READ
      llvm::SmallSet<llvm::StringRef, 8u> phiBBVisited;
      InstrData res = __getInstrData_val(*I.getPointerOperand(), phiBBVisited);
      if (res == InstrData::NEVER)
        return InstrData::NEVER;
      res |= (I.isAtomic()) ? InstrData::ATOMIC_MEM : InstrData::NEVER;
      return (InstrData)(InstrData::ON_HOST | InstrData::WRITE | res);
#     else
      return InstrData::NEVER;
#     endif
    }

    template<>
    template<>
    InstrData DepTrace<HOST>::getInstrData(const llvm::LoadInst& I) const
    {
      llvm::SmallSet<llvm::StringRef, 8u> phiBBVisited;
      if (const auto alloca = llvm::dyn_cast_or_null<llvm::AllocaInst>(I.getPointerOperand()))
        return InstrData::NEVER;  // skip all loads direct from local memory
      InstrData res = __getInstrData_val(*I.getPointerOperand(), phiBBVisited);
      if (res == InstrData::NEVER)
        return InstrData::NEVER;
      res |= (I.isAtomic()) ? InstrData::ATOMIC_MEM : InstrData::NEVER;
      return (InstrData)(InstrData::ON_HOST | InstrData::READ | res);
    }

    // template<> 
    // template<>
    // InstrData DepTrace<HOST>::getInstrData(const llvm::CallInst& I) const
    // {
    //   return InstrData::NEVER; //TODO
    // }

    template<> 
    template<>
    InstrData DepTrace<HOST>::getInstrData(const llvm::AtomicRMWInst& I) const
    {
      llvm::SmallSet<llvm::StringRef, 8u> phiBBVisited;
      return __getInstrData_val(I, phiBBVisited);
    }

    template<> 
    template<>
    InstrData DepTrace<HOST>::getInstrData(const llvm::AtomicCmpXchgInst& I) const
    {
      return InstrData::NEVER;  //This will need to change if we start recording writes on the cpu side
    }

    // << ------------------------------------------------------------------------------------------ >> 

    template<> 
    template<>
    InstrData DepTrace<HOST>::__getInstrData_rec(const llvm::LoadInst& I, llvm::SmallSet<llvm::StringRef, 8u>& phiBBVisited) const
    {
      return __getInstrData_val(*I.getPointerOperand(), phiBBVisited);
    }

    template<> 
    template<>
    InstrData DepTrace<HOST>::__getInstrData_rec(const llvm::CallInst& I, llvm::SmallSet<llvm::StringRef, 8u>& phiBBVisited) const
    {
      return InstrData::NEVER; //might need to change this to bellow if validity fails
      // return InstrData::ON_HOST | InstrData::_RUNTIME_CONDITIONAL;
    }

    template<> 
    template<>
    InstrData DepTrace<HOST>::__getInstrData_rec(const llvm::AtomicRMWInst& I, llvm::SmallSet<llvm::StringRef, 8u>& phiBBVisited) const
    {
      auto res = __getInstrData_val(*I.getPointerOperand(), phiBBVisited);
      if (res == InstrData::NEVER)
        return InstrData::NEVER;
      return (InstrData)(InstrData::ATOMIC_MEM | res);
    }

    template<> 
    template<>
    InstrData DepTrace<HOST>::__getInstrData_rec(const llvm::AddrSpaceCastInst& I, llvm::SmallSet<llvm::StringRef, 8u>& phiBBVisited) const
    {
      return __getInstrData_val(*I.getPointerOperand(), phiBBVisited);
    }

    template<> 
    template<>
    InstrData DepTrace<HOST>::__getInstrData_rec(const llvm::GetElementPtrInst& I, llvm::SmallSet<llvm::StringRef, 8u>& phiBBVisited) const
    {
      return __getInstrData_val(*I.getPointerOperand(), phiBBVisited);
    }

    template<> 
    template<>
    InstrData DepTrace<HOST>::__getInstrData_rec(const llvm::AllocaInst& I, llvm::SmallSet<llvm::StringRef, 8u>& phiBBVisited) const
    {
      //check if this is used in a hipMalloc
      for (const auto& U : I.uses()) {
        if (const auto* CI = llvm::dyn_cast_or_null<llvm::CallInst>(U.getUser())) {
          auto p = funcsOfInterest.find(CI->getCalledFunction()->getName().str());
          if (p != funcsOfInterest.end() && p->second(I,*CI))
            return InstrData::ON_HOST;
        }
      }
      return InstrData::NEVER; // this means that this ptr comes from the stack frame
    }

    template<> 
    template<>
    InstrData DepTrace<HOST>::__getInstrData_rec(const llvm::BitCastInst& I, llvm::SmallSet<llvm::StringRef, 8u>& phiBBVisited) const
    {
      return __getInstrData_val(*I.getOperand(0), phiBBVisited);
    }

    template<>
    template<>
    InstrData DepTrace<HOST>::__getInstrData_rec(const llvm::IntToPtrInst& I, llvm::SmallSet<llvm::StringRef, 8u>& phiBBVisited) const
    {
      return InstrData::ON_HOST | InstrData::_RUNTIME_CONDITIONAL; 
    }

    template<> 
    template<>
    InstrData DepTrace<HOST>::__getInstrData_rec(const llvm::Argument& A, llvm::SmallSet<llvm::StringRef, 8u>& phiBBVisited) const
    {
      const auto& TY = *A.getType();
      if (A.hasByRefAttr() || TY.isPointerTy() /* || TY.isArrayTy() || TY.isPtrOrPtrVectorTy() */) {
        // we don't know for certain if this is device mem or not, 
        // ==> so we will check before emitting trace (aka add the runtime conditional flag)
        return (InstrData)(InstrData::ON_HOST | InstrData::_RUNTIME_CONDITIONAL);
      } // else // not of concern at all
      return InstrData::NEVER;
    }


    // << ------------------------------------------------------------------------------------------ >> 

    template<> 
    template<>
    InstrData DepTrace<HOST>::getInstrData(const llvm::Instruction& i) const
    {
      if (auto* _i = llvm::dyn_cast_or_null<llvm::StoreInst>(&i)) {
        return getInstrData(*_i);
      } else if (auto* _i = llvm::dyn_cast_or_null<llvm::LoadInst>(&i)) {
        return getInstrData(*_i);
      // } else if (auto* _i = llvm::dyn_cast_or_null<llvm::CallInst>(&i)) {
      //   return getInstrData(*_i, phiBBVisited);
      } else if (auto _i = llvm::dyn_cast_or_null<llvm::AtomicRMWInst>(&i)) {
        return getInstrData(*_i);
      } else if (auto _i = llvm::dyn_cast_or_null<llvm::AtomicCmpXchgInst>(&i)) {
        return getInstrData(*_i);
      }
      return InstrData::NEVER;
    }
  



  } // namespace instr
} //?namespace scabbard
