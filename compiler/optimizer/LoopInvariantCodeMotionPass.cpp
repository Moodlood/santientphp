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
    auto loops = findNaturalLoops(fn, dt);
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
            size_t insertPos = loop.preheader->hasTerminator() ? (loop.preheader->getInstructions().size() - 1) : loop.preheader->getInstructions().size();
            for (const auto& inv : invariantInsts) {
                loop.preheader->insertInstruction(insertPos++, inv);
            }
            anyChanged = true;
        }
    }

    return anyChanged;
}

} // namespace sph
