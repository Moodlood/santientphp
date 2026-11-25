#include "StrengthReductionPass.hpp"
#include "Dominance.hpp"
#include "compiler/ir/IRBasicBlock.hpp"
#include "compiler/ir/IRFunction.hpp"
#include "compiler/ir/IRInstruction.hpp"
#include <algorithm>
#include <bit>
#include <map>
#include <utility>
#include <vector>

namespace sph {

namespace {

bool isPowerOfTwo(int64_t val) {
    return val > 0 && (val & (val - 1)) == 0;
}

int countTrailingZeros(int64_t val) {
    int count = 0;
    while ((val & 1) == 0 && count < 64) {
        val >>= 1;
        ++count;
    }
    return count;
}

} // namespace

std::shared_ptr<IRValue> StrengthReductionPass::reduceBinary(BinaryInst& bin, bool& inPlaceChanged) {
    auto lhs = bin.getLhs();
    auto rhs = bin.getRhs();
    if (!lhs || !rhs) return nullptr;

    auto cLhsInt = dynamic_cast<const ConstantInt*>(lhs.get());
    auto cRhsInt = dynamic_cast<const ConstantInt*>(rhs.get());

    if (!bin.getType()->isInteger()) return nullptr;

    switch (bin.getOpcode()) {
        case IROpcode::Mul: {
            if (cRhsInt && isPowerOfTwo(cRhsInt->getValue())) {
                int shift = countTrailingZeros(cRhsInt->getValue());
                bin.setOpcode(IROpcode::ShiftLeft);
                bin.setRhs(ConstantInt::get(shift, bin.getType()));
                inPlaceChanged = true;
                return nullptr;
            }
            if (cLhsInt && isPowerOfTwo(cLhsInt->getValue())) {
                int shift = countTrailingZeros(cLhsInt->getValue());
                bin.setOpcode(IROpcode::ShiftLeft);
                bin.setLhs(rhs);
                bin.setRhs(ConstantInt::get(shift, bin.getType()));
                inPlaceChanged = true;
                return nullptr;
            }
            break;
        }

        case IROpcode::Div: {
            // For power-of-two division, if dividend >= 0
            if (cRhsInt && isPowerOfTwo(cRhsInt->getValue()) && cRhsInt->getValue() > 1) {
                if (cLhsInt && cLhsInt->getValue() >= 0) {
                    int shift = countTrailingZeros(cRhsInt->getValue());
                    bin.setOpcode(IROpcode::ShiftRight);
                    bin.setRhs(ConstantInt::get(shift, bin.getType()));
                    inPlaceChanged = true;
                    return nullptr;
                }
            }
            break;
        }

        case IROpcode::Mod: {
            // x % 2^k -> x & (2^k - 1) when x >= 0
            if (cRhsInt && isPowerOfTwo(cRhsInt->getValue())) {
                if (cLhsInt && cLhsInt->getValue() >= 0) {
                    int64_t mask = cRhsInt->getValue() - 1;
                    bin.setOpcode(IROpcode::BitwiseAnd);
                    bin.setRhs(ConstantInt::get(mask, bin.getType()));
                    inPlaceChanged = true;
                    return nullptr;
                }
            }
            break;
        }

        default:
            break;
    }

    return nullptr;
}

namespace {

void replaceAllUses(IRFunction& fn, const IRValue* targetValue, const std::shared_ptr<IRValue>& replacement) {
    for (auto& otherBb : fn.getBlocks()) {
        for (auto& otherInst : otherBb->getInstructions()) {
            if (otherInst.get() != targetValue) {
                replaceOperandInInstruction(*otherInst, targetValue, replacement);
            }
        }
    }
}

bool isLoopInvariantValue(const IRValue* val, const NaturalLoop& loop, const DominatorTree& dt) {
    if (!val) return false;
    if (dynamic_cast<const ConstantInt*>(val) || val->isParameter()) {
        return true;
    }
    if (!val->isConstant() && val->isInstruction()) {
        const auto* inst = static_cast<const IRInstruction*>(val);
        IRBasicBlock* defBb = inst->getParent();
        if (!defBb) return false;
        if (loop.blocks.count(defBb) > 0) return false;
        return dt.dominates(defBb, loop.preheader);
    }
    return false;
}

} // namespace

bool StrengthReductionPass::reduceLoopInductionVars(IRFunction& fn) {
    if (fn.getBlocks().size() < 2) return false;

    DominatorTree dt(fn);
    auto loops = findNaturalLoops(fn, dt);
    if (loops.empty()) return false;

    // Process innermost loops first (smallest block count)
    std::sort(loops.begin(), loops.end(), [](const NaturalLoop& a, const NaturalLoop& b) {
        return a.blocks.size() < b.blocks.size();
    });

    bool changed = false;

    for (const auto& loop : loops) {
        if (!loop.header || !loop.preheader || loop.latches.size() != 1) continue;
        IRBasicBlock* latch = loop.latches[0];
        if (loop.preheader == loop.header) continue;

        auto preds = getPredecessors(*loop.header, fn);
        if (preds.size() != 2) continue;
        bool hasPre = false, hasLatch = false;
        for (auto* p : preds) {
            if (p == loop.preheader) hasPre = true;
            if (p == latch) hasLatch = true;
        }
        if (!hasPre || !hasLatch) continue;

        struct BasicIV {
            PhiInst* phi;
            std::shared_ptr<IRValue> initVal;
            std::shared_ptr<IRValue> stepVal;
            bool isSub;
        };
        std::vector<BasicIV> ivs;

        for (const auto& inst : loop.header->getInstructions()) {
            if (inst->getOpcode() != IROpcode::Phi) break;
            auto* phi = static_cast<PhiInst*>(inst.get());
            if (!phi->getType() || !phi->getType()->isInteger() || phi->getType()->isBoolean()) continue;

            auto initVal = phi->getIncomingValue(loop.preheader);
            auto nextVal = phi->getIncomingValue(latch);
            if (!initVal || !nextVal || !isLoopInvariantValue(initVal.get(), loop, dt)) continue;

            auto* stepBin = dynamic_cast<BinaryInst*>(nextVal.get());
            if (!stepBin || !stepBin->getParent() || loop.blocks.count(stepBin->getParent()) == 0) continue;

            if (stepBin->getOpcode() == IROpcode::Add) {
                if (stepBin->getLhs().get() == phi && isLoopInvariantValue(stepBin->getRhs().get(), loop, dt)) {
                    ivs.push_back({phi, initVal, stepBin->getRhs(), false});
                } else if (stepBin->getRhs().get() == phi && isLoopInvariantValue(stepBin->getLhs().get(), loop, dt)) {
                    ivs.push_back({phi, initVal, stepBin->getLhs(), false});
                }
            } else if (stepBin->getOpcode() == IROpcode::Sub) {
                if (stepBin->getLhs().get() == phi && isLoopInvariantValue(stepBin->getRhs().get(), loop, dt)) {
                    ivs.push_back({phi, initVal, stepBin->getRhs(), true});
                }
            }
        }

        if (ivs.empty()) continue;

        std::map<std::pair<const PhiInst*, const IRValue*>, std::shared_ptr<PhiInst>> derivedCache;

        for (auto* bb : loop.blocks) {
            if (!bb) continue;
            // Snapshot instructions so insertions don't confuse iteration
            auto instsCopy = bb->getInstructions();
            for (const auto& inst : instsCopy) {
                if (inst->getOpcode() != IROpcode::Mul) continue;
                auto* bin = static_cast<BinaryInst*>(inst.get());
                if (!bin->getType() || !bin->getType()->isInteger() || bin->getType()->isBoolean()) continue;

                const BasicIV* matchedIV = nullptr;
                std::shared_ptr<IRValue> scale;
                for (const auto& iv : ivs) {
                    if (bin->getLhs().get() == iv.phi &&
                        isLoopInvariantValue(bin->getRhs().get(), loop, dt) &&
                        bin->getRhs()->getType() &&
                        bin->getRhs()->getType()->getKind() == iv.phi->getType()->getKind() &&
                        bin->getType()->getKind() == iv.phi->getType()->getKind()) {
                        matchedIV = &iv;
                        scale = bin->getRhs();
                        break;
                    }
                    if (bin->getRhs().get() == iv.phi &&
                        isLoopInvariantValue(bin->getLhs().get(), loop, dt) &&
                        bin->getLhs()->getType() &&
                        bin->getLhs()->getType()->getKind() == iv.phi->getType()->getKind() &&
                        bin->getType()->getKind() == iv.phi->getType()->getKind()) {
                        matchedIV = &iv;
                        scale = bin->getLhs();
                        break;
                    }
                }
                if (!matchedIV || !scale) continue;

                auto key = std::make_pair(matchedIV->phi, scale.get());
                std::shared_ptr<PhiInst> derivedPhi;
                auto cacheIt = derivedCache.find(key);
                if (cacheIt != derivedCache.end()) {
                    derivedPhi = cacheIt->second;
                } else {
                    auto foldOrEmitMul = [&](const std::shared_ptr<IRValue>& a,
                                             const std::shared_ptr<IRValue>& b,
                                             const std::string& tag) -> std::shared_ptr<IRValue> {
                        auto* ca = dynamic_cast<const ConstantInt*>(a.get());
                        auto* cb = dynamic_cast<const ConstantInt*>(b.get());
                        if (ca && cb) {
                            return ConstantInt::get(ca->getValue() * cb->getValue(), bin->getType());
                        }
                        if (ca && ca->getValue() == 0) return ConstantInt::get(0, bin->getType());
                        if (cb && cb->getValue() == 0) return ConstantInt::get(0, bin->getType());
                        if (ca && ca->getValue() == 1) return b;
                        if (cb && cb->getValue() == 1) return a;

                        size_t prePos = loop.preheader->hasTerminator()
                            ? (loop.preheader->getInstructions().size() - 1)
                            : loop.preheader->getInstructions().size();
                        auto mulInst = std::make_shared<BinaryInst>(
                            IROpcode::Mul, a, b, bin->getType(),
                            ".lsr." + tag + "." + std::to_string(m_lsrCounter++));
                        mulInst->setParent(loop.preheader);
                        loop.preheader->insertInstruction(prePos, mulInst);
                        return mulInst;
                    };

                    auto derivedInit = foldOrEmitMul(matchedIV->initVal, scale, "init");
                    auto derivedStep = foldOrEmitMul(matchedIV->stepVal, scale, "step");

                    derivedPhi = std::make_shared<PhiInst>(
                        bin->getType(), ".lsr.phi." + std::to_string(m_lsrCounter++));
                    derivedPhi->setParent(loop.header);

                    // Insert derivedPhi with existing header phis
                    size_t phiInsertPos = 0;
                    const auto& hdrInsts = loop.header->getInstructions();
                    while (phiInsertPos < hdrInsts.size() && hdrInsts[phiInsertPos]->getOpcode() == IROpcode::Phi) {
                        ++phiInsertPos;
                    }
                    loop.header->insertInstruction(phiInsertPos, derivedPhi);

                    size_t latchPos = latch->hasTerminator()
                        ? (latch->getInstructions().size() - 1)
                        : latch->getInstructions().size();
                    auto derivedNext = std::make_shared<BinaryInst>(
                        matchedIV->isSub ? IROpcode::Sub : IROpcode::Add,
                        derivedPhi, derivedStep, bin->getType(),
                        ".lsr.next." + std::to_string(m_lsrCounter++));
                    derivedNext->setParent(latch);
                    latch->insertInstruction(latchPos, derivedNext);

                    derivedPhi->addIncoming(loop.preheader, derivedInit);
                    derivedPhi->addIncoming(latch, derivedNext);
                    derivedCache[key] = derivedPhi;
                }

                replaceAllUses(fn, inst.get(), derivedPhi);
                changed = true;
            }
        }
    }

    return changed;
}

bool StrengthReductionPass::runOnFunction(IRFunction& fn) {
    bool changed = false;

    for (auto& bb : fn.getBlocks()) {
        for (auto& inst : bb->getInstructions()) {
            if (auto* bin = dynamic_cast<BinaryInst*>(inst.get())) {
                bool inPlaceChanged = false;
                auto replacement = reduceBinary(*bin, inPlaceChanged);
                if (inPlaceChanged) {
                    changed = true;
                } else if (replacement) {
                    replaceAllUses(fn, inst.get(), replacement);
                    changed = true;
                }
            }
        }
    }

    if (reduceLoopInductionVars(fn)) {
        changed = true;
    }

    return changed;
}

} // namespace sph

