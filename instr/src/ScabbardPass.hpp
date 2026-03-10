/**
 * @file Scabbard.h
 * @author Andrew Osterhout (osterhoutan+scabbard@gmail.com)
 * @brief The instrumentation pass for the gpu thread sanitizer
 *
 * @date 2024-09-17
 *
 * @copyright Copyright (c) 2024
 * @license APACHE 2.0 with llvm exceptions
 *
 */

#ifndef LLVM_TRANSFORMS_INSTRUMENTATION_SCABBARD_H
#define LLVM_TRANSFORMS_INSTRUMENTATION_SCABBARD_H

#include <llvm/IR/PassManager.h>

namespace llvm {

class Function;
class Module;

/// Instrumenter for gpu thread sanitizer
/// It needs to be run on both device and host modules.
/// The pass assumes that for the device side that gpu-rdc is being used
/// and that all linking has been completed before the pass is run
///  (no dynamic runtime modules or dynamic libs for the device).
///
struct ScabbardPass : llvm::PassInfoMixin<ScabbardPass> {
    const bool IS_LTO;
    ScabbardPass(bool isLTO=false) : IS_LTO(isLTO) {}
    PreservedAnalyses run(Module& M, ModuleAnalysisManager& FAM);
    static bool isRequired() { return true; }
};

} // namespace llvm

#endif /* LLVM_TRANSFORMS_INSTRUMENTATION_SCABBARD_H */