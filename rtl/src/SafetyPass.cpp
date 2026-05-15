/**
 * @file registry.cpp
 * @author osterhoutan (osterhoutan+scabbard@gmail.com)
 * @brief This is where the messy code for registering a pass resides
 * 
 * @version alpha 0.0.1
 * @date 2023-04-20
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <llvm/Pass.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/IR/PassManager.h>

namespace /* anon */ {

/// Add the \c disable_sanitizer_instrumentation attribute to all
///  functions in a module so that they don't get instrumented 
///  by a sanitizer instrumenter like asan, tsan or scabbard.
struct SafetyPass : llvm::PassInfoMixin<SafetyPass> {
    SafetyPass() = default;
    llvm::PreservedAnalyses run(llvm::Module& M, llvm::ModuleAnalysisManager& FAM) {
      for (llvm::Function& F : M)
        if (not F.hasFnAttribute("disable_sanitizer_instrumentation"))
          F.addFnAttr("disable_sanitizer_instrumentation");
      return llvm::PreservedAnalyses::all();
    }
    static bool isRequired() { return true; }
};

} //? namespace <anon>



// << ========================================================================================== >> 
// <<                           NEW PASS-MANAGER PASS PLUGIN REGISTRY                            >> 
// << ========================================================================================== >> 

// scabbard::instr::MetadataHandler metadata;

/* New PM Registration */
llvm::PassPluginLibraryInfo getSafetyPassPluginInfo() {
  using namespace llvm;
  return {LLVM_PLUGIN_API_VERSION, "safety", "1.0.1",
          [](PassBuilder& PB) {
            
              PB.registerOptimizerLastEPCallback( // used to handle link time instrumentation
                  [](llvm::ModulePassManager &MPM, OptimizationLevel level) {
                    MPM.addPass(SafetyPass());
                  }
                );
          }};
}
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getSafetyPassPluginInfo();
}
