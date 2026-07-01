/**
 * @file scabbard.cpp
 * @author Andrew Osterhout
 * @brief
 *
 * @version alpha 0.0.1
 * @date 2023-03-28
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "ScabbardPass.hpp"

#include <llvm/IR/Instructions.h>
#if __clang_major__ <= 16
# include <llvm/ADT/Triple.h>
#else
# include <llvm/TargetParser/Triple.h>
#endif
#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/Regex.h>
#include <llvm/Linker/IRMover.h>
#include <llvm/Linker/Linker.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Metadata.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/Cloning.h>






#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>

#define DEBUG_TYPE "scabbard"

namespace scabbard {
  namespace instr {

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
        //llvm::errs() << "\n[scabbard.instr.device:DBG] Saving snapshot of device module pre-instrumentation to: \"" << _DBG::write_module_to_file(M,"device") << "\"\n"; //DEBUG
      }
    } //?namespace _DBG

    auto get_meta_file_from_env() -> std::string {
      const char* _META_FILE = std::getenv("SCABBARD_METADATA_FILE");
      std::string r = ((_META_FILE) 
                        ? std::string(_META_FILE) 
                        : std::string("./anon.scabbard.meta"));
      return r;
    }


    static bool is_rocm_builtin(const llvm::DIFile* FILE)
    {
      return (FILE->getDirectory().starts_with("/opt/rocm") 
              || FILE->getDirectory().contains("include/hip")
              || FILE->getFilename().starts_with("/opt/rocm")
              || FILE->getFilename().contains("include/hip")
              );
    }
    static bool is_rocm_builtin(const llvm::DebugLoc& LOC)
    {
      if (not LOC) return true;
      if (auto DIS = llvm::dyn_cast_or_null<llvm::DIScope>(LOC.getScope()))
        return is_rocm_builtin(DIS->getFile());
      return true;
    }
    static bool is_rocm_builtin(const llvm::DISubprogram* SUB)
    {
      if (SUB == nullptr) return true; // might not be rocm, but most likely an intrinsic
      auto FILE = SUB->getFile();
      if (FILE == nullptr) return true; // might not be rocm, but most likely an intrinsic
      return is_rocm_builtin(FILE);
    }


    ScabbardPassPlugin::ScabbardPassPlugin(bool isLTO_)
      : metadata(get_meta_file_from_env()), isLTO(isLTO_)
    {}

    // ScabbardPassPlugin::ScabbardPassPlugin() {}

    // ScabbardPassPlugin::ScabbardPassPlugin(const std::string& InstrLibLoc)
    // {
    //   llvm::Bitecode
    // }


    // << ========================================================================================== >> 
    // <<                                       MODULE STUFF                                         >> 
    // << ========================================================================================== >> 

    llvm::PreservedAnalyses ScabbardPassPlugin::run(llvm::Module& M, llvm::ModuleAnalysisManager& MAM)
    {
      llvm::errs() << "\n[scabbard.instr.run:DBG] running instrumentation pass\n"; //DEBUG
      const llvm::Triple target(M.getTargetTriple());
      // archBit = ((target.isArch64Bit()) ? 64 //ASSUMING for now this will only be used on 64 bit machines
      //             : ((target.isArch32Bit()) ? 32 
      //               : (target.isArch16Bit()) ? 16 : 0))
      if (target.isAMDGCN() and isLTO) { // checks for both amdgcn & r600 arch(s) (might need to restrict this to just amdgcn with `isAMDGCN()`)
        run_device(M, MAM);    //                                        To support hip on Nvidia GPUs we might need to also run this for nvptx arch(s) (this might be the same as supporting CUDA though)
      } else if (not target.isAMDGCN() and not isLTO) {
        run_host(M, MAM);
      }
      llvm::errs() << "\n[scabbard.instr.run:DBG] END instrumentation pass\n"; //DEBUG
      //TODO process analysis invalidations and return the Preserved analysis of all changes
      return llvm::PreservedAnalyses::none(); // this will have to change after transforms are performed
      // create custom implementation of Fn llvm::PreservedAnalysis::invalidate : ( -> llvm::PreservedAnalysis) to do so
    }

    // << ======================================== Device ========================================== >> 

    void ScabbardPassPlugin::run_device(llvm::Module& M, llvm::ModuleAnalysisManager& MAM)
    {
      llvm::errs() << "\n[scabbard.instr.device.run:INFO] running instrumentation pass on device/GPU module ("<< (isLTO ? "LTO" : "LateOpt") <<")\n"; //DEBUG
      // llvm::errs() << "\n[scabbard.instr.device:DBG] Saving snapshot of device module pre-instrumentation to: \"" << _DBG::write_module_to_file(M,"device") << "\"\n"; //DEBUG
      instrCallbacks_device(M, MAM);
      // llvm::FunctionAnalysisManager& fam = MAM.getResult<llvm::FunctionAnalysisManagerModuleProxy>(M)
      //   .getManager();
      // DepTraceDevice dt(M);
      // for (auto& f : M.getFunctionList())
      //     run_device(f, fam, dt);
      // auto result = linkPass.run(M,MAM);x
      // if (nullptr == M.getFunction(device.trace_append$mem_name)) {
      //   const auto SCABBARD_PATH = std::getenv("SCABBARD_PATH");
      //   if (SCABBARD_PATH == nullptr) {
      //     llvm::errs() << "\n[scabbard.instr.device.link:ERROR] Env var `SCABBARD_PATH` was undefined could not identify instrumentation file location!!\n";
      //     return;
      //   }
      //   const std::string LIBTRACE_DEVICE_PATH = std::string(SCABBARD_PATH) + "/libtrace-device.ll";
      //   auto diag = llvm::SMDiagnostic();
      //   // auto context = llvm::LLVMContext();
      //   M.getContext().setDiscardValueNames(false);
      //   std::unique_ptr<llvm::Module> traceModule = llvm::parseIRFile(LIBTRACE_DEVICE_PATH, diag, M.getContext());
      //   M.getContext().setDiscardValueNames(true);
        
      //   if (traceModule == nullptr) {
      //     llvm::errs() << "\n[scabbard.instr.device.link:ERROR] could not parse the libtrace-device bitcode/IR \"library\"!!"
      //                     "\n[scabbard.instr.device.link:ERROR] error msg: ```\n";
      //     diag.print("scabbard.instr.device.link", llvm::errs());
      //     llvm::errs() << "```\n";
      //     return;
      //   }
      //   // std::unique_ptr<llvm::Module>& traceModule = loadModule(_bug.get());
      //   if (not llvm::Triple(traceModule->getTargetTriple()).isAMDGPU()) {
      //     llvm::errs() << "\n[scabbard.instr.device.link:ERROR] could not find the device module in the libtrace-device bitcode \"library\"!!\n";
      //     return;
      //   }
      //   llvm::Linker linker(M);
      //   if (linker.linkInModule(std::move(traceModule), llvm::Linker::Flags::OverrideFromSrc, {})) {
      //     llvm::errs() << "\n[scabbard.instr.device.link:ERROR] the llvm linker encountered an error while linking!!\n";
      //     return;
      //   }
      // }
      // get references to linked in utility functions
      // device.trace_append$mem = M.getFunction(device.trace_append$mem_name);
      // device.trace_append$alloc = M.getFunction(device.trace_append$alloc_name);
      // device.dummyFunc = M.getFunction(SCABBARD_DEVICE_DUMMY_FUNC_NAME);

      // device.DeviceTrackerPtrTy_metadata = llvm::cast<llvm::DILocalVariable>(llvm::dyn_cast<llvm::Function>(device.trace_append$mem.getCallee())->getSubprogram()->getRetainedNodes()[4])->getType();

      // llvm::FunctionAnalysisManager fam; //DEBUG when running with debug build fo clang a dense map inside get result has unexpected behavior, but `fam` isn't actually used yet so just swap it for a default for now
      llvm::FunctionAnalysisManager& fam = MAM.getResult<llvm::FunctionAnalysisManagerModuleProxy>(M)
                                              .getManager();
      
      DepTraceDevice dt(M);
      llvm::SmallVector<llvm::Function*> funcs;
      for (auto& f : M.getFunctionList())
        funcs.push_back(&f);
      for (auto* f : funcs)
        if (not(f->getName().starts_with("llvm.") //exclude intrinsics
             || f->getName().starts_with("scabbard.") //exclude name mangled scabbard builtins
             || device.NO_INSTR_FNS.count(f->getName().str()) // a manually excluded function (usually c++ builtin)
             || is_rocm_builtin(f->getSubprogram())
             || f->hasFnAttribute("disable_sanitizer_instrumentation"))) {
          run_device(*f, fam, dt);
        }
      finish_replacing_old_funcs_device(M);
      // remove dummy function
      auto fn = M.getFunction(SCABBARD_DEVICE_DUMMY_FUNC_NAME);
      if (fn != nullptr)
        fn->eraseFromParent();

      // remove unused virtual function call handlers (__cxa_pure_virtual, __cxa_deleted_virtual, __assert_fail)
      // for (auto fn_name : CXX_DUP_FNS) {
      //   auto fn = M.getFunction(fn_name);
      //   if (fn != nullptr && fn->getNumUses() < 1u) 
      //     fn->eraseFromParent();
      // }

      // remove tmp variables with function comdat pointers referencing replaced functions
      // llvm::SmallVector<llvm::GlobalVariable*,4u> GV_to_remove;
      // for (auto& GV : M.globals())
      //   if (GV.getName().starts_with("__clang_gpu_used_external")) 
      //     GV_to_remove.push_back(&GV);
      

      // remove the dummy caller function from device_def
      // M.getFunction(SCABBARD_DEVICE_DUMMY_FUNC_NAME)->eraseFromParent(); //note: causes all linked functions to also be removed
      // llvm::errs() << "\n[scabbard.instr.device:DBG] Saving snapshot of device module pre-instrumentation to: \"" << _DBG::write_module_to_file(M,"device") << "\"\n"; //DEBUG
      // llvm::errs() << "\n[scabbard.instr.device:DBG] Saving snapshot of device module post-instrumentation to: \"" << _DBG::write_module_to_file(M,"device.instr") << "\"\n"; //DEBUG
    }

    void ScabbardPassPlugin::instrCallbacks_device(llvm::Module& M, llvm::ModuleAnalysisManager& MAM)
    {
      auto ptrTy = llvm::PointerType::get(M.getContext(), 0u);
      auto int64Ty = llvm::IntegerType::get(M.getContext(), 64u);
      device.trace_append$mem = M.getOrInsertFunction(
          device.trace_append$mem_name,
          llvm::FunctionType::get(
              llvm::Type::getVoidTy(M.getContext()),
              llvm::ArrayRef<llvm::Type*>(std::vector<llvm::Type*>{
                  ptrTy,
                  llvm::IntegerType::get(M.getContext(), sizeof(InstrData) * 8),
                  ptrTy, //WARN: This constant 0u might need to be dynamicly decided for host modules
                  int64Ty
                }),
              false
            )
        );
      device.trace_append$alloc = M.getOrInsertFunction(
          device.trace_append$alloc_name,
          llvm::FunctionType::get(
              llvm::Type::getVoidTy(M.getContext()),
              llvm::ArrayRef<llvm::Type*>(std::vector<llvm::Type*>{
                  ptrTy,
                  llvm::IntegerType::get(M.getContext(), sizeof(InstrData) * 8),
                  ptrTy,
                  int64Ty,
                  int64Ty
                }),
              false
            )
        );
    }



    // << ========================================= Host =========================================== >> 

    void ScabbardPassPlugin::run_host(llvm::Module& M, llvm::ModuleAnalysisManager& MAM)
    {
      if (nullptr != M.getFunction(host.scabbard_init_name) 
            || nullptr != M.getFunction(host.trace_append$mem_name)
            || nullptr != M.getFunction(host.register_job_name))
          return; // return early this is already instrumented
      llvm::errs() << "\n[scabbard.instr.host.run:INFO] running instrumentation pass on host/CPU module ("<< (isLTO ? "LTO" : "LateOpt") <<")\n\n"; //DEBUG
      // llvm::errs() << "\n[scabbard.instr.host:DBG] Saving snapshot of host module pre-instrumentation to: \"" << _DBG::write_module_to_file(M,"host") << "\"\n\n"; //DEBUG
      // make any necessary additions to the Module (i.e.inserting globals and linking references)
      instrCallbacks_host(M, MAM);
      llvm::FunctionAnalysisManager& fam = MAM.getResult<llvm::FunctionAnalysisManagerModuleProxy>(M)
        .getManager();
      // instrument if module contains main
      if (auto f = M.getFunction("main"))
        instr_mainFunc_host(*f, fam);
      DepTraceHost dt(M);
      for (auto& f : M.getFunctionList())
        if (not(f.getName() == "__hip_module_ctor"
             || f.getName() == "__hip_module_dtor"
             || f.hasFnAttribute("disable_sanitizer_instrumentation")))
          run_host(f, fam, dt);

      // llvm::errs() << "\n[scabbard.instr.host:DBG] Saving snapshot of host module post-instrumentation to: \"" << _DBG::write_module_to_file(M,"host.instr") << "\"\n"; //DEBUG
    }

    void ScabbardPassPlugin::instrCallbacks_host(llvm::Module& M, llvm::ModuleAnalysisManager& MAM)
    {
      if (M.getFunction("main") != nullptr)
        host.scabbard_init = M.getOrInsertFunction(
            host.scabbard_init_name,
            llvm::FunctionType::get(
                llvm::Type::getVoidTy(M.getContext()),
                {},
                // llvm::ArrayRef<llvm::Type*>(std::array<llvm::Type*,1>{
                //     llvm::Type::getVoidTy(M.getContext()),
                //   }),
                false
              )
          );
      // host.scabbard_close = M.getOrInsertFunction(
      //     host.scabbard_close_name,
      //     llvm::FunctionType::get(
      //         llvm::Type::getVoidTy(M.getContext()),
      //         llvm::ArrayRef<llvm::Type*>(std::array<llvm::Type*,1>{
      //             llvm::Type::getVoidTy(M.getContext()),
      //           }),
      //         false
      //       )
      //   );
      host.trace_append$mem = M.getOrInsertFunction(
          host.trace_append$mem_name,
          llvm::FunctionType::get(
              llvm::Type::getVoidTy(M.getContext()),
              llvm::ArrayRef<llvm::Type*>(std::vector<llvm::Type*>{
                  llvm::IntegerType::get(M.getContext(), sizeof(InstrData) * 8),
                  llvm::PointerType::get(M.getContext(), 0u), //WARN: This constant 0u might need to be dynamicly decided for host modules
                  llvm::IntegerType::get(M.getContext(), 64u)
                }),
              false
            )
        );
      host.trace_append$mem$cond = M.getOrInsertFunction(
          host.trace_append$mem$cond_name,
          llvm::FunctionType::get(
              llvm::Type::getVoidTy(M.getContext()),
              llvm::ArrayRef<llvm::Type*>(std::vector<llvm::Type*>{
                  llvm::IntegerType::get(M.getContext(), sizeof(InstrData) * 8),
                  llvm::PointerType::get(M.getContext(), 0u), //WARN: This constant 0u might need to be dynamicly decided for host modules
                  llvm::IntegerType::get(M.getContext(), 64u)
                }),
              false
            )
        );
      host.trace_append$alloc = M.getOrInsertFunction(
          host.trace_append$alloc_name,
          llvm::FunctionType::get(
              llvm::Type::getVoidTy(M.getContext()),
              llvm::ArrayRef<llvm::Type*>(std::vector<llvm::Type*>{
                  llvm::IntegerType::get(M.getContext(), sizeof(InstrData) * 8),
                  llvm::PointerType::get(M.getContext(), 0u), //WARN: This constant 0u might need to be dynamicly decided for host modules
                  llvm::IntegerType::get(M.getContext(), 64u),
                  llvm::IntegerType::get(M.getContext(), 64u)
                }),
              false
            )
        );
      host.register_job = M.getOrInsertFunction(
          host.register_job_name,
          llvm::FunctionType::get(
              llvm::PointerType::getUnqual(M.getContext()),
              llvm::ArrayRef<llvm::Type*>(std::vector<llvm::Type*>{
                  llvm::PointerType::getUnqual(M.getContext())
                }),
              false
            )
        );
      host.register_job_callback = M.getOrInsertFunction(
          host.register_job_callback_name,
          llvm::FunctionType::get(
              llvm::Type::getVoidTy(M.getContext()),
              llvm::ArrayRef<llvm::Type*>(std::vector<llvm::Type*>{
                  llvm::PointerType::getUnqual(M.getContext()),
                  llvm::PointerType::getUnqual(M.getContext()),
                  llvm::IntegerType::get(M.getContext(), 64u)
                }),
              false
            )
        );
    }



    // << ========================================================================================== >> 
    // <<                                      FUNCTION STUFF                                        >> 
    // << ========================================================================================== >> 

    // llvm::PreservedAnalyses ScabbardPassPlugin::run(llvm::Function &F, llvm::FunctionAnalysisManager &FAM) 
    // {
    //   if (F.getCallingConv() == llvm::CallingConv::AMDGPU_KERNEL) // figure out if this is a kernel function or not
    //     return run_device(F, FAM);
    //   return run_host(F, FAM); // this will have to change after transforms are performed
    //   // create custom implementation of Fn llvm::PreservedAnalysis::invalidate : ( -> llvm::PreservedAnalysis) to do so
    // }


    // << ======================================== Device ========================================== >> 

    void ScabbardPassPlugin::run_device(llvm::Function& _F, llvm::FunctionAnalysisManager& FAM, const DepTraceDevice& DT)
    {
      llvm::errs() << "\n[scabbard.instr.device.run:DBG] START working on Fn(`"<< _F.getName() <<"`) \n"; //DEBUG
      // if (_F.isDeclaration()) return; // skip functions not defined (only declared) in this module 
      llvm::Function& F = *replace_device_function(_F);

      llvm::SmallVector<std::tuple<llvm::Instruction*,llvm::Value*,InstrData,bool>,16u> to_instr;
      llvm::SmallVector<llvm::CallInst*,16u> calls_to_instr;

       llvm::errs() << "\n[scabbard.instr.device.run:DBG] searching Fn for instructions of interest. \n"; //DEBUG
      // search for instructions to instrument
      for (auto& bb : F)
        for (auto& i : bb)
          if (auto* store = llvm::dyn_cast<llvm::StoreInst>(&i)) {
            auto data = DT.getInstrData(*store);
            LLVM_DEBUG(
              if (not data) continue;
              llvm::errs() << "[scabbard.instr.device:DBG] Found a `store` inst to instrument!\n"
                "[scabbard.instr.device:DBG]    prop: " << data << "\n"
                "[scabbard.instr.device:DBG]    repr: `";
              i.print(llvm::errs());
              llvm::errs() << "`\n[scabbard.instr.device:DBG]\n";
            );
            //instrument store instructions to update trace
            to_instr.emplace_back(&i, store->getPointerOperand(), data, false);
          } else if (auto* load = llvm::dyn_cast<llvm::LoadInst>(&i)) {
            auto data = DT.getInstrData(*load);
            if (not data) continue;
            LLVM_DEBUG(
              llvm::errs() << "[scabbard.instr.device:DBG] Found a `load` inst to instrument!\n"
                "[scabbard.instr.device:DBG]    prop: " << data << "\n"
                "[scabbard.instr.device:DBG]    repr: `";
              i.print(llvm::errs());
              llvm::errs() << "`\n[scabbard.instr.device:DBG]\n";
            );
            //instrument load instructions
            to_instr.emplace_back(&i, load->getPointerOperand(), data, true);
          } else if (auto* call = llvm::dyn_cast<llvm::CallInst>(&i)) {
            calls_to_instr.push_back(call);
          } else if (auto atomic = llvm::dyn_cast<llvm::AtomicRMWInst>(&i)) {
            auto data = DT.getInstrData(*atomic);
            LLVM_DEBUG(
              if (not data) continue;
              llvm::errs() << "[scabbard.instr.device:DBG] Found an `atomicrmw` inst to instrument!\n"
                "[scabbard.instr.device:DBG]    prop: " << data << "\n"
                "[scabbard.instr.device:DBG]    repr: `";
              i.print(llvm::errs());
              llvm::errs() << "`\n[scabbard.instr.device:DBG]\n";
            );
            //instrument atomic readwrite instructions
            to_instr.emplace_back(&i, atomic->getPointerOperand(), data, true);
          } else if (auto cmpxchg = llvm::dyn_cast<llvm::AtomicCmpXchgInst>(&i)) {
            auto data = DT.getInstrData(*cmpxchg);
            LLVM_DEBUG(
              if (not data) continue;
              llvm::errs() << "[scabbard.instr.device:DBG] Found an `cmpxchg` inst to instrument!\n"
                "[scabbard.instr.device:DBG]    prop: " << data << "\n"
                "[scabbard.instr.device:DBG]    repr: `";
              i.print(llvm::errs());
              llvm::errs() << "`\n[scabbard.instr.device:DBG]\n";
            );
            //instrument atomic readwrite instructions
            to_instr.emplace_back(&i, cmpxchg->getPointerOperand(), data, true);
          }
      
      llvm::errs() << "\n[scabbard.instr.device.run:DBG] instrumenting found instructions (#: " << to_instr.size() << ")\n"; //DEBUG
      // instrument found instructions
      for (auto [I, Ptr, data, insertAfter] : to_instr)
        instr_mem_func_device(F, *I, Ptr, data, insertAfter);
      for (llvm::CallInst* CI : calls_to_instr)
        instr_call_device(F, CI);

      llvm::errs() << "\n[scabbard.instr.device.run:DBG] END working on Fn(`"<< _F.getName() <<"`) \n"; //DEBUG
      // llvm::errs() << "\n[scabbard.instr.device:DBG] instrumented a function:\n```"; F.print(llvm::errs()); llvm::errs() << "\n```\n"; //DEBUG
    }



    void ScabbardPassPlugin::instr_mem_func_device(llvm::Function& F, 
                                                    llvm::Instruction& I, llvm::Value* V,
                                                    const InstrData& data, bool InsertAfter)
    {
      if (not (data & InstrData::ON_DEVICE)) // make sure this is supposed to be instrumented on device
        return;
      // llvm::IRBuilder<> IRB(&I);
      llvm::CastInst* castInst = nullptr;
      if (auto ptrTy = llvm::dyn_cast<llvm::PointerType>(V->getType()))
        if (ptrTy->getAddressSpace() != 0u) { //NOTE hard coding this might be an issue
          castInst = llvm::AddrSpaceCastInst::Create(llvm::Instruction::CastOps::AddrSpaceCast, V, device.trace_append$mem.getFunctionType()->getParamType(2u));
          castInst->insertBefore(&I);
        }
      auto loc = metadata.trace(F, I.getDebugLoc(), ModuleType::DEVICE);
      auto ci = llvm::CallInst::Create(
          device.trace_append$mem,
          llvm::ArrayRef<llvm::Value*>(std::array<llvm::Value*,4>{
              F.getArg(F.arg_size()-1),
              llvm::ConstantInt::get(
                  llvm::IntegerType::get(F.getContext(), sizeof(InstrData) * 8),
                  llvm::APInt(sizeof(InstrData)*8, data)
                ),
              (castInst ? castInst : V),
              loc.get_as_constant(F.getContext())
            })
        );
      // ci->setIsNoInline();
      ci->setDebugLoc(I.getDebugLoc());
      ci->setCallingConv(llvm::CallingConv::Fast);
      // if (castInst && InsertAfter) {
      //   castInst->insertAfter(&I);
      //   ci->insertAfter(castInst);
      // } else if (castInst) {
      //   castInst->insertBefore(&I);
      //   ci->insertBefore(&I);
      // } else 
      if (InsertAfter) {
        ci->insertAfter(&I);
      } else {
        ci->insertBefore(&I);
      }
      // if (castInst) { llvm::errs()<<"\n[scabbard.instr.device:DBG] instrumented instruction. reporting surrounding block in fn `"<<F.getName()<<"`\n```"; I.getParent()->print(llvm::errs()); llvm::errs()<<"\n```\n"; } //DEBUG
    }

    void ScabbardPassPlugin::instr_call_device(const llvm::Function& F, llvm::CallInst* ci)
    {
      //TODO modify to register reads and writes from builtin functions
    }


    llvm::Function* ScabbardPassPlugin::replace_device_function(llvm::Function& OldFn)
    {
      llvm::errs() << "\n[scabbard.instr.device.expandFnSigs:DBG] cloning and extending Fn signature.\n"; //DEBUG
      llvm::Module& M = *OldFn.getParent();
      std::string old_name = OldFn.getName().str();
      const auto* ptrTy = llvm::PointerType::get(OldFn.getContext(),0ul);
      OldFn.setName(old_name+"__old__scabbard_instr_replaced__old__");
      auto oldParamTys = OldFn.getFunctionType()->params();
      std::vector<llvm::Type*> paramTys(oldParamTys.begin(), oldParamTys.end());
      paramTys.push_back((llvm::Type*)ptrTy);
      auto fn_callee = M.getOrInsertFunction(
                          old_name,
                          llvm::FunctionType::get(
                              OldFn.getFunctionType()->getReturnType(),
                              llvm::ArrayRef<llvm::Type*>(paramTys),
                              OldFn.getFunctionType()->isVarArg()
                            ),
                          OldFn.getAttributes()
                        );
      llvm::Function* NewFn = llvm::dyn_cast<llvm::Function>(fn_callee.getCallee()); // new function (OldFn is old function)
      NewFn->setCallingConv(OldFn.getCallingConv());
      NewFn->setLinkage(OldFn.getLinkage());

      to_replace.push_back(std::tuple(&OldFn,NewFn));
      // if (OldFn.isDeclaration())
      //   return NewFn;
      
      llvm::ValueToValueMapTy vMap;
      for (size_t i=0; i<OldFn.arg_size(); ++i)
        vMap[OldFn.getArg(i)] = NewFn->getArg(i);
      llvm::SmallVector<llvm::ReturnInst*,8> rets;
      //?NOTE: this might be used wrong.  Double check results in testing to make sure it works correctly
      //?     if wrong likely due to not creating vMap properly
      llvm::CloneFunctionInto(NewFn, &OldFn, vMap, llvm::CloneFunctionChangeType::LocalChangesOnly, rets);
      // provide metadata for the added argument
      // if (NewFn->isDeclaration()) return NewFn; //skip this for external functions
      // auto* subPMD = NewFn->getSubprogram();
      // auto retMDNs = subPMD->getRetainedNodes();
      // std::vector<llvm::Metadata*> arg_metadata;
      // for (auto arg : retMDNs) arg_metadata.push_back(arg);
      // arg_metadata.push_back(
      //     llvm::DILocalVariable::get(
      //       NewFn->getContext(),
      //       subPMD,
      //       llvm::MDString::get(NewFn->getContext(), "SCABBARD_DT"),
      //       subPMD->getFile(),
      //       subPMD->getLine(),
      //       device.DeviceTrackerPtrTy_metadata,
      //       arg_metadata.size(),
      //       llvm::DINode::DIFlags::FlagObjectPointer,
      //       llvm::dwarf::MemorySpace::DW_MSPACE_LLVM_constant,
      //       8u,
      //       nullptr
      //     )
      //   );
      // subPMD->replaceRetainedNodes(llvm::DINodeArray(llvm::MDTuple::get(NewFn->getContext(),arg_metadata)));
      // // amend function type metadata
      // std::vector<llvm::Metadata*> fnTy_metadata;
      // auto fnTyMDs = subPMD->getType();
      // for (auto type : subPMD->getType()->getTypeArray()) fnTy_metadata.push_back(type);
      // fnTy_metadata.push_back(device.DeviceTrackerPtrTy_metadata);
      // subPMD->replaceType(
      //   llvm::DISubroutineType::get(
      //       NewFn->getContext(),
      //       fnTyMDs->getFlags(),
      //       fnTyMDs->getCC(),
      //       llvm::DITypeRefArray(llvm::MDTuple::get(NewFn->getContext(), fnTy_metadata))
      //     )
      //   );
      // return the new function
      return NewFn;
    }

    void ScabbardPassPlugin::finish_replacing_old_funcs_device(llvm::Module& M)
    {
      llvm::errs() << "\n[scabbard.instr.device.extendFnSigs:DBG] Cleanup old Fn's and perform RAUW with new Fn's\n"; //DEBUG
      llvm::SmallPtrSet<llvm::CallInst*,8u> to_remove;
      //modify to pass device tracker through as last parameter to all functions defined in this module'
      for (auto& tr : to_replace) {
        llvm::Function* OldFn, * NewFn; // llvm::MDNode* NewFnMDN;
        // std::tie(OldFn, NewFn, NewFnMDN) = tr;
        std::tie(OldFn, NewFn) = tr;
        for (auto u : OldFn->users()) {
          // if (auto CI = llvm::dyn_cast<llvm::CallInst>(u.getUser())) {
          if (auto CI = llvm::dyn_cast<llvm::CallInst>(u)) {
            llvm::Function* iFn = CI->getFunction();
#           if __clang_major__ <= 16
              if (iFn->getName().endswith("__old__scabbard_instr_replaced__old__"))
                continue;
#           else
              if (iFn->getName().ends_with("__old__scabbard_instr_replaced__old__"))
                continue;
#           endif

            llvm::SmallVector<llvm::Value*,4u> operands;
            for (auto& op : CI->args())
              operands.push_back(op.get());
            operands.push_back(iFn->getArg(iFn->arg_size()-1));
            auto ci = llvm::CallInst::Create(
                          NewFn->getFunctionType(),
                          NewFn,
                          llvm::ArrayRef<llvm::Value*>(operands)            
                        );
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
            LLVM_DEBUG(llvm::errs() << "\n[scabbard.instr.device:DBG] overwritten device function used in non-call instruction!\n";);
          }
        }
        for (auto ci : to_remove)
          ci->eraseFromParent();
        to_remove.clear();
      }
      // remove OldFn from module
      for (auto& tr : to_replace) {
        llvm::Function* OldFn, * NewFn;
        std::tie(OldFn, NewFn) = tr;
        OldFn->replaceAllUsesWith(NewFn);
        OldFn->eraseFromParent();
      }
      // clear the list so we're ready for reuse
      to_replace.clear();

    }


    // << ========================================= Host =========================================== >> 


    void ScabbardPassPlugin::run_host(llvm::Function& F, llvm::FunctionAnalysisManager& FAM, const DepTraceHost& DT)
    {
      if (F.isDeclaration()) return; // skip functions not defined (only declared) in this module

      llvm::errs() << "\n[scabbard.instr.host.run:DBG] Start working on Fn(`"<< F.getName() <<"`) \n"; //DEBUG
      
      llvm::SmallVector<std::tuple<llvm::Instruction*,llvm::Value*,InstrData>,64u> to_instr;
      llvm::SmallVector<llvm::CallInst*,32u> calls_to_instr;

      // find instructions to instrument
      for (auto& bb : F)
        for (auto& i : bb)
          if (auto* store = llvm::dyn_cast<llvm::StoreInst>(&i)) {
            auto data = DT.getInstrData(*store);
            LLVM_DEBUG(
              if (not data) continue;
            llvm::errs() << "[scabbard.instr.host:DBG] Found a `store` inst to instrument!\n"
              "[scabbard.instr.host:DBG]    prop: " << data << "\n"
              "[scabbard.instr.host:DBG]    repr: `";
            i.print(llvm::errs());
            llvm::errs() << "`\n[scabbard.instr.host:DBG]\n";
            );
            // instrument store instructions to update trace 
            to_instr.emplace_back(&i, store->getPointerOperand(), data);
          } else if (auto* load = llvm::dyn_cast<llvm::LoadInst>(&i)) {
            auto data = DT.getInstrData(*load);
            LLVM_DEBUG(
              if (not data) continue;
            llvm::errs() << "[scabbard.instr.host:DBG] Found a `load` inst to instrument!\n"
              "[scabbard.instr.host:DBG]    prop: " << data << "\n"
              "[scabbard.instr.host:DBG]    repr: `";
            i.print(llvm::errs());
            llvm::errs() << "`\n[scabbard.instr.host:DBG]\n";
            );
            // skip the loads inserted after instrumenting a hipMalloc call
            if (not load->getDebugLoc()) continue; //NOTE: bad optimizers might make this skip valid loads to instrument
            // instrument load instructions
            to_instr.emplace_back(&i, load->getPointerOperand(), data);
          } else if (auto* call = llvm::dyn_cast<llvm::CallInst>(&i)) {
            calls_to_instr.push_back(call);
          } else if (auto atomic = llvm::dyn_cast<llvm::AtomicRMWInst>(&i)) {
            auto data = DT.getInstrData(*atomic);
            LLVM_DEBUG(
              if (not data) continue;
            llvm::errs() << "[scabbard.instr.host:DBG] Found an `atomicrmw` inst to instrument!\n"
              "[scabbard.instr.host:DBG]    prop: " << data << "\n"
              "[scabbard.instr.host:DBG]    repr: `";
            i.print(llvm::errs());
            llvm::errs() << "`\n[scabbard.instr.host:DBG]\n";
            );
            //instrument atomic readwrite instructions
            to_instr.emplace_back(&i, atomic->getPointerOperand(), data);
          } else if (auto cmpxchg = llvm::dyn_cast<llvm::AtomicCmpXchgInst>(&i)) {
            auto data = DT.getInstrData(*cmpxchg);
            LLVM_DEBUG(
              if (not data) continue;
            llvm::errs() << "[scabbard.instr.host:DBG] Found an `cmpxchg` inst to instrument!\n"
              "[scabbard.instr.host:DBG]    prop: " << data << "\n"
              "[scabbard.instr.host:DBG]    repr: `";
            i.print(llvm::errs());
            llvm::errs() << "`\n[scabbard.instr.host:DBG]\n";
            );
            //instrument atomic readwrite instructions
            to_instr.emplace_back(&i, cmpxchg->getPointerOperand(), data);
          }
          // return llvm::PreservedAnalyses::all();
          for (auto [I, Ptr, data] : to_instr)
            instr_mem_func_device(F, *I, Ptr, data);
          for (llvm::CallInst* CI : calls_to_instr)
            instr_call_device(F, CI);

          llvm::errs() << "\n[scabbard.instr.host.run:DBG] END working on Fn(`"<< F.getName() <<"`) \n"; //DEBUG
    }


    void ScabbardPassPlugin::instr_mainFunc_host(llvm::Function& MAIN, llvm::FunctionAnalysisManager &FAM)
    {
      // auto BB = llvm::BasicBlock::Create(MAIN.getContext(), "_BBscabbard_init");
      auto CI = llvm::CallInst::Create(host.scabbard_init, "");
      auto firstInst = &(*(MAIN.getEntryBlock().getFirstInsertionPt()));
      CI->insertBefore(firstInst);
      CI->setDebugLoc(firstInst->getDebugLoc());
      // BB->insertInto(&MAIN, &MAIN.getEntryBlock());
    }


    void ScabbardPassPlugin::instr_mem_func_host(llvm::Function& F, 
                                                llvm::Instruction& I, llvm::Value* V,
                                                const InstrData& data, bool InsertAfter)
    {
      if (not (data & InstrData::ON_HOST)) // make sure this is supposed to be instrumented on device
        return;
      auto loc = metadata.trace(F, I.getDebugLoc(), ModuleType::HOST);
      auto ci = llvm::CallInst::Create(
          ((data & InstrData::_RUNTIME_CONDITIONAL) ? host.trace_append$mem$cond : host.trace_append$mem),
          llvm::ArrayRef<llvm::Value*>(std::array<llvm::Value*,3>{
              llvm::ConstantInt::get(
                  llvm::IntegerType::get(F.getContext(), sizeof(InstrData) * 8),
                  llvm::APInt(sizeof(InstrData)*8, data)
                ),
              V,
              loc.get_as_constant(F.getContext())
            })
        );
      // ci->setIsNoInline();
      ci->setDebugLoc(I.getDebugLoc());
      ci->setCallingConv(llvm::CallingConv::Fast);
      ci->setDebugLoc(I.getDebugLoc());
      if (InsertAfter)
        ci->insertAfter(&I);
      else
        ci->insertBefore(&I);
    }


    /**
     * @brief Get the value that contains the mem address of the pointer pointer
     *        (used to get the mem address of the ptr value of hipMalloc)
     *        Not safe to use outside of specific use
     */
    llvm::Value* get_ptr_from_ptr(llvm::Value* V)
    {
      if (auto* alloc = llvm::dyn_cast<llvm::AllocaInst>(V)) {
        return alloc;
      } else if (auto* arg = llvm::dyn_cast<llvm::Argument>(V)) {
        if (arg->getType()->isPointerTy())
          return arg;
        return nullptr;
      } else if (auto* bitCast = llvm::dyn_cast<llvm::BitCastInst>(V)) {
        return get_ptr_from_ptr(bitCast->getOperand(0u)); //NOTE: this not being arg operand might cause problems
      } else if (auto* bitCast = llvm::dyn_cast<llvm::BitCastOperator>(V)) {
        return get_ptr_from_ptr(bitCast->getOperand(0u)); //NOTE: this not being arg operand might cause problems
      } else if (auto* inst = llvm::dyn_cast<llvm::Instruction>(V)) {
        return get_ptr_from_ptr(inst->getOperand(0u)); //NOTE: this not being arg operand might cause problems
      }
      return nullptr;
    }


    void ScabbardPassPlugin::instr_call_host(llvm::Function& F, llvm::CallInst* CI)
    {
      llvm::errs() << "\n[scabbard.instr.host.run:DBG] instrumenting call to `"<< CI->getCalledFunction()->getName() <<"` \n"; //DEBUG
      const auto* fn = CI->getCalledFunction();
      if (fn == nullptr) return; // don't deal with antonymous functions 
      // const auto& fnTy = *fn.getFunctionType();
      if (not fn->hasName()) return; // don't deal with antonymous functions
      const auto fnName = fn->getName();
      if (fnName == "hipStreamSynchronize" || fnName == "hipDeviceSynchronize") // make sure this is supposed to be instrumented on device
      {
      auto loc = metadata.trace(F, CI->getDebugLoc(), ModuleType::HOST);
      // auto ptrtoint = llvm::CastInst::Create(llvm::CastInst::CastOps::PtrToInt, 
      //                                         CI->getArgOperand(0), 
      //                                         llvm::IntegerType::getInt64PtrTy(F.getContext()));
      auto ci = llvm::CallInst::Create(
          host.trace_append$mem,
          llvm::ArrayRef<llvm::Value*>(std::array<llvm::Value*,3>{
              llvm::ConstantInt::get(
                  llvm::IntegerType::get(F.getContext(), sizeof(InstrData) * 8),
                  llvm::APInt(sizeof(InstrData)*8, InstrData::SYNC_EVENT)
                ),
              ((fnName == "hipDeviceSynchronize") 
                ? llvm::ConstantPointerNull::get(llvm::PointerType::get(F.getContext(), 0u))
                : CI->getArgOperand(0u)), // ptrtoint
              loc.get_as_constant(F.getContext())
            })
        );
        ci->insertAfter(CI);
        ci->setDebugLoc(CI->getDebugLoc());
      }
      else if (fnName == "hipMalloc")
      {
        auto loc = metadata.trace(F, CI->getDebugLoc(), ModuleType::HOST);
        if (auto _mem_loc = get_ptr_from_ptr(CI->getArgOperand(0u))) {
          auto mem_loc = new llvm::LoadInst(llvm::PointerType::get(F.getContext(),0u), _mem_loc, llvm::Twine(""), CI->getNextNode());
          auto ci = llvm::CallInst::Create(
              host.trace_append$alloc,
              llvm::ArrayRef<llvm::Value*>(std::array<llvm::Value*,4>{
                  llvm::ConstantInt::get(
                      llvm::IntegerType::get(F.getContext(), sizeof(InstrData) * 8),
                      llvm::APInt(sizeof(InstrData)*8, InstrData::ALLOCATE | InstrData::UNKNOWN_HEAP)
                    ),
                  mem_loc, // ptrtoint
                  loc.get_as_constant(F.getContext()),
                  CI->getArgOperand(1u)
                })
            );
          ci->insertAfter(mem_loc);
          ci->setDebugLoc(CI->getDebugLoc());
        } 
        else llvm::errs() << "\n[scabbard.instr.host:ERR] hip malloc found but could not find associated memory address!\n"; //DEBUG
      }
      else if (fnName == "hipFree")
      {
        auto loc = metadata.trace(F, CI->getDebugLoc(), ModuleType::HOST);
        auto ci = llvm::CallInst::Create(
          host.trace_append$alloc,
          llvm::ArrayRef<llvm::Value*>(std::array<llvm::Value*,4>{
              llvm::ConstantInt::get(
                  llvm::IntegerType::get(F.getContext(), sizeof(InstrData) * 8),
                  llvm::APInt(sizeof(InstrData)*8, InstrData::FREE | InstrData::DEVICE_HEAP)
                ),
              CI->getArgOperand(0u),
              loc.get_as_constant(F.getContext()),
              llvm::ConstantInt::get( // we can't contextually know how much is being free'd inline, but we can keep track like a normal allocator does 
                  llvm::IntegerType::get(F.getContext(), 64u),
                  llvm::APInt(64u, 0ul)
                )
            })
        );
        ci->insertBefore(CI);
        ci->setDebugLoc(CI->getDebugLoc());
      }
      else if (fnName == "hipLaunchKernel")
      {
       instr_launch_func_host(F,*CI);
       return;
      }
      else if (fnName.starts_with("hipMemcpy"))
      {
        auto loc = metadata.trace(F, CI->getDebugLoc(), ModuleType::HOST);
        auto getSrcLoc = [&]() -> std::string {
          return F.getParent()->getSourceFileName() 
                + ':' + std::to_string(CI->getDebugLoc().getLine())
                + ',' + std::to_string(CI->getDebugLoc().getCol());
        };
        llvm::Regex incompatiblePattern(".*(?:Symbol|Array|2D|3D).*");
        if (incompatiblePattern.match(fnName)) {
          llvm::errs() << "\n[scabbard.instr.host:WARN] WARNING: Scabbard does not currently support using Hip Symbols, Hip Arrays, or the Hip 2D/3D memcpy operations."
                              " (from: `" << getSrcLoc() << "`)\n"
                          "\n[scabbard.instr.host:WARN]          >>> Validity of Scabbard results cannot be guaranteed for this program! <<< \n";
          return;
        }
        // hipMemCpy performs a hipStreamSync(0) unless it's async so we must also register the sync event
        if (not fnName.contains("Async")) {
          auto cis = llvm::CallInst::Create(
            host.trace_append$mem,
            llvm::ArrayRef<llvm::Value*>(std::array<llvm::Value*,3>{
                llvm::ConstantInt::get(
                    llvm::IntegerType::get(F.getContext(), sizeof(InstrData) * 8),
                    llvm::APInt(sizeof(InstrData)*8, InstrData::SYNC_EVENT)
                  ),
                ((fnName == "hipMemcpyWithStream") 
                  ? CI->getArgOperand(0)
                  : llvm::ConstantPointerNull::get(llvm::PointerType::get(F.getContext(), 0u))),
                loc.get_as_constant(F.getContext())
              })
          );
          cis->insertBefore(CI);
          cis->setDebugLoc(CI->getDebugLoc());
        }
        // we only need to register the cpy as a read if it's from device to host
        llvm::Regex transferDirPattern("\\w+[DH]to[DH]");
        if (not transferDirPattern.match(fnName)) { // check if direction is an argument or not
          if (auto* TrTy = llvm::dyn_cast<llvm::ConstantInt>(CI->getArgOperand(CI->arg_size()-1ull))) {
            switch (TrTy->getSExtValue()) {
              case 2: // D->H
                break; // we only care about instrumenting copies that count as reading read by the host
              case 0: // H->H
              case 1: // H->D
              case 3: // D->D
              case 4: // D->D (no CU)
              default: // Unknown
                return;
            }
          } else { // could not determine value of direction argument at compiletime
            llvm::errs() << "\n[scabbard.instr.host:ERR] ERROR: direction of memcpy is not determinable at compile time! (from: `" << getSrcLoc() << "`)\n";
            return;
          }
        } else if (not fnName.contains("DtoH")) return; // if not device to host and not determined in args
        auto ci = llvm::CallInst::Create(
          host.trace_append$alloc,
          llvm::ArrayRef<llvm::Value*>(std::array<llvm::Value*,4>{
              llvm::ConstantInt::get(
                  llvm::IntegerType::get(F.getContext(), sizeof(InstrData) * 8),
                  llvm::APInt(sizeof(InstrData)*8, InstrData::READ | InstrData::DEVICE_HEAP | InstrData::ON_HOST | InstrData::_OPT_USED)
                ),
              CI->getArgOperand(0ull),
              loc.get_as_constant(F.getContext()),
              CI->getArgOperand(2ull)
            })
        );
        ci->insertBefore(CI);
        ci->setDebugLoc(CI->getDebugLoc());
        return;
      }
      else if (fnName.starts_with("hipHostRegister")) 
      {}
      else if (fnName.starts_with("hipHostUnregister")) 
      {}
    }


    llvm::GetElementPtrInst* expand_param_args_alloc(llvm::AllocaInst& alloc)
    {
      auto oldAllocTy = alloc.getAllocatedType();
      size_t old_size = 0ul;
      if (auto arrTy = llvm::dyn_cast<llvm::ArrayType>(oldAllocTy)) { // case: alloc array type
        old_size = arrTy->getNumElements();
      } else if (auto C = llvm::dyn_cast<llvm::ConstantInt>(alloc.getArraySize())) { // case: built in alloc array concept
        old_size = C->getSExtValue();
      } else {  // case: unknown size 
        LLVM_DEBUG(llvm::errs() << "\n[scabbard.instr.ERR] could not determine size of alloc instr!\n";);
        old_size = 1;
      }
      auto plainPtrTy = llvm::PointerType::get(alloc.getContext(),0ul);
      // auto newAllocTy = llvm::ArrayType::get(plainPtrTy, old_size+1);
      auto newAlloc = new llvm::AllocaInst(
                              plainPtrTy,
                              0u,
                              llvm::ConstantInt::get(
                                  llvm::IntegerType::get(alloc.getContext(), 32u),
                                  llvm::APInt(32u, old_size+1)
                                ),
                              "instrParamAlloc",
                              &alloc
                            );
      newAlloc->setDebugLoc(alloc.getDebugLoc()); //might cause issues after alloc is deleted
      auto memLoc = llvm::GetElementPtrInst::Create(
                      plainPtrTy,
                      newAlloc, 
                      llvm::ArrayRef<llvm::Value*>(std::array<llvm::Value*,1>{
                          llvm::ConstantInt::get(
                              llvm::IntegerType::get(alloc.getContext(), 64u),
                              llvm::APInt(64u, 0u)
                            )
                        })
                      );
      memLoc->insertAfter(newAlloc);
      memLoc->setDebugLoc(alloc.getDebugLoc()); //might cause issues after alloc is deleted
      alloc.replaceAllUsesWith(memLoc); //DBG: does not seem to be working
      alloc.eraseFromParent();
      return llvm::GetElementPtrInst::Create(
                plainPtrTy,
                newAlloc, 
                llvm::ArrayRef<llvm::Value*>(std::array<llvm::Value*,1>{
                    llvm::ConstantInt::get(
                        llvm::IntegerType::get(alloc.getContext(), 64u),
                        llvm::APInt(64u, old_size)
                      )
                  })
              );
    }


    void ScabbardPassPlugin::instr_launch_func_host(llvm::Function& F, llvm::CallInst& CI)
    {
      // instrument in `scabbard.trace.register_job` before this function and instrument in `scabbard.trace.register_job_callback` after this function call
      auto regFn = llvm::CallInst::Create(
          host.register_job.getFunctionType(),
          host.register_job.getCallee(),
          llvm::ArrayRef<llvm::Value*>(std::array<llvm::Value*,1>{
              CI.getArgOperand(7)
            })
        );
      regFn->insertBefore(&CI);
      regFn->setDebugLoc(CI.getDebugLoc()); //might cause issues if in a device stub
      auto loc = ((CI.getDebugLoc()) // hip generated device stubs have no debug location data so must accommodate
                    ? metadata.trace(F, CI.getDebugLoc(), ModuleType::HOST) 
                    : MetadataHandler::get_hipAPI_loc());
      auto regCbFn = llvm::CallInst::Create(
          host.register_job_callback.getFunctionType(),
          host.register_job_callback.getCallee(),
          llvm::ArrayRef<llvm::Value*>(std::array<llvm::Value*,3>{
              regFn,
              CI.getArgOperand(7),
              loc.get_as_constant(F.getContext())
            })
        );
      regCbFn->insertAfter(&CI);
      regCbFn->setDebugLoc(CI.getDebugLoc()); //might cause issues if in a device stub
      //TODO? modify the type of the last operand (should be a global or function pass)
      // trace back args var and expand it to include the pointer to the DeviceTracker that is returned as the result of `scabbard.trace.register_job` as the last parameter
      auto plainPtrTy = llvm::PointerType::get(F.getContext(),0ul);
      llvm::GetElementPtrInst* paramPtr = nullptr;
      if (auto argElmPtr = llvm::dyn_cast<llvm::GetElementPtrInst>(CI.getArgOperand(5))) { // case: >=2 function parameter length
        if (auto argAlloc = llvm::dyn_cast<llvm::AllocaInst>(argElmPtr->getPointerOperand())) {
          paramPtr = expand_param_args_alloc(*argAlloc);
          if (paramPtr != nullptr)
            argElmPtr->replaceAllUsesWith(paramPtr);
        } else {
          llvm::errs() << "\n[scabbard.instr.host:ERROR] kernel launch user args could not be traced to param args construct allocation\n";
        }
      } else if (auto argAlloc = llvm::dyn_cast<llvm::AllocaInst>(CI.getArgOperand(5))) { // case: single or zero function parameter length
        paramPtr = expand_param_args_alloc(*argAlloc);
      } else {
        llvm::errs() << "\n[scabbard.instr.host:DBG] kernel launch user args are not loaded from local frame\n```\n";
        CI.getArgOperand(5)->print(llvm::errs());
        llvm::errs() << "\n```\n\n";
      }
      if (paramPtr == nullptr) {
        llvm::errs() << "\n[scabbard.instr:ERROR] could not instrument kernel call (instrumentation failed)\n";
        return;
      }
      auto dtAlloc = new llvm::AllocaInst(plainPtrTy, 0u, "dtPtr", regFn);
      dtAlloc->setDebugLoc(CI.getDebugLoc()); //might cause issues if in a device stub
      paramPtr->insertAfter(regFn);
      paramPtr->setDebugLoc(CI.getDebugLoc()); //might cause issues if in a device stub
      auto dtStore = new llvm::StoreInst(regFn, dtAlloc, paramPtr);
      dtStore->setDebugLoc(CI.getDebugLoc()); //might cause issues if in a device stub
      auto dtParamStore = new llvm::StoreInst(dtAlloc, paramPtr, &CI);
      dtParamStore->setDebugLoc(CI.getDebugLoc()); //might cause issues if in a device stub
    }

    // void ScabbardPassPlugin::instr_call_host(const llvm::Function& F, llvm::CallInst* ci)
    // {
    //   const auto& fn = *ci->getCalledFunction();
    //   const auto& fnTy = *fn.getFunctionType();
    //   const auto fnName = fn.getName();
    //   if (fnName.startswith("hip") || fnName.startswith("cuda")) {
    //     if (fnName.contains("Host")) { // deal with all host specific functions separately (for organizational purposes)
    //       instr_host_call_host(F,ci,fnTy,fnName);
    //     } else if (fnName.contains("Stream")) { // deal with all host specific functions separately (for organizational purposes)
    //       instr_stream_call_host(F, ci,fnTy,fnName); 
    //     } else if (fnName.contains("Memcpy")) { // deal with all various hipMemcpy specific fn's separately (for organizational purposes)
    //       instr_memcpy_call_host(F,ci,fnTy,fnName);
    //     } else if (fnName.contains("Memset")) { // deal with the various hipMemset funcs
    //       //TODO
    //     } else if (fnName.contains("Malloc")) { // deal with the various hipMalloc funcs 
    //       //TODO
    //     } else if (fnName.contains("Free")) { // deal with the various hipFree funcs 
    //       //TODO
    //     } else if (fnName.contains("LaunchKernel")) { // deal with kernel launch funcs (this one is complicated)
    //       //TODO
    //     }
    //   }
    // }
    
    void ScabbardPassPlugin::instr_stream_call_host(const llvm::Function& F, llvm::CallInst* ci,
                                                    const llvm::FunctionType& FnTy, const llvm::StringRef FnName) 
    {
      if (FnName.contains("Create")) { // handle stream creation
        //TODO
      } else if (FnName.contains("Destroy")) { // handle stream forced destruction
        //TODO
      } else if (FnName.contains("Synchronize")) { // handle the stream synchronize event
        //TODO very important 
      }
    }
    
    void ScabbardPassPlugin::instr_host_call_host(const llvm::Function& F, llvm::CallInst* ci,
                                                  const llvm::FunctionType& FnTy, const llvm::StringRef FnName) 
    {
      if (FnName.contains("Register")) {
        //TODO
      }
    }

    void ScabbardPassPlugin::instr_memcpy_call_host(const llvm::Function& F, llvm::CallInst* ci,
                                                    const llvm::FunctionType& FnTy, const llvm::StringRef FnName) 
    {
      //TODO
    }

    // const llvm::ImmutableMap<std::string, llvm::Function> ScabbardPassPlugin::HostInstrLibFuncs = {
    //   {
    //     "host.trace_append$mem",
    //     llvm::Function(new llvm::FunctionType(,,false))
    //   },
    //   {
    //     "host.trace_append$mem$cond",
    //   },
    //   {
    //     "host.trace_append$mem$atomic",
    //   },
    //   {
    //     "host.trace_append$mem$atomic_cond",
    //   },
    //   {
    //     "host.trace_append$barrier",
    //   },
    // };
    // const llvm::ImmutableMap<std::string, llvm::Function> ScabbardPassPlugin::DeviceInstrLibFuncs = {
    //   {
    //     "device.trace_append$mem",
    //   },
    //   {
    //     "device.trace_append$barrier",
    //   },
    // };

    // char LegacyScabbard::ID = 0;

  } //?namespace instr
} //?namespace scabbard

#undef DEBUG_TYPE // "scabbard"
