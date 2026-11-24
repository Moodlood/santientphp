#include "CriticalEdgeSplittingPass.hpp"
#include "Dominance.hpp"
#include "compiler/ir/IRInstruction.hpp"
#include <algorithm>
#include <vector>

namespace sph {

bool isCriticalEdge(const IRBasicBlock& src, const IRBasicBlock& dst, const IRFunction& fn) {
    auto succs = getSuccessors(src);
    if (std::find(succs.begin(), succs.end(), &dst) == succs.end()) return false;
    if (succs.size() <= 1) return false;

    auto preds = getPredecessors(dst, fn);
    return preds.size() > 1;
}

bool splitCriticalEdges(IRFunction& fn) {
    bool changed = false;

    // Collect critical edges first to avoid iterator invalidation when creating new blocks
    struct Edge {
        IRBasicBlock* src;
        IRBasicBlock* dst;
    };
    std::vector<Edge> criticalEdges;

    for (const auto& bb : fn.getBlocks()) {
        auto succs = getSuccessors(*bb);
        if (succs.size() > 1) {
            for (auto* succ : succs) {
                if (succ && getPredecessors(*succ, fn).size() > 1) {
                    criticalEdges.push_back({bb.get(), succ});
                }
            }
        }
    }

    if (criticalEdges.empty()) {
        return false;
    }

    for (const auto& edge : criticalEdges) {
        IRBasicBlock* src = edge.src;
        IRBasicBlock* dst = edge.dst;

        // Create new intermediate block
        IRBasicBlock* splitBlock = fn.createBlock(src->getName() + "_" + dst->getName() + "_split");
        splitBlock->addInstruction(std::make_shared<BranchInst>(dst));

        // Update terminator of src
        auto* term = src->getTerminator();
        if (term && term->getOpcode() == IROpcode::CondBr) {
            auto* condBr = static_cast<CondBranchInst*>(term);
            if (condBr->getTrueTarget() == dst) {
                condBr->setTrueTarget(splitBlock);
            } else if (condBr->getFalseTarget() == dst) {
                condBr->setFalseTarget(splitBlock);
            }
        } else if (term && term->getOpcode() == IROpcode::Br) {
            auto* br = static_cast<BranchInst*>(term);
            if (br->getTarget() == dst) {
                br->setTarget(splitBlock);
            }
        }

        // Update any Phi instructions in dst
        for (const auto& inst : dst->getInstructions()) {
            if (inst->getOpcode() == IROpcode::Phi) {
                auto& phi = static_cast<PhiInst&>(*inst);
                auto& incoming = phi.getIncomingMutable();
                for (size_t i = 0; i < incoming.size(); ++i) {
                    if (incoming[i].first == src) {
                        phi.setIncomingBlock(i, splitBlock);
                    }
                }
            } else {
                break; // Phis are only at the start of the block
            }
        }

        changed = true;
    }

    return changed;
}

bool CriticalEdgeSplittingPass::runOnFunction(IRFunction& function) {
    return splitCriticalEdges(function);
}

} // namespace sph
