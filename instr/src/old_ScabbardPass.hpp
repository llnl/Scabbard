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

#pragma once

#include "DepTrace.hpp"
#include "MetadataHandler.hpp"

#include <scabbard/instr-names.def>

#include <llvm/Pass.h> 
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Intrinsics.h>

#include <string>
#include <memory>
#include <unordered_set>

namespace scabbard {
namespace instr {


  
  // bool runScabbardPass(llvm::Module& M);

  
  // bool runScabbardPass(llvm::Function& F);



  // struct LegacyScabbard : public llvm::FunctionPass {
  //   static char ID;
  // public:
  //   LegacyScabbard() : FunctionPass(ID) {}
  //   bool doInitialization(llvm::Module& M) override { return runScabbardPass(M); }
  //   bool runOnFunction(llvm::Function& F) override { return runScabbardPass(F); }
  // };//?END class LegacyScabbard

  
  
  struct ScabbardPassPlugin : llvm::PassInfoMixin<ScabbardPassPlugin> {
  protected:
    std::string source_loc = "";
    std::size_t archBit = 64;
    bool isLTO = false;

    // std::vector<std::tuple<llvm::Function*,llvm::Function*,llvm::MDNode*>> to_replace;
    std::vector<std::tuple<llvm::Function*,llvm::Function*>> to_replace;

    MetadataHandler metadata;

    struct {
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
      } host;
    struct {
        llvm::FunctionCallee trace_append$mem;
        const std::string trace_append$mem_name = SCABBARD_DEVICE_CALLBACK_APPEND_MEM_NAME;
        llvm::FunctionCallee trace_append$alloc;
        const std::string trace_append$alloc_name = SCABBARD_DEVICE_CALLBACK_APPEND_ALLOC_NAME;
        llvm::FunctionCallee dummyFunc;
        const std::unordered_set<std::string> NO_INSTR_FNS{
                SCABBARD_DEVICE_CALLBACK_APPEND_MEM_NAME,
                SCABBARD_DEVICE_CALLBACK_APPEND_ALLOC_NAME,
                SCABBARD_DEVICE_DUMMY_FUNC_NAME,
                "__ockl_hostcall_internal",
                "__cxa_pure_virtual",
                "__cxa_deleted_virtual", 
                "__assertfail",
                "__assert_fail"
              };
        // const std::vector<std::string> TRACE_LIB_FNS_TO_CLONE{
        //         trace_append$mem_name,
        //         trace_append$alloc_name
        //       };
        // const std::unordered_map<std::string, llvm::Intrinsic::ID> REQUIRED_BUILTINS{
        //         {"llvm.amdgcn.workgroup.id.x",  llvm::Intrinsic::not_intrinsic},
        //         {"llvm.amdgcn.workgroup.id.y",  llvm::Intrinsic::not_intrinsic},
        //         {"llvm.amdgcn.workgroup.id.z",  llvm::Intrinsic::not_intrinsic},
        //         {"llvm.amdgcn.workitem.id.x",   llvm::Intrinsic::not_intrinsic},
        //         {"llvm.amdgcn.workitem.id.y",   llvm::Intrinsic::not_intrinsic},
        //         {"llvm.amdgcn.workitem.id.z",   llvm::Intrinsic::not_intrinsic},
        //         {"llvm.lifetime.start.p5",      llvm::Intrinsic::lifetime_start},
        //         {"llvm.lifetime.end.p5",        llvm::Intrinsic::lifetime_end},
        //         {"llvm.memcpy.p0.p5.i64",       llvm::Intrinsic::memcpy},
        //       };
        llvm::Metadata* DeviceTrackerPtrTy_metadata = nullptr;
      } device;


  public:

    ScabbardPassPlugin(bool isLTO_=false);
    // ScabbardPassPlugin(MetadataHandler& metadataHandler_);
    // ScabbardPassPlugin(const std::string& InstrLibLoc);
    // ~ScabbardPassPlugin() = default;

    /**
     * @brief \em fn \c run : (llvm::Module& -> bool) \n
     *        The \c doInitialization method is allowed to do most of the things that \c FunctionPasses
     *         are not allowed to do. 
     *        They can add and remove functions, 
     *         get pointers to functions, \em etc. 
     *        The \c doInitialization method is designed to do simple initialization type of stuff that
     *         does not depend on the functions being processed. 
     *        The \c doInitialization method call is not scheduled to overlap with any other pass executions 
     *         (thus it should be very fast).
     * @param M \c llvm::Module& - The module being processed
     * @return \c llvm::PreservedAnalyses - the relevant analysis that are still valid
     */
    llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &MAM);


