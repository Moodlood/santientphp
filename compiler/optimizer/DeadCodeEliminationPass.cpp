#include "DeadCodeEliminationPass.hpp"
#include <algorithm>
#include <vector>

namespace sph {

bool DeadCodeEliminationPass::isSideEffectFree(const IRInstruction& inst) noexcept {
    switch (inst.getOpcode()) {
        case IROpcode::Alloca:
        case IROpcode::Load:
        case IROpcode::GetFieldPtr:
        case IROpcode::GetElementPtr:
        case IROpcode::AddressOf:
        case IROpcode::Add:
        case IROpcode::Sub:
        case IROpcode::Mul:
        case IROpcode::Div:
        case IROpcode::Mod:
        case IROpcode::Equal:
        case IROpcode::NotEqual:
        case IROpcode::Less:
        case IROpcode::LessEqual:
        case IROpcode::Greater:
        case IROpcode::GreaterEqual:
        case IROpcode::LogicalAnd:
        case IROpcode::LogicalOr:
        case IROpcode::BitwiseAnd:
        case IROpcode::BitwiseOr:
        case IROpcode::BitwiseXor:
        case IROpcode::ShiftLeft:
        case IROpcode::ShiftRight:
        case IROpcode::Negate:
        case IROpcode::LogicalNot:
        case IROpcode::BitwiseNot:
        case IROpcode::Cast:
            return true;
        default:
            return false;
    }
}

bool DeadCodeEliminationPass::eliminatePostTerminatorCode(IRFunction& fn) {
    bool changed = false;
    for (auto& bb : fn.getBlocks()) {
        auto& insts = bb->getInstructions();
        size_t termIdx = insts.size();
        for (size_t i = 0; i < insts.size(); ++i) {
            if (insts[i]->isTerminator()) {
                termIdx = i;
                break;
            }
        }
        if (termIdx + 1 < insts.size()) {
            insts.erase(insts.begin() + termIdx + 1, insts.end());
            changed = true;
        }
    }
    return changed;
}

bool DeadCodeEliminationPass::eliminateUnreachableBlocks(IRFunction& fn) {
    if (fn.getBlocks().empty()) return false;

    std::unordered_set<IRBasicBlock*> reachable;
    std::vector<IRBasicBlock*> worklist;

    IRBasicBlock* entry = fn.getEntryBlock();
    if (!entry) entry = fn.getBlocks().front().get();

    reachable.insert(entry);
    worklist.push_back(entry);

    while (!worklist.empty()) {
        IRBasicBlock* curr = worklist.back();
        worklist.pop_back();

        if (curr->hasTerminator()) {
            const auto* term = curr->getTerminator();
            if (term->getOpcode() == IROpcode::Br) {
                const auto* br = static_cast<const BranchInst*>(term);
                if (br->getTarget() && reachable.insert(br->getTarget()).second) {
                    worklist.push_back(br->getTarget());
                }
            } else if (term->getOpcode() == IROpcode::CondBr) {
                const auto* condBr = static_cast<const CondBranchInst*>(term);
                if (condBr->getTrueTarget() && reachable.insert(condBr->getTrueTarget()).second) {
                    worklist.push_back(condBr->getTrueTarget());
                }
                if (condBr->getFalseTarget() && reachable.insert(condBr->getFalseTarget()).second) {
                    worklist.push_back(condBr->getFalseTarget());
                }
            }
        }
    }

    if (reachable.size() == fn.getBlocks().size()) {
        return false;
    }

    auto& blocks = fn.getBlocks();
    size_t oldSize = blocks.size();
    blocks.erase(
        std::remove_if(blocks.begin(), blocks.end(),
            [&](const std::unique_ptr<IRBasicBlock>& b) {
                return reachable.find(b.get()) == reachable.end();
            }),
        blocks.end()
    );

    return blocks.size() < oldSize;
}

bool DeadCodeEliminationPass::eliminateDeadInstructions(IRFunction& fn) {
    bool anyChanged = false;
    bool changedInPass = true;

    while (changedInPass) {
        changedInPass = false;

        // Build use count map
        std::unordered_map<const IRValue*, size_t> useCounts;
        for (const auto& bb : fn.getBlocks()) {
            for (const auto& inst : bb->getInstructions()) {
                auto operands = getInstructionOperands(*inst);
                for (const auto* op : operands) {
                    if (op) useCounts[op]++;
                }
            }
        }

        // Identify and remove dead side-effect-free instructions
        for (auto& bb : fn.getBlocks()) {
            auto& insts = bb->getInstructions();
            size_t oldSize = insts.size();
            insts.erase(
                std::remove_if(insts.begin(), insts.end(),
                    [&](const std::shared_ptr<IRInstruction>& inst) {
                        if (isSideEffectFree(*inst)) {
                            auto it = useCounts.find(inst.get());
                            if (it == useCounts.end() || it->second == 0) {
                                return true;
                            }
                        }
                        return false;
                    }),
                insts.end()
            );

            if (insts.size() < oldSize) {
                changedInPass = true;
                anyChanged = true;
            }
        }
    }

    return anyChanged;
}

bool DeadCodeEliminationPass::runOnFunction(IRFunction& fn) {
    bool changed = false;
    if (eliminatePostTerminatorCode(fn)) changed = true;
    if (eliminateUnreachableBlocks(fn)) changed = true;
    if (eliminateDeadInstructions(fn)) changed = true;
    return changed;
}

} // namespace sph

