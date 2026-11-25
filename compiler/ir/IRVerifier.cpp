#include "IRVerifier.hpp"
#include "compiler/ir/IRInstruction.hpp"
#include "compiler/optimizer/Dominance.hpp"
#include <unordered_map>
#include <unordered_set>

namespace sph {

bool IRVerifier::verify(const IRModule& module, std::vector<std::string>& errors) const {
    bool ok = true;
    for (const auto& fn : module.getFunctions()) {
        if (!verify(*fn, errors)) {
            ok = false;
        }
    }
    return ok;
}

bool IRVerifier::verify(const IRFunction& fn, std::vector<std::string>& errors) const {
    if (fn.isDeclaration()) {
        return true;
    }

    bool ok = true;

    if (fn.getBlocks().empty()) {
        errors.push_back("function '@" + fn.getName() + "' has no basic blocks");
        return false;
    }

    // Step 1: Verify all basic blocks structurally
    for (const auto& bb : fn.getBlocks()) {
        if (!bb) {
            errors.push_back("function '@" + fn.getName() + "' contains a null basic block");
            ok = false;
            continue;
        }
        if (bb->getParent() != &fn) {
            errors.push_back("basic block '" + bb->getName() + "' parent pointer does not match function '@" + fn.getName() + "'");
            ok = false;
        }
        if (!verify(*bb, fn, errors)) {
            ok = false;
        }
    }

    // Step 2: Verify SSA invariants, dominance, and phi nodes
    DominatorTree domTree(fn);
    if (!verifySSA(fn, domTree, errors)) {
        ok = false;
    }

    return ok;
}

bool IRVerifier::verify(const IRBasicBlock& bb, const IRFunction& fn, std::vector<std::string>& errors) const {
    bool ok = true;
    const auto& instructions = bb.getInstructions();

    if (instructions.empty()) {
        errors.push_back("basic block '" + bb.getName() + "' in function '@" + fn.getName() + "' is empty");
        return false;
    }

    if (!bb.hasTerminator()) {
        errors.push_back("basic block '" + bb.getName() + "' in function '@" + fn.getName() +
                         "' does not end with a terminator");
        ok = false;
    }

    bool seenNonPhi = false;
    for (size_t i = 0; i < instructions.size(); ++i) {
        const auto& inst = instructions[i];

        if (!inst) {
            errors.push_back("null instruction in basic block '" + bb.getName() + "' of function '@" + fn.getName() + "'");
            ok = false;
            continue;
        }

        if (!inst->getParent()) {
            errors.push_back("instruction in block '" + bb.getName() + "' of function '@" + fn.getName() + "' has null parent basic block pointer");
            ok = false;
        } else if (inst->getParent() != &bb) {
            errors.push_back("instruction in block '" + bb.getName() + "' has mismatched parent pointer pointing to block '" + inst->getParent()->getName() + "'");
            ok = false;
        }

        // Ensure terminator only at the end
        if (inst->isTerminator() && i != instructions.size() - 1) {
            errors.push_back("terminator instruction appears before the end of block '" + bb.getName() + "'");
            ok = false;
        }

        if (inst->getOpcode() == IROpcode::Phi) {
            if (seenNonPhi) {
                errors.push_back("phi instruction appears after non-phi instruction in block '" + bb.getName() + "'");
                ok = false;
            }
        } else {
            seenNonPhi = true;
        }

        // Validate specific instructions
        switch (inst->getOpcode()) {
            case IROpcode::Ret: {
                const auto& retInst = static_cast<const RetInst&>(*inst);
                if (fn.getReturnType()->isVoid()) {
                    if (retInst.hasValue()) {
                        errors.push_back("void function '@" + fn.getName() + "' returns a value");
                        ok = false;
                    }
                } else {
                    if (!retInst.hasValue()) {
                        errors.push_back("non-void function '@" + fn.getName() + "' returns void");
                        ok = false;
                    } else if (retInst.getValue() && !retInst.getValue()->getType()->isEqual(*fn.getReturnType())) {
                        errors.push_back("return type mismatch in function '@" + fn.getName() +
                                         "': expected '" + fn.getReturnType()->toString() +
                                         "', got '" + retInst.getValue()->getType()->toString() + "'");
                        ok = false;
                    }
                }
                break;
            }
            case IROpcode::Br: {
                const auto& brInst = static_cast<const BranchInst&>(*inst);
                if (!brInst.getTarget()) {
                    errors.push_back("branch instruction in block '" + bb.getName() + "' has null target");
                    ok = false;
                } else if (brInst.getTarget()->getParent() != &fn) {
                    errors.push_back("branch target does not belong to enclosing function '@" + fn.getName() + "'");
                    ok = false;
                }
                break;
            }
            case IROpcode::CondBr: {
                const auto& condBr = static_cast<const CondBranchInst&>(*inst);
                if (!condBr.getCondition()) {
                    errors.push_back("conditional branch in block '" + bb.getName() + "' has null condition");
                    ok = false;
                } else if (!condBr.getCondition()->getType()->isBoolean()) {
                    errors.push_back("conditional branch condition must be bool, found '" +
                                     condBr.getCondition()->getType()->toString() + "'");
                    ok = false;
                }

                if (!condBr.getTrueTarget() || condBr.getTrueTarget()->getParent() != &fn) {
                    errors.push_back("conditional branch true target invalid in function '@" + fn.getName() + "'");
                    ok = false;
                }
                if (!condBr.getFalseTarget() || condBr.getFalseTarget()->getParent() != &fn) {
                    errors.push_back("conditional branch false target invalid in function '@" + fn.getName() + "'");
                    ok = false;
                }
                break;
            }
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
            case IROpcode::ShiftRight: {
                const auto& binInst = static_cast<const BinaryInst&>(*inst);
                if (!binInst.getLhs() || !binInst.getRhs()) {
                    errors.push_back("binary instruction has null operand");
                    ok = false;
                }
                break;
            }
            case IROpcode::Negate:
            case IROpcode::LogicalNot:
            case IROpcode::BitwiseNot: {
                const auto& unInst = static_cast<const UnaryInst&>(*inst);
                if (!unInst.getOperand()) {
                    errors.push_back("unary instruction has null operand");
                    ok = false;
                }
                break;
            }
            case IROpcode::Cast: {
                const auto& castInst = static_cast<const CastInst&>(*inst);
                if (!castInst.getOperand()) {
                    errors.push_back("cast instruction has null operand");
                    ok = false;
                }
                break;
            }
            case IROpcode::Load: {
                const auto& loadInst = static_cast<const LoadInst&>(*inst);
                if (!loadInst.getPointer()) {
                    errors.push_back("load instruction has null pointer");
                    ok = false;
                }
                break;
            }
            case IROpcode::Store: {
                const auto& storeInst = static_cast<const StoreInst&>(*inst);
                if (!storeInst.getPointer()) {
                    errors.push_back("store instruction has null pointer");
                    ok = false;
                }
                if (!storeInst.getValue()) {
                    errors.push_back("store instruction has null value");
                    ok = false;
                }
                break;
            }
            case IROpcode::GetFieldPtr: {
                const auto& gfp = static_cast<const GetFieldPtrInst&>(*inst);
                if (!gfp.getBasePtr()) {
                    errors.push_back("getfieldptr instruction has null base pointer");
                    ok = false;
                }
                break;
            }
            case IROpcode::GetElementPtr: {
                const auto& gep = static_cast<const GetElementPtrInst&>(*inst);
                if (!gep.getBasePtr()) {
                    errors.push_back("getelementptr instruction has null base pointer");
                    ok = false;
                }
                if (!gep.getIndex()) {
                    errors.push_back("getelementptr instruction has null index");
                    ok = false;
                }
                break;
            }
            case IROpcode::Alloca: {
                const auto& allocaInst = static_cast<const AllocaInst&>(*inst);
                if (!allocaInst.getAllocatedType() || allocaInst.getAllocatedType()->isVoid()) {
                    errors.push_back("alloca instruction in block '" + bb.getName() +
                                     "' must have a non-void allocated type");
                    ok = false;
                }
                break;
            }
            case IROpcode::AddressOf: {
                const auto& addrInst = static_cast<const AddressOfInst&>(*inst);
                if (!addrInst.getTarget()) {
                    errors.push_back("addressof instruction has null target");
                    ok = false;
                } else if (!addrInst.getType()->isPointer()) {
                    errors.push_back("addressof instruction result type must be pointer");
                    ok = false;
                }
                break;
            }
            case IROpcode::RcAlloc: {
                const auto& rcAlloc = static_cast<const RcAllocInst&>(*inst);
                if (!rcAlloc.getType()->isRc()) {
                    errors.push_back("rc_alloc instruction result type must be Rc<T>");
                    ok = false;
                }
                break;
            }
            case IROpcode::RcRetain: {
                const auto& rcRetain = static_cast<const RcRetainInst&>(*inst);
                if (!rcRetain.getPtr()) {
                    errors.push_back("rc_retain instruction has null pointer");
                    ok = false;
                }
                break;
            }
            case IROpcode::RcRelease: {
                const auto& rcRelease = static_cast<const RcReleaseInst&>(*inst);
                if (!rcRelease.getPtr()) {
                    errors.push_back("rc_release instruction has null pointer");
                    ok = false;
                }
                break;
            }
            case IROpcode::ArcAlloc: {
                const auto& arcAlloc = static_cast<const ArcAllocInst&>(*inst);
                if (!arcAlloc.getType()->isArc()) {
                    errors.push_back("arc_alloc instruction result type must be Arc<T>");
                    ok = false;
                }
                break;
            }
            case IROpcode::ArcRetain: {
                const auto& arcRetain = static_cast<const ArcRetainInst&>(*inst);
                if (!arcRetain.getPtr()) {
                    errors.push_back("arc_retain instruction has null pointer");
                    ok = false;
                }
                break;
            }
            case IROpcode::ArcRelease: {
                const auto& arcRelease = static_cast<const ArcReleaseInst&>(*inst);
                if (!arcRelease.getPtr()) {
                    errors.push_back("arc_release instruction has null pointer");
                    ok = false;
                }
                break;
            }
            default:
                break;
        }
    }

    return ok;
}

bool IRVerifier::verifySSA(const IRFunction& fn, const DominatorTree& domTree, std::vector<std::string>& errors) const {
    bool ok = true;

    // Collect all valid parameters for fast lookup
    std::unordered_set<const IRValue*> validParams;
    for (const auto& param : fn.getParameters()) {
        validParams.insert(param.get());
    }

    for (const auto& bb : fn.getBlocks()) {
        auto preds = getPredecessors(*bb, fn);
        std::unordered_set<const IRBasicBlock*> predSet(preds.begin(), preds.end());

        // Map instruction order within the block for local dominance checking
        std::unordered_map<const IRInstruction*, size_t> instOrder;
        for (size_t i = 0; i < bb->getInstructions().size(); ++i) {
            instOrder[bb->getInstructions()[i].get()] = i;
        }

        for (const auto& inst : bb->getInstructions()) {
            if (inst->getOpcode() == IROpcode::Phi) {
                const auto& phi = static_cast<const PhiInst&>(*inst);
                const auto& incoming = phi.getIncoming();

                // Check 1: Every predecessor must have an entry in incoming
                std::unordered_set<const IRBasicBlock*> seenPreds;
                for (const auto& [inBb, inVal] : incoming) {
                    if (!inBb) {
                        errors.push_back("phi instruction has null incoming basic block in block '" + bb->getName() + "'");
                        ok = false;
                        continue;
                    }
                    if (seenPreds.count(inBb)) {
                        errors.push_back("duplicate incoming predecessor block '" + inBb->getName() +
                                         "' in phi instruction of block '" + bb->getName() + "'");
                        ok = false;
                    }
                    seenPreds.insert(inBb);

                    if (!predSet.count(inBb)) {
                        errors.push_back("incoming block '" + inBb->getName() +
                                         "' is not a predecessor of block '" + bb->getName() + "'");
                        ok = false;
                    }

                    if (!inVal) {
                        errors.push_back("phi instruction has null incoming value for predecessor '" +
                                         inBb->getName() + "' in block '" + bb->getName() + "'");
                        ok = false;
                    } else {
                        // Check type match
                        if (!inVal->getType()->isEqual(*phi.getType())) {
                            errors.push_back("phi incoming type mismatch for block '" + inBb->getName() +
                                             "': expected '" + phi.getType()->toString() +
                                             "', got '" + inVal->getType()->toString() + "'");
                            ok = false;
                        }

                        // Check cross-function and dominance along the predecessor edge
                        if (inVal->isInstruction()) {
                            const auto* defInst = static_cast<const IRInstruction*>(inVal.get());
                            if (!defInst->getParent() || defInst->getParent()->getParent() != &fn) {
                                errors.push_back("cross-function value reference in phi instruction of function '@" +
                                                 fn.getName() + "'");
                                ok = false;
                            } else if (domTree.isReachable(inBb)) {
                                if (defInst->getParent() != inBb && !domTree.dominates(defInst->getParent(), inBb)) {
                                    errors.push_back("phi incoming value defined in block '" + defInst->getParent()->getName() +
                                                     "' does not dominate predecessor block '" + inBb->getName() + "'");
                                    ok = false;
                                }
                            }
                        }
                    }
                }

                // Check for missing predecessors
                for (const auto* pred : preds) {
                    if (!seenPreds.count(pred)) {
                        errors.push_back("phi instruction in block '" + bb->getName() +
                                         "' is missing incoming value for predecessor '" + pred->getName() + "'");
                        ok = false;
                    }
                }
            } else {
                // Non-Phi instruction: check operands and dominance
                auto operands = getInstructionOperands(*inst);
                for (const auto* op : operands) {
                    if (!op) {
                        errors.push_back("instruction has null operand in block '" + bb->getName() + "'");
                        ok = false;
                        continue;
                    }

                    if (op->isInstruction()) {
                        const auto* defInst = static_cast<const IRInstruction*>(op);
                        if (!defInst->getParent() || defInst->getParent()->getParent() != &fn) {
                            errors.push_back("cross-function operand reference in function '@" + fn.getName() + "'");
                            ok = false;
                        } else if (domTree.isReachable(bb.get())) {
                            if (defInst->getParent() == bb.get()) {
                                // Must be defined earlier in the same block
                                auto itDef = instOrder.find(defInst);
                                auto itUse = instOrder.find(inst.get());
                                if (itDef == instOrder.end() || itUse == instOrder.end() || itDef->second >= itUse->second) {
                                    errors.push_back("instruction use precedes or self-references definition in block '" +
                                                     bb->getName() + "' of function '@" + fn.getName() + "'");
                                    ok = false;
                                }
                            } else {
                                // Must strictly dominate the use block
                                if (!domTree.strictlyDominates(defInst->getParent(), bb.get())) {
                                    errors.push_back("definition in block '" + defInst->getParent()->getName() +
                                                     "' does not dominate use in block '" + bb->getName() +
                                                     "' in function '@" + fn.getName() + "'");
                                    ok = false;
                                }
                            }
                        }
                    } else if (op->isParameter()) {
                        if (!validParams.count(op)) {
                            errors.push_back("parameter operand does not belong to function '@" + fn.getName() + "'");
                            ok = false;
                        }
                    }
                }
            }
        }
    }

    return ok;
}

} // namespace sph
