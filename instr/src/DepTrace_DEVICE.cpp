/**
 * @file DepTrace_DEVICE.cpp
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

#include <unordered_map>

namespace scabbard {
  namespace instr {

    // typedef std::function<bool(const llvm::Value&,const llvm::CallBase&)> CallCheck_t;

    // auto BASE_CHECK(const llvm::Value& V, const llvm::CallBase& C) -> bool { 
    //         return V == *C.getArgOperand(0);
    // }

    // const std::unordered_map<std::string, CallCheck_t> funcsOfInterest = {
    //   {"", BASE_CHECK},
    //   {"<none>", BASE_CHECK}
    // };

    /**
     * @brief return whether or not the cpu can access the memory in the address space of the pointer provided
     *        (only for AMD gpu's)
     * @param PTR_OP a \c llvm::PointerType who's address space will be checked
     * @return \c bool - if the cpu can read from the address pointed to (based off of the address-space)
     */
    static bool is_accessible_address_space(const llvm::Value* PTR_OP) 
    {
      if (auto ptr = llvm::dyn_cast<llvm::PointerType>(PTR_OP->getType())) {
        switch (ptr->getAddressSpace()) {
          case 0: // generic
          case 1: // global
            return true;
          case 2: // region
            return true;    // only guessing for this one
          case 3: // local
          case 5: // private
            return false;
          case 4: // constant
          case 6: // constant 32-bit
            return true;  // technically we should never see this here since you can't write to these addresses on the gpu
          case 7: // buffer fat ptr
          case 8: // buffer resource
          case 9: // buffer strided ptr
          case 128: // stream-out registers
          default:
            return false;
        }
      }
      assert(false && "expected a pointer type");
    }

    // << ------------------------------------------------------------------------------------------ >> 

    template<>
    DepTrace<DEVICE>::DepTrace(const llvm::Module& M)
      : DepTrace()
    {
      llvm::errs() << "\n[scabbard.instr.device.ptrTrace:DBG] Constructing PtrTracer from Module\n"; //DEBUG
      for (const auto& g : M.globals())
        if (const auto* mg = M.getNamedGlobal(llvm::StringRef(std::string(g.getName().str() + ".managed")))) {
          globalManagedMem.insert(std::make_pair(g.getName(),mg));
#     if __clang_major__ <= 17
        } else if (not g.getName().endswith(".managed")) {
#     else
        } else if (not g.getName().ends_with(".managed")) {
#     endif
          globalDeviceMem.insert(std::make_pair(g.getName(),&g));
        }
    }

    // << ------------------------------------------------------------------------------------------ >> 

    template<>
    template<>
    InstrData DepTrace<DEVICE>::getInstrData(const llvm::StoreInst& I) const
    {
      llvm::errs() << "\n[scabbard.instr.device.ptrTrace:DBG] searching for origin of Ptr from LoadInst(`"<< I.getName()<<"`). "; //DEBUG
      if (not is_accessible_address_space(I.getPointerOperand())) return InstrData::NEVER;
      llvm::SmallSet<llvm::StringRef, 8u> phiBBVisited;
      InstrData res = __getInstrData_val(*I.getPointerOperand(), phiBBVisited);
      if (res == InstrData::NEVER)
        return InstrData::NEVER;
      res |= (I.isAtomic()) ? InstrData::ATOMIC_MEM : InstrData::NEVER;
      return (InstrData)(InstrData::ON_DEVICE | InstrData::WRITE | res);
    }

    template<>
    template<>
    InstrData DepTrace<DEVICE>::getInstrData(const llvm::LoadInst& I) const
    {
#     ifdef __SCABBARD_TRACE_HOST_WRITE_TO_GPU_READ
      if (not is_accessible_address_space(I.getPointerOperand())) return InstrData::NEVER;
      llvm::SmallSet<llvm::StringRef, 8u> phiBBVisited;
      InstrData res = __getInstrData_val(*I.getPointerOperand(), phiBBVisited);
      if (res == InstrData::NEVER)
        return InstrData::NEVER;
      res |= (I.isAtomic()) ? InstrData::ATOMIC_MEM : InstrData::NEVER;
      return (InstrData)(InstrData::ON_DEVICE | InstrData::READ | res);
#     else
      return InstrData::NEVER;
#     endif
    }

    // template<>
    // template<>
    // InstrData DepTrace<DEVICE>::getInstrData(const llvm::CallInst& I) const
    // {
    //   return InstrData::NEVER;
    // }

    template<>
    template<>
    InstrData DepTrace<DEVICE>::getInstrData(const llvm::AtomicRMWInst& I) const
    {
      llvm::errs() << "\n[scabbard.instr.device.ptrTrace:DBG] searching for origin of Ptr from AtomicRMWInst(`"<< I.getName()<<"`). "; //DEBUG
      if (not is_accessible_address_space(I.getPointerOperand())) return InstrData::NEVER;
      llvm::SmallSet<llvm::StringRef, 8u> phiBBVisited;
      InstrData res = __getInstrData_val(*I.getPointerOperand(), phiBBVisited);
      if (res == InstrData::NEVER)
        return InstrData::NEVER;
      return (InstrData)(InstrData::ATOMIC_MEM | InstrData::ON_DEVICE | InstrData::READ | InstrData::WRITE | res);  // this means that the mem is device heap (shared or global doesn't matter)
    }

    template<>
    template<>
    InstrData DepTrace<DEVICE>::getInstrData(const llvm::AtomicCmpXchgInst& I) const
    {
      llvm::errs() << "\n[scabbard.instr.device.ptrTrace:DBG] searching for origin of Ptr from AtomicCmpXchgInst(`"<< I.getName()<<"`). "; //DEBUG
      if (not is_accessible_address_space(I.getPointerOperand())) return InstrData::NEVER;
      llvm::SmallSet<llvm::StringRef, 8u> phiBBVisited;
      InstrData res = __getInstrData_val(*I.getPointerOperand(), phiBBVisited);
      if (res == InstrData::NEVER)
        return InstrData::NEVER;
      return (InstrData)(InstrData::ATOMIC_MEM | InstrData::ON_DEVICE | InstrData::WRITE | res);  // this means that the mem is device heap (shared or global doesn't matter)
    }

    // << ------------------------------------------------------------------------------------------ >> 

    template<>
    template<>
    InstrData DepTrace<DEVICE>::__getInstrData_rec(const llvm::LoadInst& I, llvm::SmallSet<llvm::StringRef, 8u>& phiBBVisited) const
    {
      return __getInstrData_val(*I.getPointerOperand(), phiBBVisited);
    }

    template<>
    template<>
    InstrData DepTrace<DEVICE>::__getInstrData_rec(const llvm::CallInst& I, llvm::SmallSet<llvm::StringRef, 8u>& phiBBVisited) const
    {
      return InstrData::NEVER; // TODO
    }

    template<>
    template<>
    InstrData DepTrace<DEVICE>::__getInstrData_rec(const llvm::AtomicRMWInst& I, llvm::SmallSet<llvm::StringRef, 8u>& phiBBVisited) const
    {
      return __getInstrData_val(*I.getPointerOperand(), phiBBVisited);
    }

    template<>
    template<>
    InstrData DepTrace<DEVICE>::__getInstrData_rec(const llvm::AddrSpaceCastInst& I, llvm::SmallSet<llvm::StringRef, 8u>& phiBBVisited) const
    {
      if (not is_accessible_address_space(I.getPointerOperand())) return InstrData::NEVER;
      return __getInstrData_val(*I.getPointerOperand(), phiBBVisited);
    }

    template<>
    template<>
    InstrData DepTrace<DEVICE>::__getInstrData_rec(const llvm::GetElementPtrInst& I, llvm::SmallSet<llvm::StringRef, 8u>& phiBBVisited) const
    {
      return __getInstrData_val(*I.getPointerOperand(), phiBBVisited);
    }

    template<> 
    template<>
    InstrData DepTrace<DEVICE>::__getInstrData_rec(const llvm::BitCastInst& I, llvm::SmallSet<llvm::StringRef, 8u>& phiBBVisited) const
    {
      return __getInstrData_val(*I.getOperand(0), phiBBVisited);
    }

    template<>
    template<>
    InstrData DepTrace<DEVICE>::__getInstrData_rec(const llvm::AllocaInst& I, llvm::SmallSet<llvm::StringRef, 8u>& phiBBVisited) const
    {
      //check if this is used in a hipMalloc
      // for (const auto& U : I.users()) {
      //   if (const auto& CI = llvm::dyn_cast_or_null<llvm::CallInst>(U)) {
      //     const auto p = funcsOfInterest.find(CI.getName().str());
      //     if (p != funcsOfInterest.end()) {
      //       if (p.second(I,CI))
      //         return InstrData::ON_DEVICE;
      //     }
      //   }
      // }
      return InstrData::NEVER; // this means that this ptr comes from the stack frame
    }


    template<>
    template<>
    InstrData DepTrace<DEVICE>::__getInstrData_rec(const llvm::IntToPtrInst& I, llvm::SmallSet<llvm::StringRef, 8u>& phiBBVisited) const
    {
      return InstrData::ON_DEVICE | InstrData::UNKNOWN_HEAP; 
    }

    template<>
    template<>
    InstrData DepTrace<DEVICE>::__getInstrData_rec(const llvm::Argument& I, llvm::SmallSet<llvm::StringRef, 8u>& phiBBVisited) const
    { //TODO decide if this is necessary or not
      const auto* TY = I.getType();
      if (TY != nullptr && TY->isPointerTy())
        return InstrData::ON_DEVICE | InstrData::UNKNOWN_HEAP;
      return InstrData::NEVER;
    }


    // << ------------------------------------------------------------------------------------------ >> 

    template<>
    template<>
    InstrData DepTrace<DEVICE>::getInstrData(const llvm::Instruction& i) const
    {
      llvm::errs() << "\n[scabbard.instr.host.ptrTrace:DBG] searching for origin of Ptr for Ints(`"<< i.getName()<<"`)"; //DEBUG
      if (auto* _i = llvm::dyn_cast_or_null<llvm::StoreInst>(&i)) {
        return getInstrData(*_i);
      } else if (auto* _i = llvm::dyn_cast_or_null<llvm::LoadInst>(&i)) {
        return getInstrData(*_i);
      // } else if (auto* _i = llvm::dyn_cast_or_null<llvm::CallInst>(&i)) {
      //   return getInstrData(*_i);
      } else if (auto _i = llvm::dyn_cast_or_null<llvm::AtomicRMWInst>(&i)) {
        return getInstrData(*_i);
      } else if (auto _i = llvm::dyn_cast_or_null<llvm::AtomicCmpXchgInst>(&i)) {
        return getInstrData(*_i);
      }
      return InstrData::NEVER;
    }


    


  } // namespace I
} //?namespace scabbard
