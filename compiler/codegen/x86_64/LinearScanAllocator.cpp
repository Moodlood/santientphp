#include "LinearScanAllocator.hpp"
#include "compiler/optimizer/Dominance.hpp"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sph {

LinearScanAllocator::LinearScanAllocator(const IRFunction& fn, bool isSysV)
    : m_fn(fn), m_isSysV(isSysV) {}

void LinearScanAllocator::allocate() {
    computeLiveIntervals();
    runLinearScan();
}

bool LinearScanAllocator::valuesInterfere(const IRValue* a, const IRValue* b) const {
    if (!a || !b || a == b) return false;
    auto it = m_interference.find(a);
    if (it != m_interference.end() && it->second.count(b) > 0) {
        return true;
    }
    return false;
}

void LinearScanAllocator::computeLiveIntervals() {
    m_intervals.clear();
    m_intervalMap.clear();
    m_coalescePrefs.clear();
    m_interference.clear();

    const auto& blocks = m_fn.getBlocks();
    if (blocks.empty()) return;

    // Estimate loop nesting depth per block to weight spill costs accurately
    std::unordered_map<const IRBasicBlock*, size_t> loopDepth;
    if (blocks.size() >= 2) {
        DominatorTree dt(m_fn);
        auto loops = findNaturalLoops(m_fn, dt);
        for (const auto& loop : loops) {
            for (const auto* lb : loop.blocks) {
                loopDepth[lb]++;
            }
        }
    }

    auto blockWeight = [&](const IRBasicBlock* bb) -> uint64_t {
        size_t d = loopDepth[bb];
        return 10ULL * (1ULL << std::min<size_t>(d, 6));
    };

    int instIndex = 0;
    std::unordered_map<const IRBasicBlock*, int> blockStart;
    std::unordered_map<const IRBasicBlock*, int> blockEnd;

    auto getOrCreateInterval = [&](const IRValue* val, int defPoint) -> LiveInterval& {
        auto it = m_intervalMap.find(val);
        if (it != m_intervalMap.end()) {
            return m_intervals[it->second];
        }
        size_t idx = m_intervals.size();
        m_intervalMap[val] = idx;
        LiveInterval li;
        li.value = val;
        li.start = defPoint;
        li.end = defPoint + 1;
        m_intervals.push_back(li);
        return m_intervals.back();
    };

    for (const auto& param : m_fn.getParameters()) {
        auto& li = getOrCreateInterval(param.get(), 0);
        li.spillCost += 15;
    }

    // Collect SSA incoming edges (predBB -> phiVal) and assign linear instruction indices
    std::unordered_map<const IRBasicBlock*, std::vector<const IRValue*>> outgoingPhiUses;

    for (const auto& bb : blocks) {
        blockStart[bb.get()] = instIndex;
        uint64_t w = blockWeight(bb.get());
        for (const auto& inst : bb->getInstructions()) {
            if (inst->getOpcode() == IROpcode::Phi) {
                const auto& phi = static_cast<const PhiInst&>(*inst);
                for (const auto& [predBb, inVal] : phi.getIncoming()) {
                    if (predBb && inVal && !inVal->isConstant()) {
                        outgoingPhiUses[predBb].push_back(inVal.get());
                        m_coalescePrefs[phi.getIncomingValue(predBb).get()].push_back(&phi);
                        m_coalescePrefs[&phi].push_back(inVal.get());
                    }
                }
            } else {
                auto operands = getInstructionOperands(*inst);
                for (const auto* op : operands) {
                    if (op && !op->isConstant()) {
                        auto it = m_intervalMap.find(op);
                        if (it != m_intervalMap.end()) {
                            m_intervals[it->second].end = std::max(m_intervals[it->second].end, instIndex);
                            m_intervals[it->second].spillCost += w;
                        } else {
                            auto& li = getOrCreateInterval(op, 0);
                            li.end = std::max(li.end, instIndex);
                            li.spillCost += w;
                        }
                    }
                }
            }

            // Record two-address arithmetic coalescing preferences
            auto op = inst->getOpcode();
            if (op == IROpcode::Add || op == IROpcode::Sub || op == IROpcode::Mul ||
                op == IROpcode::BitwiseAnd || op == IROpcode::BitwiseOr || op == IROpcode::BitwiseXor) {
                const auto& bin = static_cast<const BinaryInst&>(*inst);
                if (bin.getLhs() && !bin.getLhs()->isConstant()) {
                    m_coalescePrefs[bin.getLhs().get()].push_back(inst.get());
                    m_coalescePrefs[inst.get()].push_back(bin.getLhs().get());
                }
                if ((op != IROpcode::Sub) && bin.getRhs() && !bin.getRhs()->isConstant()) {
                    m_coalescePrefs[bin.getRhs().get()].push_back(inst.get());
                    m_coalescePrefs[inst.get()].push_back(bin.getRhs().get());
                }
            }

            if (inst->getOpcode() != IROpcode::Store &&
                inst->getOpcode() != IROpcode::Ret &&
                inst->getOpcode() != IROpcode::Br &&
                inst->getOpcode() != IROpcode::CondBr &&
                !inst->getType()->isVoid()) {
                auto& li = getOrCreateInterval(inst.get(), instIndex);
                li.spillCost += w;
            }

            instIndex += 2;
        }
        blockEnd[bb.get()] = instIndex;
    }

    // Credit spill cost for outgoing Phi uses on the predecessor block
    for (const auto& [predBb, vals] : outgoingPhiUses) {
        uint64_t pw = blockWeight(predBb);
        for (const auto* v : vals) {
            auto it = m_intervalMap.find(v);
            if (it != m_intervalMap.end()) {
                m_intervals[it->second].spillCost += pw;
            }
        }
    }

    // Exact SSA CFG backward liveness (Phi inputs belong to pred's liveOut, not succ's liveIn)
    bool changed = true;
    std::unordered_map<const IRBasicBlock*, std::unordered_set<const IRValue*>> liveIn;
    std::unordered_map<const IRBasicBlock*, std::unordered_set<const IRValue*>> liveOut;

    while (changed) {
        changed = false;
        for (auto it = blocks.rbegin(); it != blocks.rend(); ++it) {
            const auto* bb = it->get();
            std::unordered_set<const IRValue*> newLiveOut;

            for (auto* succ : getSuccessors(*bb)) {
                for (const auto* val : liveIn[succ]) {
                    newLiveOut.insert(val);
                }
                for (const auto& sInst : succ->getInstructions()) {
                    if (sInst->getOpcode() != IROpcode::Phi) break;
                    const auto& phi = static_cast<const PhiInst&>(*sInst);
                    auto inVal = phi.getIncomingValue(bb);
                    if (inVal && !inVal->isConstant()) {
                        newLiveOut.insert(inVal.get());
                    }
                }
            }

            if (newLiveOut != liveOut[bb]) {
                liveOut[bb] = std::move(newLiveOut);
                changed = true;
            }

            std::unordered_set<const IRValue*> newLiveIn = liveOut[bb];
            for (auto instIt = bb->getInstructions().rbegin(); instIt != bb->getInstructions().rend(); ++instIt) {
                const auto* inst = instIt->get();
                newLiveIn.erase(inst);
                if (inst->getOpcode() != IROpcode::Phi) {
                    for (const auto* op : getInstructionOperands(*inst)) {
                        if (op && !op->isConstant()) {
                            newLiveIn.insert(op);
                        }
                    }
                }
            }

            if (newLiveIn != liveIn[bb]) {
                liveIn[bb] = std::move(newLiveIn);
                changed = true;
            }
        }
    }

    // Build exact per-value interference sets for sound copy coalescing and span intervals
    auto addInterfere = [&](const IRValue* x, const IRValue* y) {
        if (!x || !y || x == y) return;
        m_interference[x].insert(y);
        m_interference[y].insert(x);
    };

    for (const auto& bb : blocks) {
        int bStart = blockStart[bb.get()];
        int bEnd = blockEnd[bb.get()];

        for (const auto* val : liveIn[bb.get()]) {
            auto it = m_intervalMap.find(val);
            if (it != m_intervalMap.end()) {
                m_intervals[it->second].start = std::min(m_intervals[it->second].start, bStart);
                m_intervals[it->second].end = std::max(m_intervals[it->second].end, bStart);
            }
        }
        for (const auto* val : liveOut[bb.get()]) {
            auto it = m_intervalMap.find(val);
            if (it != m_intervalMap.end()) {
                m_intervals[it->second].end = std::max(m_intervals[it->second].end, bEnd);
            }
        }

        std::unordered_set<const IRValue*> live = liveOut[bb.get()];
        std::vector<const IRValue*> blockPhis;
        const auto& bbInsts = bb->getInstructions();
        for (auto instIt = bbInsts.rbegin(); instIt != bbInsts.rend(); ++instIt) {
            const auto* inst = instIt->get();
            if (inst->getOpcode() == IROpcode::Phi) {
                blockPhis.push_back(inst);
                continue;
            }
            if (!inst->getType()->isVoid() &&
                inst->getOpcode() != IROpcode::Store &&
                inst->getOpcode() != IROpcode::Ret &&
                inst->getOpcode() != IROpcode::Br &&
                inst->getOpcode() != IROpcode::CondBr) {
                for (const auto* l : live) {
                    addInterfere(inst, l);
                }
            }
            live.erase(inst);
            for (const auto* op : getInstructionOperands(*inst)) {
                if (op && !op->isConstant()) {
                    live.insert(op);
                }
            }
        }

        // All Phis defined in the same block become live together against live-after-Phis
        for (const auto* p : blockPhis) {
            live.erase(p);
        }
        for (size_t i = 0; i < blockPhis.size(); ++i) {
            for (const auto* l : live) {
                addInterfere(blockPhis[i], l);
            }
            for (size_t j = i + 1; j < blockPhis.size(); ++j) {
                addInterfere(blockPhis[i], blockPhis[j]);
            }
        }
    }

    // Parameters are simultaneously live at function entry
    const auto& params = m_fn.getParameters();
    for (size_t i = 0; i < params.size(); ++i) {
        for (const auto* l : liveIn[blocks[0].get()]) {
            addInterfere(params[i].get(), l);
        }
        for (size_t j = i + 1; j < params.size(); ++j) {
            addInterfere(params[i].get(), params[j].get());
        }
    }
}

