#include "LoopInvariantCodeMotionPass.hpp"
#include "compiler/ir/IRBasicBlock.hpp"
#include "compiler/ir/IRFunction.hpp"
#include "compiler/ir/IRInstruction.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sph {

namespace {

struct LoopInfo {
    IRBasicBlock* header = nullptr;
    IRBasicBlock* preheader = nullptr;
    std::unordered_set<IRBasicBlock*> blocks;
};

// Find simple natural loops: blocks that jump backwards to a header
std::vector<LoopInfo> findLoops(IRFunction& fn) {
    std::vector<LoopInfo> loops;
    const auto& bbs = fn.getBlocks();
    if (bbs.size() < 2) return loops;

    std::unordered_map<IRBasicBlock*, size_t> blockIndices;
    for (size_t i = 0; i < bbs.size(); ++i) {
        blockIndices[bbs[i].get()] = i;
    }

    for (size_t i = 0; i < bbs.size(); ++i) {
        auto* bb = bbs[i].get();
        auto* term = bb->getTerminator();
        if (!term) continue;

        std::vector<IRBasicBlock*> targets;
        if (auto* br = dynamic_cast<BranchInst*>(term)) {
            targets.push_back(br->getTarget());
        } else if (auto* cbr = dynamic_cast<CondBranchInst*>(term)) {
            targets.push_back(cbr->getTrueTarget());
            targets.push_back(cbr->getFalseTarget());
        }

        for (auto* target : targets) {
            auto it = blockIndices.find(target);
            if (it != blockIndices.end() && it->second <= i) {
                // Back-edge from bbs[i] to bbs[it->second]
                size_t headerIdx = it->second;
                LoopInfo loop;
                loop.header = bbs[headerIdx].get();
                if (headerIdx > 0) {
                    loop.preheader = bbs[headerIdx - 1].get();
                }

                for (size_t b = headerIdx; b <= i; ++b) {
                    loop.blocks.insert(bbs[b].get());
                }
                loops.push_back(loop);
            }
        }
    }

    return loops;
}

bool isValueDefinedOutsideLoop(const IRValue* val, const std::unordered_set<IRBasicBlock*>& loopBlocks) {
    if (!val || val->isConstant()) return true;
    for (auto* bb : loopBlocks) {
        for (const auto& inst : bb->getInstructions()) {
            if (inst.get() == val) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

bool LoopInvariantCodeMotionPass::runOnFunction(IRFunction& fn) {
    auto loops = findLoops(fn);
    if (loops.empty()) return false;

    bool anyChanged = false;

    for (size_t lIdx = 0; lIdx < loops.size(); ++lIdx) {
        auto& loop = loops[lIdx];
        if (!loop.preheader || !loop.header) continue;

        std::vector<std::shared_ptr<IRInstruction>> invariantInsts;

        for (auto* bb : loop.blocks) {
            if (!bb) continue;
            auto& insts = bb->getInstructions();
            std::vector<std::shared_ptr<IRInstruction>> remaining;
            bool blockChanged = false;

            for (const auto& inst : insts) {
                if (!inst) continue;
                if (inst->isTerminator()) {
                    remaining.push_back(inst);
                    continue;
                }
                auto op = inst->getOpcode();
                if (op == IROpcode::Call || op == IROpcode::Store || op == IROpcode::Alloca || op == IROpcode::Load) {
                    remaining.push_back(inst);
                    continue;
                }

                // Check binary instruction operands
                if (auto* bin = dynamic_cast<BinaryInst*>(inst.get())) {
                    if (op == IROpcode::Div || op == IROpcode::Mod) {
                        // Speculative hoisting of Div/Mod is unsafe because divisor may be zero,
                        // triggering hardware SIGFPE. Only hoist if divisor is a proven non-zero constant.
                        auto* constRhs = dynamic_cast<const ConstantInt*>(bin->getRhs().get());
                        if (!constRhs || constRhs->getValue() == 0) {
                            remaining.push_back(inst);
                            continue;
                        }
                    }
                    if (isValueDefinedOutsideLoop(bin->getLhs().get(), loop.blocks) &&
                        isValueDefinedOutsideLoop(bin->getRhs().get(), loop.blocks)) {
                        invariantInsts.push_back(inst);
                        blockChanged = true;
                        continue;
                    }
                }

                remaining.push_back(inst);
            }

            if (blockChanged) {
                insts = std::move(remaining);
                anyChanged = true;
            }
        }

        // Hoist invariant instructions to preheader right before its terminator
        if (!invariantInsts.empty()) {
            auto& preInsts = loop.preheader->getInstructions();
            if (!preInsts.empty() && preInsts.back()->isTerminator()) {
                auto term = preInsts.back();
                preInsts.pop_back();
                for (const auto& inv : invariantInsts) {
                    preInsts.push_back(inv);
                }
                preInsts.push_back(term);
            } else {
                for (const auto& inv : invariantInsts) {
                    preInsts.push_back(inv);
                }
            }
            anyChanged = true;
        }
    }

    return anyChanged;
}

} // namespace sph
