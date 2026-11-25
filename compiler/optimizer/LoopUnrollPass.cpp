#include "LoopUnrollPass.hpp"
#include "Dominance.hpp"
#include "compiler/ir/IRBasicBlock.hpp"
#include "compiler/ir/IRFunction.hpp"
#include "compiler/ir/IRInstruction.hpp"
#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace sph {

namespace {

bool isLoopInvariantVal(const IRValue* val, const NaturalLoop& loop, const DominatorTree& dt) {
    if (!val) return false;
    if (val->isConstant() || val->isParameter()) return true;
    if (val->isInstruction()) {
        const auto* inst = static_cast<const IRInstruction*>(val);
        IRBasicBlock* defBb = inst->getParent();
        if (!defBb || loop.blocks.count(defBb) > 0) return false;
        return dt.dominates(defBb, loop.preheader);
    }
    return false;
}

std::shared_ptr<IRInstruction> cloneLoopInstruction(
    const IRInstruction& inst,
    const std::unordered_map<const IRValue*, std::shared_ptr<IRValue>>& valMap,
    size_t& counter) {
    auto resolve = [&](const std::shared_ptr<IRValue>& v) -> std::shared_ptr<IRValue> {
        if (!v) return nullptr;
        auto it = valMap.find(v.get());
        return (it != valMap.end()) ? it->second : v;
    };

    std::string suffix = ".u" + std::to_string(counter++);
    if (inst.getOpcode() >= IROpcode::Add && inst.getOpcode() <= IROpcode::GreaterEqual) {
        const auto& bin = static_cast<const BinaryInst&>(inst);
        return std::make_shared<BinaryInst>(
            bin.getOpcode(), resolve(bin.getLhs()), resolve(bin.getRhs()),
            bin.getType(), bin.getName() + suffix);
    }
    if (inst.getOpcode() == IROpcode::Negate ||
        inst.getOpcode() == IROpcode::LogicalNot ||
        inst.getOpcode() == IROpcode::BitwiseNot) {
        const auto& un = static_cast<const UnaryInst&>(inst);
        return std::make_shared<UnaryInst>(
            un.getOpcode(), resolve(un.getOperand()), un.getType(), un.getName() + suffix);
    }
    if (inst.getOpcode() == IROpcode::Cast) {
        const auto& c = static_cast<const CastInst&>(inst);
        return std::make_shared<CastInst>(
            resolve(c.getOperand()), c.getType(), c.getName() + suffix);
    }
    if (inst.getOpcode() == IROpcode::Load) {
        const auto& ld = static_cast<const LoadInst&>(inst);
        return std::make_shared<LoadInst>(
            ld.getType(), resolve(ld.getPointer()), ld.getName() + suffix);
    }
    if (inst.getOpcode() == IROpcode::Store) {
        const auto& st = static_cast<const StoreInst&>(inst);
        return std::make_shared<StoreInst>(
            resolve(st.getValue()), resolve(st.getPointer()));
    }
    if (inst.getOpcode() == IROpcode::GetFieldPtr) {
        const auto& gfp = static_cast<const GetFieldPtrInst&>(inst);
        return std::make_shared<GetFieldPtrInst>(
            resolve(gfp.getBasePtr()), gfp.getFieldIndex(), gfp.getByteOffset(),
            gfp.getType(), gfp.getName() + suffix);
    }
    if (inst.getOpcode() == IROpcode::GetElementPtr) {
        const auto& gep = static_cast<const GetElementPtrInst&>(inst);
        return std::make_shared<GetElementPtrInst>(
            resolve(gep.getBasePtr()), resolve(gep.getIndex()), gep.getElementSize(),
            gep.getType(), gep.getName() + suffix);
    }
    return nullptr;
}

bool isClonableInstruction(const IRInstruction& inst) {
    auto op = inst.getOpcode();
    if (op >= IROpcode::Add && op <= IROpcode::GreaterEqual) {
        return op != IROpcode::Div && op != IROpcode::Mod;
    }
    return op == IROpcode::Negate || op == IROpcode::LogicalNot ||
           op == IROpcode::BitwiseNot || op == IROpcode::Cast ||
           op == IROpcode::Load || op == IROpcode::Store ||
           op == IROpcode::GetFieldPtr || op == IROpcode::GetElementPtr;
}

} // namespace