void LinearScanAllocator::runLinearScan() {
    m_physRegs.clear();
    m_spilled.clear();
    m_usedCalleeSaved.clear();

    if (m_intervals.empty()) return;

    // Filter out allocas, big structs, arrays, slices, and floats (handled via XMM/stack)
    std::vector<LiveInterval*> candidates;
    for (auto& li : m_intervals) {
        if (!li.value) continue;
        auto type = li.value->getType();
        if (li.value->isInstruction()) {
            auto op = static_cast<const IRInstruction*>(li.value)->getOpcode();
            if (op == IROpcode::Alloca) continue;
        }
        if (type->isStruct() || type->isArray() || type->isSlice() || type->isFloatingPoint()) {
            continue;
        }
        candidates.push_back(&li);
    }

    std::sort(candidates.begin(), candidates.end(), [](const LiveInterval* a, const LiveInterval* b) {
        if (a->start != b->start) return a->start < b->start;
        return a->end < b->end;
    });

    std::vector<std::string> calleeSavedRegs = {"rbx", "r12", "r13", "r14", "r15"};
    if (!m_isSysV) {
        calleeSavedRegs.push_back("rsi");
        calleeSavedRegs.push_back("rdi");
    }
    const std::vector<std::string> callerSavedRegs = {"r8", "r9", "r10", "r11"};

    auto isCalleeSavedReg = [&](const std::string& reg) {
        return std::find(calleeSavedRegs.begin(), calleeSavedRegs.end(), reg) != calleeSavedRegs.end();
    };

    std::vector<LiveInterval*> active;
    std::unordered_set<std::string> freeCalleeRegs(calleeSavedRegs.begin(), calleeSavedRegs.end());
    std::unordered_set<std::string> freeCallerRegs(callerSavedRegs.begin(), callerSavedRegs.end());

    auto popDeterministicFree = [&](const std::vector<std::string>& order,
                                    std::unordered_set<std::string>& freeSet) -> std::string {
        for (const auto& r : order) {
            if (freeSet.erase(r) > 0) return r;
        }
        return "";
    };

    auto regInUseByActive = [&](const std::string& reg, const LiveInterval* skip) -> bool {
        for (const auto* act : active) {
            if (act != skip && act->assignedReg == reg) return true;
        }
        return false;
    };

    auto expireOldIntervals = [&](int currentStart) {
        for (auto it = active.begin(); it != active.end(); ) {
            if ((*it)->end < currentStart) {
                const std::string reg = (*it)->assignedReg;
                LiveInterval* dying = *it;
                it = active.erase(it);
                if (!regInUseByActive(reg, dying)) {
                    if (isCalleeSavedReg(reg)) {
                        freeCalleeRegs.insert(reg);
                    } else {
                        freeCallerRegs.insert(reg);
                    }
                }
            } else {
                ++it;
            }
        }
    };

    // Call points inside instruction index space (values strictly spanning across a call need callee-saved)
    std::vector<int> callPoints;
    int idx = 0;
    for (const auto& bb : m_fn.getBlocks()) {
        for (const auto& inst : bb->getInstructions()) {
            auto op = inst->getOpcode();
            if (op == IROpcode::Call || op == IROpcode::RcAlloc ||
                op == IROpcode::RcRetain || op == IROpcode::RcRelease ||
                op == IROpcode::ArcAlloc || op == IROpcode::ArcRetain ||
                op == IROpcode::ArcRelease || op == IROpcode::SetJmp) {
                callPoints.push_back(idx);
            }
            idx += 2;
        }
    }

    auto spansCall = [&](const LiveInterval& li) -> bool {
        for (int cp : callPoints) {
            if (li.value->isParameter()) {
                if (cp >= 0 && cp < li.end) return true;
            } else {
                if (cp > li.start && cp < li.end) return true;
            }
        }
        return false;
    };

    auto canShareRegWithActive = [&](const LiveInterval* current, const std::string& reg) -> bool {
        for (const auto* act : active) {
            if (act->assignedReg == reg && valuesInterfere(act->value, current->value)) {
                return false;
            }
        }
        return true;
    };

    for (auto* current : candidates) {
        expireOldIntervals(current->start);

        bool crossesCall = spansCall(*current);
        std::string chosenReg;

        // Try copy-coalescing hint first (Phi <-> incoming or two-address Arithmetic)
        auto prefIt = m_coalescePrefs.find(current->value);
        if (prefIt != m_coalescePrefs.end()) {
            for (const IRValue* prefVal : prefIt->second) {
                auto regIt = m_physRegs.find(prefVal);
                if (regIt == m_physRegs.end()) continue;
                const std::string& hintReg = regIt->second;
                if (crossesCall && !isCalleeSavedReg(hintReg)) continue;

                // Ensure hintReg is either completely free or all active holders do not interfere with current
                if (freeCallerRegs.count(hintReg) || freeCalleeRegs.count(hintReg) ||
                    canShareRegWithActive(current, hintReg)) {
                    chosenReg = hintReg;
                    freeCallerRegs.erase(hintReg);
                    freeCalleeRegs.erase(hintReg);
                    break;
                }
            }
        }

        // Also allow coalescing against an active register holding an interval whose end == current->start
        if (chosenReg.empty()) {
            for (const auto* act : active) {
                if (act->end == current->start && !valuesInterfere(act->value, current->value)) {
                    if (crossesCall && !isCalleeSavedReg(act->assignedReg)) continue;
                    // Prefer only when explicitly hinted OR when no free register exists below
                    break;
                }
            }
        }

        if (chosenReg.empty()) {
            if (crossesCall) {
                chosenReg = popDeterministicFree(calleeSavedRegs, freeCalleeRegs);
            } else {
                chosenReg = popDeterministicFree(callerSavedRegs, freeCallerRegs);
                if (chosenReg.empty()) {
                    chosenReg = popDeterministicFree(calleeSavedRegs, freeCalleeRegs);
                }
            }
        }

        if (!chosenReg.empty()) {
            current->assignedReg = chosenReg;
            m_physRegs[current->value] = chosenReg;
            active.push_back(current);

            if (isCalleeSavedReg(chosenReg)) {
                if (std::find(m_usedCalleeSaved.begin(), m_usedCalleeSaved.end(), chosenReg) == m_usedCalleeSaved.end()) {
                    m_usedCalleeSaved.push_back(chosenReg);
                }
            }
        } else {
            // Weighted spill heuristic: protect hot inner-loop variables over outer/cold ones
            LiveInterval* toSpill = nullptr;
            auto bestVictimScore = [&](const LiveInterval* cand) {
                // Evict only non-shared registers usable by current
                return std::make_pair(cand->spillCost, -static_cast<int64_t>(cand->end));
            };

            for (auto* act : active) {
                if (crossesCall && !isCalleeSavedReg(act->assignedReg)) continue;
                if (regInUseByActive(act->assignedReg, act)) continue; // Shared web
                if (!toSpill || bestVictimScore(act) < bestVictimScore(toSpill)) {
                    toSpill = act;
                }
            }

            if (toSpill &&
                (toSpill->spillCost < current->spillCost ||
                 (toSpill->spillCost == current->spillCost && toSpill->end > current->end))) {
                std::string stolenReg = toSpill->assignedReg;
                toSpill->assignedReg.clear();
                toSpill->isSpilled = true;
                m_physRegs.erase(toSpill->value);
                m_spilled.insert(toSpill->value);
                active.erase(std::find(active.begin(), active.end(), toSpill));

                current->assignedReg = stolenReg;
                m_physRegs[current->value] = stolenReg;
                active.push_back(current);

                if (isCalleeSavedReg(stolenReg)) {
                    if (std::find(m_usedCalleeSaved.begin(), m_usedCalleeSaved.end(), stolenReg) == m_usedCalleeSaved.end()) {
                        m_usedCalleeSaved.push_back(stolenReg);
                    }
                }
            } else {
                current->isSpilled = true;
                m_spilled.insert(current->value);
            }
        }
    }
}

const std::string& LinearScanAllocator::getRegister(const IRValue* val) const {
    auto it = m_physRegs.find(val);
    if (it != m_physRegs.end()) {
        return it->second;
    }
    return m_emptyStr;
}

bool LinearScanAllocator::hasRegister(const IRValue* val) const {
    return m_physRegs.count(val) > 0;
}

bool LinearScanAllocator::isSpilled(const IRValue* val) const {
    return m_spilled.count(val) > 0;
}

} // namespace sph
