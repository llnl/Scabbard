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

#include "ScabbardPass.hpp"
// #include "ScabbardPostPass.hpp"
#include "MetadataHandler.hpp"

#include <llvm/Pass.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/Support/CommandLine.h>
// #include <clang/Frontend/FrontendAction.h>
// #include <clang/Frontend/FrontendPluginRegistry.h>
// #include <clang/AST/RecursiveASTVisitor.h>
// #include <clang/AST/DeclCXX.h>


// << ========================================================================================== >> 
// <<                                COMAND LINE OPTION REGISTRY                                 >> 
// << ========================================================================================== >> 


// << ======================= Scababrd Instrumenter CL Options Registry ======================== >> 
// command line option for the scabbard instrumentation pass
static llvm::cl::opt<bool> ScabbardPassOpt("scabbard", llvm::cl::init(false), 
                                            llvm::cl::desc("scabbard, a HIP based data race detection system's instrumenter for generating a trace generator"));
static llvm::cl::opt<std::string> ScabbardLibOpt("scabbard-lib", llvm::cl::init("./build/lib/libscabbard.so"), 
                                                  llvm::cl::desc("path to the general scabbard lib (not the instrumeter lib). Use if compiler/linker can't find it by default."));



// << ========================================================================================== >> 
// <<                                   LEGACY PASS REGISTRY                                     >> 
// << ========================================================================================== >> 

// static llvm::RegisterPass<scabbard::instr::LegacyScabbard> X("scabbard-leg", "scabbard instrumentation pass",
//                                                               false, false);




// << ========================================================================================== >> 
// <<                           NEW PASS-MANAGER PASS PLUGIN REGISTRY                            >> 
// << ========================================================================================== >> 

// scabbard::instr::MetadataHandler metadata;

/* New PM Registration */
llvm::PassPluginLibraryInfo getScabbardPassPluginInfo() {
  using namespace llvm;
  return {LLVM_PLUGIN_API_VERSION, "scabbard", "alpha 0.0.1",
          [](PassBuilder& PB) {
            // PB.registerOptimizerEarlyEPCallback( // ~can~ find kernel functions (sometimes run's twice)
            //     [&](llvm::ModulePassManager &MPM, OptimizationLevel level) {
            //       // MPM.addPass(scabbard::instr::ScabbardPassPlugin());
            //       // MPM.addPass(scabbard::instr::ScabbardPostPass(metadata)); // moved to linker phase
            //     }
            //   );
              PB.registerOptimizerLastEPCallback( // used to handle link time instrumentation
                  [](llvm::ModulePassManager &MPM, OptimizationLevel level) {
                    MPM.addPass(scabbard::instr::ScabbardPassPlugin(false));
                    // MPM.addPass(scabbard::instr::ScabbardPostPass(metadata));
                  }
                );
              // PB.registerFullLinkTimeOptimizationEarlyEPCallback(
              //   [](llvm::ModulePassManager &MPM, OptimizationLevel level) {
              //       MPM.addPass(scabbard::instr::ScabbardPassPlugin());
              //       // MPM.addPass(scabbard::instr::ScabbardPostPass(metadata));
              //     }
              // );
              PB.registerFullLinkTimeOptimizationLastEPCallback(
                [](llvm::ModulePassManager &MPM, OptimizationLevel level) {
                    // llvm::errs() << "\n[scabbard.instr.register:DBG] pass plugin registered\n"; //DEBUG
                    MPM.addPass(scabbard::instr::ScabbardPassPlugin(true));
                    // MPM.addPass(scabbard::instr::ScabbardPostPass(metadata));
                  }
              );
          }};
}
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getScabbardPassPluginInfo();
}


// << ========================================================================================== >> 
// <<                                    CLANG PLUGIN STUFF                                      >> 
// << ========================================================================================== >> 
// NOTE: Not currently viable
// namespace {
// struct ScabbardClangPlugin : public clang::PluginASTAction {
//   ScabbardClangPlugin() = default;
//   ~ScabbardClangPlugin() = default;
//   bool ParseArgs(const clang::CompilerInstance &CI,
//                   const std::vector<std::string>& args) {
//                     llvm::errs() << "\n[scabbard::TEST] clang plugin test\n";
//     bool has_dbg_flag = false;
//     bool has_scabbard_lib_link = false;
//     for (unsigned i = 0, e = args.size(); i != e; ++i) {
//       if (args[i] == "-g") 
//         has_dbg_flag = true;
//       else if (args[i] == "-lscabbard") 
//         has_scabbard_lib_link = true;
//     }
//     if (not has_dbg_flag) {
//       llvm::dbgs() << "\n[scabbard::WARNING] scabbard requires the `-g` flag during compilation to provide human understandable feedback\n";
//       return false;
//     }
//     if (not has_scabbard_lib_link) {
//       llvm::errs() << "\n[scabbard::ERROR] scabbard requires you to link to libscabbard durring compilation and linking to properly instrument the code\n";
//       return false;
//     }
//     return true;
//   }
//   struct DummyASTConsumer : public clang::ASTConsumer {
//   public:
//     virtual void HandleTranslationUnit(clang::ASTContext &Context) {}
//   };
//   std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &CI, clang::StringRef InFile) override {
//     return std::unique_ptr<DummyASTConsumer>(new DummyASTConsumer());
//   };
// };
// }

// static clang::FrontendPluginRegistry::Add<ScabbardClangPlugin> P("scabbard", "Unified memory race detector (clang plugin frontend)");
