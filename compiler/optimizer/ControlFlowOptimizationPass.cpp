#include "ControlFlowOptimizationPass.hpp"
#include <unordered_map>

namespace sph {

bool ControlFlowOptimizationPass::simplifyConditionalBranches(IRFunction& fn) {
    bool changed = false;

    for (auto& bb : fn.getBlocks()) {
        auto& insts = bb->getInstructions();
        if (insts.empty()) continue;

        auto& lastInst = insts.back();
        if (lastInst->getOpcode() == IROpcode::CondBr) {
            auto condBr = std::static_pointer_cast<CondBranchInst>(lastInst);
            if (condBr->getTrueTarget() == condBr->getFalseTarget()) {
                insts.back() = std::make_shared<BranchInst>(condBr->getTrueTarget());
                changed = true;
            }
        }
    }

    return changed;
}

bool ControlFlowOptimizationPass::bypassTrampolineBlocks(IRFunction& fn) {
    bool changed = false;

    // Identify blocks that only contain an unconditional branch
    std::unordered_map<IRBasicBlock*, IRBasicBlock*> trampolines;
    for (auto& bb : fn.getBlocks()) {
        if (bb.get() == fn.getEntryBlock()) continue; // Don't bypass the entry block

        const auto& insts = bb->getInstructions();
        if (insts.size() == 1 && insts[0]->getOpcode() == IROpcode::Br) {
            auto br = std::static_pointer_cast<BranchInst>(insts[0]);
            if (br->getTarget() && br->getTarget() != bb.get()) {
                trampolines[bb.get()] = br->getTarget();
            }
        }
    }

    if (trampolines.empty()) return false;

    // Resolve chained trampolines (A -> B -> C => A -> C)
    for (auto& [from, to] : trampolines) {
        IRBasicBlock* curr = to;
        int depth = 0;
        while (trampolines.find(curr) != trampolines.end() && depth < 16) {
            curr = trampolines[curr];
            depth++;
        }
        if (curr != from) {
            to = curr;
        }
    }

    // Update branches
    for (auto& bb : fn.getBlocks()) {
        for (auto& inst : bb->getInstructions()) {
            if (inst->getOpcode() == IROpcode::Br) {
                auto br = std::static_pointer_cast<BranchInst>(inst);
                auto it = trampolines.find(br->getTarget());
                if (it != trampolines.end() && it->second != br->getTarget()) {
                    br->setTarget(it->second);
                    changed = true;
                }
            } else if (inst->getOpcode() == IROpcode::CondBr) {
                auto condBr = std::static_pointer_cast<CondBranchInst>(inst);
                auto itTrue = trampolines.find(condBr->getTrueTarget());
                if (itTrue != trampolines.end() && itTrue->second != condBr->getTrueTarget()) {
                    condBr->setTrueTarget(itTrue->second);
                    changed = true;
                }
                auto itFalse = trampolines.find(condBr->getFalseTarget());
                if (itFalse != trampolines.end() && itFalse->second != condBr->getFalseTarget()) {
                    condBr->setFalseTarget(itFalse->second);
                    changed = true;
                }
            }
        }
    }

    return changed;
}

bool ControlFlowOptimizationPass::runOnFunction(IRFunction& fn) {
    bool changed = false;
    if (simplifyConditionalBranches(fn)) changed = true;
    if (bypassTrampolineBlocks(fn)) changed = true;
    return changed;
}

} // namespace sph

