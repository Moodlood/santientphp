#include "LoopInvariantCodeMotionPass.hpp"
#include "compiler/ir/IRBasicBlock.hpp"
#include "compiler/ir/IRFunction.hpp"
#include "compiler/ir/IRInstruction.hpp"
#include "compiler/optimizer/Dominance.hpp"
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

// Find natural loops using DominatorTree
std::vector<LoopInfo> findLoops(IRFunction& fn, const DominatorTree& dt) {
    std::vector<LoopInfo> loops;
    const auto& bbs = fn.getBlocks();
    if (bbs.size() < 2) return loops;

    for (const auto& bbPtr : bbs) {
        if (!bbPtr) continue;
        IRBasicBlock* bb = bbPtr.get();
        for (IRBasicBlock* succ : getSuccessors(*bb)) {
            // A back-edge is an edge bb -> succ where succ dominates bb
            if (dt.dominates(succ, bb)) {
                LoopInfo loop;
                loop.header = succ;
                loop.blocks.insert(succ);

                // Natural loop blocks: walk predecessors backwards from bb until reaching succ
                if (bb != succ) {
                    loop.blocks.insert(bb);
                    std::vector<IRBasicBlock*> worklist;
                    worklist.push_back(bb);
                    while (!worklist.empty()) {
                        IRBasicBlock* cur = worklist.back();
                        worklist.pop_back();
                        for (IRBasicBlock* pred : getPredecessors(*cur, fn)) {
                            if (loop.blocks.insert(pred).second) {
                                worklist.push_back(pred);
                            }
                        }
                    }
                }

                // Determine preheader:
                // Predecessors of loop.header that are NOT in loop.blocks
                std::vector<IRBasicBlock*> outsidePreds;
                for (IRBasicBlock* pred : getPredecessors(*loop.header, fn)) {
                    if (loop.blocks.find(pred) == loop.blocks.end()) {
                        outsidePreds.push_back(pred);
                    }
                }

                // A valid dedicated preheader must be unique and branch directly to the header
                if (outsidePreds.size() == 1) {
                    IRBasicBlock* p = outsidePreds[0];
                    if (auto* term = p->getTerminator()) {
                        if (auto* br = dynamic_cast<BranchInst*>(term)) {
                            if (br->getTarget() == loop.header) {
                                loop.preheader = p;
                            }
                        }
                    }
                }

                loops.push_back(std::move(loop));
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
    DominatorTree dt(fn);
    auto loops = findLoops(fn, dt);
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
                if (op == IROpcode::Call || op == IROpcode::Store || op == IROpcode::Alloca ||
                    op == IROpcode::Load || op == IROpcode::Phi) {
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
