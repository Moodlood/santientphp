#include "PassManager.hpp"
#include "AlgebraicSimplificationPass.hpp"
#include "CommonSubexpressionEliminationPass.hpp"
#include "ConstantFoldingPass.hpp"
#include "ControlFlowOptimizationPass.hpp"
#include "DeadCodeEliminationPass.hpp"
#include "DeadStoreEliminationPass.hpp"
#include "EscapeAnalysisPass.hpp"
#include "FunctionInliningPass.hpp"
#include "LoopInvariantCodeMotionPass.hpp"
#include "LoopUnrollPass.hpp"
#include "Mem2RegPass.hpp"
#include "StoreToLoadForwardingPass.hpp"
#include "StrengthReductionPass.hpp"
#include "TailCallPass.hpp"

#include "compiler/ir/IRVerifier.hpp"
#include <iostream>

namespace sph {

void PassManager::addFunctionPass(std::unique_ptr<FunctionPass> pass) {
    if (pass) m_functionPasses.push_back(std::move(pass));
}

void PassManager::addModulePass(std::unique_ptr<ModulePass> pass) {
    if (pass) m_modulePasses.push_back(std::move(pass));
}

PassManager PassManager::createForLevel(OptimizationLevel level) {
    PassManager pm(level);

    if (level == OptimizationLevel::O0) {
        return pm;
    }

    if (level == OptimizationLevel::O1) {
        pm.addFunctionPass(std::make_unique<Mem2RegPass>());
        pm.addFunctionPass(std::make_unique<StoreToLoadForwardingPass>());
        pm.addFunctionPass(std::make_unique<EscapeAnalysisPass>());
        pm.addFunctionPass(std::make_unique<ConstantFoldingPass>());
        pm.addFunctionPass(std::make_unique<AlgebraicSimplificationPass>());
        pm.addFunctionPass(std::make_unique<StrengthReductionPass>());
        pm.addFunctionPass(std::make_unique<TailCallPass>());
        pm.addFunctionPass(std::make_unique<DeadStoreEliminationPass>());
        pm.addFunctionPass(std::make_unique<DeadCodeEliminationPass>());
        return pm;
    }

    if (level == OptimizationLevel::O2 || level == OptimizationLevel::Os) {
        pm.addFunctionPass(std::make_unique<Mem2RegPass>());
        pm.addFunctionPass(std::make_unique<StoreToLoadForwardingPass>());
        pm.addFunctionPass(std::make_unique<EscapeAnalysisPass>());
        pm.addFunctionPass(std::make_unique<ConstantFoldingPass>());
        pm.addFunctionPass(std::make_unique<AlgebraicSimplificationPass>());
        pm.addFunctionPass(std::make_unique<CommonSubexpressionEliminationPass>());
        pm.addFunctionPass(std::make_unique<LoopInvariantCodeMotionPass>());
        pm.addFunctionPass(std::make_unique<StrengthReductionPass>());
        pm.addFunctionPass(std::make_unique<ControlFlowOptimizationPass>());
        pm.addFunctionPass(std::make_unique<TailCallPass>());
        pm.addFunctionPass(std::make_unique<DeadStoreEliminationPass>());
        pm.addFunctionPass(std::make_unique<DeadCodeEliminationPass>());
        return pm;
    }

    if (level == OptimizationLevel::Oz) {
        // Size-constrained: no inlining, maximum dead store and dead code removal
        pm.addFunctionPass(std::make_unique<Mem2RegPass>());
        pm.addFunctionPass(std::make_unique<StoreToLoadForwardingPass>());
        pm.addFunctionPass(std::make_unique<EscapeAnalysisPass>());
        pm.addFunctionPass(std::make_unique<ConstantFoldingPass>());
        pm.addFunctionPass(std::make_unique<CommonSubexpressionEliminationPass>());
        pm.addFunctionPass(std::make_unique<ControlFlowOptimizationPass>());
        pm.addFunctionPass(std::make_unique<TailCallPass>());
        pm.addFunctionPass(std::make_unique<DeadStoreEliminationPass>());
        pm.addFunctionPass(std::make_unique<DeadCodeEliminationPass>());
        return pm;
    }

    if (level == OptimizationLevel::O3) {
        pm.addModulePass(std::make_unique<FunctionInliningPass>(16));
        pm.addFunctionPass(std::make_unique<Mem2RegPass>());
        pm.addFunctionPass(std::make_unique<StoreToLoadForwardingPass>());
        pm.addFunctionPass(std::make_unique<EscapeAnalysisPass>());
        pm.addFunctionPass(std::make_unique<ConstantFoldingPass>());
        pm.addFunctionPass(std::make_unique<AlgebraicSimplificationPass>());
        pm.addFunctionPass(std::make_unique<CommonSubexpressionEliminationPass>());
        pm.addFunctionPass(std::make_unique<LoopInvariantCodeMotionPass>());
        pm.addFunctionPass(std::make_unique<StrengthReductionPass>());
        pm.addFunctionPass(std::make_unique<LoopUnrollPass>(4));
        pm.addFunctionPass(std::make_unique<ControlFlowOptimizationPass>());
        pm.addFunctionPass(std::make_unique<TailCallPass>());
        pm.addFunctionPass(std::make_unique<DeadStoreEliminationPass>());
        pm.addFunctionPass(std::make_unique<DeadCodeEliminationPass>());
        return pm;
    }

    return pm;
}

bool PassManager::run(IRModule& module) {
    m_verificationFailed = false;
    if (m_level == OptimizationLevel::O0 && m_functionPasses.empty() && m_modulePasses.empty()) {
        return false;
    }

    auto verifyPass = [&](std::string_view passName, const std::string& context) {
        if (!m_verifyEachPass) return;
        std::vector<std::string> verifierErrors;
        IRVerifier verifier;
        if (!verifier.verify(module, verifierErrors)) {
            m_verificationFailed = true;
            std::cerr << "[PassManager Error] IR verification failed after " << passName
                      << " on " << context << " (" << verifierErrors.size() << " errors):\n";
            for (const auto& err : verifierErrors) {
                std::cerr << "  - " << err << "\n";
            }
        }
    };

    bool anyChanged = false;

    // Run module passes
    for (auto& mp : m_modulePasses) {
        if (mp->runOnModule(module)) {
            anyChanged = true;
        }
        verifyPass(mp->getName(), "module");
    }

    // Run function passes iteratively until fixed point or max iterations
    size_t maxIterations = (m_level >= OptimizationLevel::O2) ? 8 : 1;

    for (size_t iter = 0; iter < maxIterations; ++iter) {
        bool iterChanged = false;
        for (auto& fn : module.getFunctions()) {
            if (fn->isDeclaration()) continue;
            for (auto& fp : m_functionPasses) {
                if (fp->runOnFunction(*fn)) {
                    iterChanged = true;
                    anyChanged = true;
                }
                verifyPass(fp->getName(), "function '@" + fn->getName() + "'");
            }
        }
        if (!iterChanged) break;
    }

    // Synchronize parent basic block pointers on all instructions across all functions
    for (auto& fn : module.getFunctions()) {
        if (fn->isDeclaration()) continue;
        for (auto& bb : fn->getBlocks()) {
            if (!bb) continue;
            for (auto& inst : bb->getInstructions()) {
                if (inst) {
                    inst->setParent(bb.get());
                }
            }
        }
    }

    return anyChanged;
}

} // namespace sph