    /**
     * @brief tells the pass manager that this pass can't be skipped
     */
    static bool isRequired() { return true; }
    

  protected:


    // /**
    //  * @brief Map of functions that exist in scabbard lib that should be inserted into any host module
    //  */
    // static const llvm::ImmutableMap<std::string, llvm::Function> HostInstrLibFuncs;
    // /**
    //  * @brief Map of functions that exist in scabbard lib that should be inserted into all device modules
    //  */
    // static const llvm::ImmutableMap<std::string, llvm::Function> DeviceInstrLibFuncs;
    

    /**
     * @brief just run coded specifically for kernel/device Modules
     * @param M \c llvm::Module& - The device side Module to instrument
     * @param MAM \c llvm::ModuleAnalysisManager& - The Analysis Manager for the Module
     */
    auto run_device(llvm::Module& M, llvm::ModuleAnalysisManager &MAM) -> void;
    /**
     * @brief just run coded specifically for host/CPU Modules
     * @param M \c llvm::Module& - The device side Module to instrument
     * @param MAM \c llvm::ModuleAnalysisManager& - The Analysis Manager for the Module
     */
    auto run_host(llvm::Module& M, llvm::ModuleAnalysisManager &MAM) -> void;
    /**
     * @brief just run coded specifically for kernel/device functions
     * @param F \c llvm::Function& - The device side function to instrument
     * @param FAM \c llvm::FunctionAnalysisManager& - The Analysis Manager for the function
     */
    auto run_device(llvm::Function& F, llvm::FunctionAnalysisManager &FAM, const DepTraceDevice& DT) -> void;
    /**
     * @brief just run coded specifically for host/CPU functions
     * @param F \c llvm::Function& - The device side function to instrument
     * @param FAM \c llvm::FunctionAnalysisManager& - The Analysis Manager for the function
     */
    auto run_host(llvm::Function& F, llvm::FunctionAnalysisManager &FAM, const DepTraceHost& DT) -> void;

    /**
     * @brief Instrument device module to have all necessary callback functions
     * @param M \c llvm::Module& - The device side Module to instrument
     * @param MAM \c llvm::ModuleAnalysisManager& - The Analysis Manager for the Module
     */
    auto instrCallbacks_device(llvm::Module& M, llvm::ModuleAnalysisManager &MAM) -> void;
    /**
     * @brief Instrument host module to have all necessary callback functions
     * @param M \c llvm::Module& - The host side Module to instrument
     * @param MAM \c llvm::ModuleAnalysisManager& - The Analysis Manager for the Module
     */
    auto instrCallbacks_host(llvm::Module& M, llvm::ModuleAnalysisManager &MAM) -> void;
    /**
     * @brief 
     * @param F \c llvm::Function& - The device side function to instrument
     * @param FAM \c llvm::FunctionAnalysisManager& - The Analysis Manager for the function
     */
    auto instr_mainFunc_host(llvm::Function& F, llvm::FunctionAnalysisManager &FAM) -> void;

    auto instr_call_device(const llvm::Function& F, llvm::CallInst* ci) -> void;
    auto instr_call_host(llvm::Function& F, llvm::CallInst* ci) -> void;
    
    auto instr_stream_call_host(const llvm::Function& F, llvm::CallInst* ci,
                                const llvm::FunctionType& FnTy, const llvm::StringRef FnName) -> void;
    auto instr_host_call_host(const llvm::Function& F, llvm::CallInst* ci,
                                const llvm::FunctionType& FnTy, const llvm::StringRef FnName) -> void;
    auto instr_memcpy_call_host(const llvm::Function& F, llvm::CallInst* ci,
                                const llvm::FunctionType& FnTy, const llvm::StringRef FnName) -> void;

    
    auto instr_mem_func_device(llvm::Function& F, 
                               llvm::Instruction& I, llvm::Value* V, 
                               const InstrData& data, bool InsertAfter=true) -> void;
    auto instr_mem_func_host(llvm::Function& F, 
                              llvm::Instruction& I, llvm::Value* V, 
                              const InstrData& data, bool InsertAfter=true) -> void;

    auto instr_launch_func_host(llvm::Function& F,
                                llvm::CallInst& CI) -> void;

    auto replace_device_function(llvm::Function& F) -> llvm::Function*;
    auto finish_replacing_old_funcs_device(llvm::Module& M) -> void;

    // auto clone_scabbard_functions_device(llvm::Module& M, const llvm::Module& LibM) -> void;
    
  };//?END class ScabbardPassPlugin

} //?namespace instr
} //?namespace scabbard