bool LoopUnrollPass::runOnFunction(IRFunction& fn) {
    if (m_unrollFactor < 2 || fn.getBlocks().size() < 2) return false;

    DominatorTree dt(fn);
    auto loops = findNaturalLoops(fn, dt);
    if (loops.empty()) return false;

    bool changed = false;

    for (const auto& loop : loops) {
        if (!loop.header || !loop.preheader) continue;
        if (loop.header->isLoopUnrolled() || loop.preheader->isLoopUnrolled()) continue;
        if (loop.blocks.size() != 2 || loop.latches.size() != 1) continue;

        IRBasicBlock* header = loop.header;
        IRBasicBlock* body = loop.latches[0];
        if (!body || body == header || loop.blocks.count(body) == 0) continue;

        auto hdrPreds = getPredecessors(*header, fn);
        if (hdrPreds.size() != 2) continue;
        auto bodyPreds = getPredecessors(*body, fn);
        if (bodyPreds.size() != 1 || bodyPreds[0] != header) continue;

        // Analyze header structure: Phis -> single compare -> CondBr(body, exit)
        const auto& hdrInsts = header->getInstructions();
        if (hdrInsts.size() < 2) continue;
        const auto* condBr = dynamic_cast<const CondBranchInst*>(header->getTerminator());
        if (!condBr || condBr->getTrueTarget() != body) continue;
        IRBasicBlock* exitBb = condBr->getFalseTarget();
        if (!exitBb || exitBb == header || exitBb == body) continue;

        std::vector<PhiInst*> hdrPhis;
        const BinaryInst* cmpInst = nullptr;
        bool validHeader = true;
        for (size_t i = 0; i + 1 < hdrInsts.size(); ++i) {
            if (hdrInsts[i]->getOpcode() == IROpcode::Phi) {
                hdrPhis.push_back(static_cast<PhiInst*>(hdrInsts[i].get()));
            } else if (!cmpInst && hdrInsts[i].get() == condBr->getCondition().get() &&
                       hdrInsts[i]->getOpcode() == IROpcode::Less) {
                cmpInst = static_cast<const BinaryInst*>(hdrInsts[i].get());
            } else {
                validHeader = false;
                break;
            }
        }
        if (!validHeader || hdrPhis.empty() || !cmpInst) continue;

        // Verify induction variable in cmpInst: iv < bound with unit step +1
        auto* ivPhi = dynamic_cast<PhiInst*>(cmpInst->getLhs().get());
        auto bound = cmpInst->getRhs();
        if (!ivPhi || ivPhi->getParent() != header || !bound) continue;
        if (!ivPhi->getType() || !ivPhi->getType()->isInteger() || ivPhi->getType()->isBoolean()) continue;
        if (!isLoopInvariantVal(bound.get(), loop, dt)) continue;

        auto ivNextVal = ivPhi->getIncomingValue(body);
        auto* ivStep = dynamic_cast<BinaryInst*>(ivNextVal.get());
        if (!ivStep || ivStep->getParent() != body || ivStep->getOpcode() != IROpcode::Add) continue;
        auto* cLhs = dynamic_cast<const ConstantInt*>(ivStep->getLhs().get());
        auto* cRhs = dynamic_cast<const ConstantInt*>(ivStep->getRhs().get());
        bool isUnitInc = (ivStep->getLhs().get() == ivPhi && cRhs && cRhs->getValue() == 1) ||
                         (ivStep->getRhs().get() == ivPhi && cLhs && cLhs->getValue() == 1);
        if (!isUnitInc) continue;

        // Verify all header Phis have incoming edges from preheader and body
        bool phisOk = true;
        for (PhiInst* p : hdrPhis) {
            if (!p->getIncomingValue(loop.preheader) || !p->getIncomingValue(body)) {
                phisOk = false;
                break;
            }
        }
        if (!phisOk) continue;

        // Verify body is small, single-block, ends in Branch(header), and has only clonable ops
        const auto& bodyInsts = body->getInstructions();
        if (bodyInsts.empty() || bodyInsts.size() > 16) continue;
        const auto* bodyBr = dynamic_cast<const BranchInst*>(body->getTerminator());
        if (!bodyBr || bodyBr->getTarget() != header) continue;

        bool bodyOk = true;
        for (size_t i = 0; i + 1 < bodyInsts.size(); ++i) {
            if (!isClonableInstruction(*bodyInsts[i])) {
                bodyOk = false;
                break;
            }
        }
        if (!bodyOk) continue;

        // Determine safe limit (bound - (UNROLL_FACTOR - 1)) without signed underflow
        const int64_t stepOffset = static_cast<int64_t>(m_unrollFactor - 1);
        const int64_t minSafeBound = ivPhi->getType()->isI32()
            ? (-2147483648LL + stepOffset + 1)
            : (-9223372036854775807LL + stepOffset + 1);

        const auto* constBound = dynamic_cast<const ConstantInt*>(bound.get());
        if (constBound && constBound->getValue() < minSafeBound) continue;
        // Skip very small known constant bounds where 4x loop never triggers
        if (constBound) {
            if (const auto* constInit = dynamic_cast<const ConstantInt*>(
                    ivPhi->getIncomingValue(loop.preheader).get())) {
                if (constBound->getValue() - constInit->getValue() < static_cast<int64_t>(m_unrollFactor)) {
                    continue;
                }
            }
        }

        // Verify cmpInst has no uses other than condBr
        size_t cmpUses = 0;
        for (const auto& bPtr : fn.getBlocks()) {
            for (const auto& instPtr : bPtr->getInstructions()) {
                for (const auto* op : getInstructionOperands(*instPtr)) {
                    if (op == cmpInst) cmpUses++;
                }
            }
        }
        if (cmpUses != 1) continue;

        // Perform 4x partial loop unrolling
        const std::string baseName = header->getName();
        IRBasicBlock* unrollSetup = nullptr;
        std::shared_ptr<IRValue> limitVal;
        if (constBound) {
            limitVal = ConstantInt::get(constBound->getValue() - stepOffset, ivPhi->getType());
        } else {
            unrollSetup = fn.createBlock(baseName + ".u_setup");
        }
        IRBasicBlock* unrollHdr = fn.createBlock(baseName + ".u_hdr");
        IRBasicBlock* unrollBody = fn.createBlock(body->getName() + ".u_body");

        header->setLoopUnrolled(true);
        body->setLoopUnrolled(true);
        unrollHdr->setLoopUnrolled(true);
        unrollBody->setLoopUnrolled(true);

        // Relocate newly created blocks right before header so linear order stays natural
        auto& fnBlocks = fn.getBlocks();
        size_t newCount = unrollSetup ? 3 : 2;
        auto hdrIt = std::find_if(fnBlocks.begin(), fnBlocks.end(),
            [header](const auto& bp) { return bp.get() == header; });
        if (hdrIt != fnBlocks.end() && fnBlocks.size() >= newCount) {
            std::rotate(hdrIt, fnBlocks.end() - static_cast<std::ptrdiff_t>(newCount), fnBlocks.end());
        }

        IRBasicBlock* unrollIncomingPre = loop.preheader;
        if (constBound) {
            // Update preheader terminator: br %unrollHdr
            auto& preInsts = loop.preheader->getInstructions();
            if (!preInsts.empty() && preInsts.back()->isTerminator()) {
                preInsts.pop_back();
            }
            auto brUnroll = std::make_shared<BranchInst>(unrollHdr);
            brUnroll->setParent(loop.preheader);
            loop.preheader->addInstruction(brUnroll);
        } else {
            // Preheader: check bound >= minSafeBound before entering unrolled path
            auto& preInsts = loop.preheader->getInstructions();
            if (!preInsts.empty() && preInsts.back()->isTerminator()) {
                preInsts.pop_back();
            }
            auto safeCmp = std::make_shared<BinaryInst>(
                IROpcode::GreaterEqual, bound,
                ConstantInt::get(minSafeBound, ivPhi->getType()),
                Type::getBool(), ".u_safe." + std::to_string(m_counter++));
            safeCmp->setParent(loop.preheader);
            loop.preheader->addInstruction(safeCmp);

            auto preCond = std::make_shared<CondBranchInst>(safeCmp, unrollSetup, header);
            preCond->setParent(loop.preheader);
            loop.preheader->addInstruction(preCond);

            auto limInst = std::make_shared<BinaryInst>(
                IROpcode::Sub, bound, ConstantInt::get(stepOffset, ivPhi->getType()),
                ivPhi->getType(), ".u_lim." + std::to_string(m_counter++));
            limInst->setParent(unrollSetup);
            unrollSetup->addInstruction(limInst);
            limitVal = limInst;

            auto setupBr = std::make_shared<BranchInst>(unrollHdr);
            setupBr->setParent(unrollSetup);
            unrollSetup->addInstruction(setupBr);
            unrollIncomingPre = unrollSetup;
        }

        // Build Phis in unrollHdr and track initial map
        std::vector<std::shared_ptr<PhiInst>> unrollPhis;
        std::unordered_map<const IRValue*, std::shared_ptr<IRValue>> valMap;
        std::shared_ptr<PhiInst> unrollIvPhi;

        for (PhiInst* hdrPhi : hdrPhis) {
            auto uPhi = std::make_shared<PhiInst>(
                hdrPhi->getType(), hdrPhi->getName() + ".u_phi." + std::to_string(m_counter++));
            uPhi->setParent(unrollHdr);
            uPhi->addIncoming(unrollIncomingPre, hdrPhi->getIncomingValue(loop.preheader));
            unrollHdr->addInstruction(uPhi);
            unrollPhis.push_back(uPhi);
            valMap[hdrPhi] = uPhi;
            if (hdrPhi == ivPhi) {
                unrollIvPhi = uPhi;
            }
        }

        auto uCmp = std::make_shared<BinaryInst>(
            IROpcode::Less, unrollIvPhi, limitVal,
            Type::getBool(), ".u_cmp." + std::to_string(m_counter++));
        uCmp->setParent(unrollHdr);
        unrollHdr->addInstruction(uCmp);

        auto uBr = std::make_shared<CondBranchInst>(uCmp, unrollBody, header);
        uBr->setParent(unrollHdr);
        unrollHdr->addInstruction(uBr);

        // Emit UNROLL_FACTOR copies of body into unrollBody
        for (size_t iter = 0; iter < m_unrollFactor; ++iter) {
            for (size_t i = 0; i + 1 < bodyInsts.size(); ++i) {
                auto cloned = cloneLoopInstruction(*bodyInsts[i], valMap, m_counter);
                if (!cloned) break;
                cloned->setParent(unrollBody);
                unrollBody->addInstruction(cloned);
                valMap[bodyInsts[i].get()] = cloned;
            }

            // Advance header Phis simultaneously (parallel assignment semantics)
            std::vector<std::shared_ptr<IRValue>> nextPhiVals(hdrPhis.size());
            for (size_t r = 0; r < hdrPhis.size(); ++r) {
                auto nxt = hdrPhis[r]->getIncomingValue(body);
                auto it = valMap.find(nxt.get());
                nextPhiVals[r] = (it != valMap.end()) ? it->second : nxt;
            }
            // Clear body-local defs before next iteration, then update header Phi mappings
            for (size_t i = 0; i + 1 < bodyInsts.size(); ++i) {
                valMap.erase(bodyInsts[i].get());
            }
            for (size_t r = 0; r < hdrPhis.size(); ++r) {
                valMap[hdrPhis[r]] = nextPhiVals[r];
            }
        }

        auto backBr = std::make_shared<BranchInst>(unrollHdr);
        backBr->setParent(unrollBody);
        unrollBody->addInstruction(backBr);

        // Hook backedge on unrollPhis and wire remainder header incoming edge from unrollHdr
        for (size_t r = 0; r < hdrPhis.size(); ++r) {
            unrollPhis[r]->addIncoming(unrollBody, valMap[hdrPhis[r]]);
            if (constBound) {
                // Preheader edge moved exclusively to unrollHdr
                for (size_t incIdx = 0; incIdx < hdrPhis[r]->getIncoming().size(); ++incIdx) {
                    if (hdrPhis[r]->getIncoming()[incIdx].first == loop.preheader) {
                        hdrPhis[r]->setIncomingBlock(incIdx, unrollHdr);
                        hdrPhis[r]->setIncomingValue(incIdx, unrollPhis[r]);
                        break;
                    }
                }
            } else {
                // Header now has 3 predecessors: preheader, unrollHdr, body
                hdrPhis[r]->addIncoming(unrollHdr, unrollPhis[r]);
            }
        }

        changed = true;
        // Rebuild dominance only once per pass call: skip overlapping outer loops in same call;
        // PassManager's fixed-point loop or subsequent passes will see updated CFG cleanly.
        break;
    }

    return changed;
}

} // namespace sph

