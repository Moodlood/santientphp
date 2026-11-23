#include "LinearScanAllocator.hpp"
#include "compiler/optimizer/Dominance.hpp"
#include <algorithm>
#include <queue>

namespace sph {

LinearScanAllocator::LinearScanAllocator(const IRFunction& fn, bool isSysV)
    : m_fn(fn), m_isSysV(isSysV) {}

void LinearScanAllocator::allocate() {
    computeLiveIntervals();
    runLinearScan();
}

void LinearScanAllocator::computeLiveIntervals() {
    m_intervals.clear();
    m_intervalMap.clear();

    // Collect all values that can receive a register:
    // Parameters and instructions that produce a scalar/pointer result.
    int instIndex = 0;
    std::unordered_map<const IRBasicBlock*, int> blockStart;
    std::unordered_map<const IRBasicBlock*, int> blockEnd;
    std::unordered_set<int> callIndices;

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

    // Parameters are live from entry
    for (const auto& param : m_fn.getParameters()) {
        getOrCreateInterval(param.get(), 0);
    }

    // Number instructions and find definitions/uses
    for (const auto& bb : m_fn.getBlocks()) {
        blockStart[bb.get()] = instIndex;
        for (const auto& inst : bb->getInstructions()) {
            if (inst->getOpcode() == IROpcode::Call) {
                callIndices.insert(instIndex);
            }

            // Operands are uses
            auto operands = getInstructionOperands(*inst);
            for (const auto* op : operands) {
                if (op && !op->isConstant()) {
                    auto it = m_intervalMap.find(op);
                    if (it != m_intervalMap.end()) {
                        m_intervals[it->second].end = std::max(m_intervals[it->second].end, instIndex);
                    } else {
                        auto& li = getOrCreateInterval(op, 0);
                        li.end = std::max(li.end, instIndex);
                    }
                }
            }

            // If instruction produces a value, it is a definition
            if (inst->getOpcode() != IROpcode::Store &&
                inst->getOpcode() != IROpcode::Ret &&
                inst->getOpcode() != IROpcode::Br &&
                inst->getOpcode() != IROpcode::CondBr &&
                !inst->getType()->isVoid()) {
                getOrCreateInterval(inst.get(), instIndex);
            }

            instIndex += 2;
        }
        blockEnd[bb.get()] = instIndex;
    }

    // Fixed-point liveness across basic blocks
    bool changed = true;
    std::unordered_map<const IRBasicBlock*, std::unordered_set<const IRValue*>> liveIn;
    std::unordered_map<const IRBasicBlock*, std::unordered_set<const IRValue*>> liveOut;

    while (changed) {
        changed = false;
        for (auto it = m_fn.getBlocks().rbegin(); it != m_fn.getBlocks().rend(); ++it) {
            const auto* bb = it->get();
            std::unordered_set<const IRValue*> newLiveOut;

            for (auto* succ : getSuccessors(*bb)) {
                for (const auto* val : liveIn[succ]) {
                    newLiveOut.insert(val);
                }
            }

            if (newLiveOut != liveOut[bb]) {
                liveOut[bb] = newLiveOut;
                changed = true;
            }

            std::unordered_set<const IRValue*> newLiveIn = liveOut[bb];
            for (auto instIt = bb->getInstructions().rbegin(); instIt != bb->getInstructions().rend(); ++instIt) {
                const auto* inst = instIt->get();
                newLiveIn.erase(inst);
                for (const auto* op : getInstructionOperands(*inst)) {
                    if (op && !op->isConstant()) {
                        newLiveIn.insert(op);
                    }
                }
            }

            if (newLiveIn != liveIn[bb]) {
                liveIn[bb] = newLiveIn;
                changed = true;
            }
        }
    }

    // Extend intervals according to block liveIn and liveOut
    for (const auto& bb : m_fn.getBlocks()) {
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
    }
}

void LinearScanAllocator::runLinearScan() {
    m_physRegs.clear();
    m_spilled.clear();
    m_usedCalleeSaved.clear();

    if (m_intervals.empty()) return;

    // Filter out allocas, big structs, and arrays that cannot be held in registers
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

    // Sort by start position
    std::sort(candidates.begin(), candidates.end(), [](const LiveInterval* a, const LiveInterval* b) {
        return a->start < b->start;
    });

    // Callee-saved registers (safe across calls):
    const std::vector<std::string> calleeSavedRegs = {"rbx", "r12", "r13", "r14", "r15"};

    // Caller-saved registers (scratch, only for intervals not spanning calls):
    const std::vector<std::string> callerSavedRegs = {"r8", "r9", "r10", "r11"};

    std::vector<LiveInterval*> active;
    std::unordered_set<std::string> freeCalleeRegs(calleeSavedRegs.begin(), calleeSavedRegs.end());
    std::unordered_set<std::string> freeCallerRegs(callerSavedRegs.begin(), callerSavedRegs.end());

    auto expireOldIntervals = [&](int currentStart) {
        for (auto it = active.begin(); it != active.end(); ) {
            if ((*it)->end < currentStart) {
                const auto& reg = (*it)->assignedReg;
                if (std::find(calleeSavedRegs.begin(), calleeSavedRegs.end(), reg) != calleeSavedRegs.end()) {
                    freeCalleeRegs.insert(reg);
                } else {
                    freeCallerRegs.insert(reg);
                }
                it = active.erase(it);
            } else {
                ++it;
            }
        }
    };

    // Check if interval spans a function call
    std::unordered_set<int> callPoints;
    int idx = 0;
    for (const auto& bb : m_fn.getBlocks()) {
        for (const auto& inst : bb->getInstructions()) {
            if (inst->getOpcode() == IROpcode::Call) {
                callPoints.insert(idx);
            }
            idx += 2;
        }
    }

    auto spansCall = [&](const LiveInterval& li) -> bool {
        for (int cp : callPoints) {
            if (cp >= li.start && cp <= li.end) return true;
        }
        return false;
    };

    for (auto* current : candidates) {
        expireOldIntervals(current->start);

        bool crossesCall = spansCall(*current);
        std::string chosenReg;

        if (crossesCall) {
            // Must use callee-saved register
            if (!freeCalleeRegs.empty()) {
                chosenReg = *freeCalleeRegs.begin();
                freeCalleeRegs.erase(freeCalleeRegs.begin());
            }
        } else {
            // Can use either caller-saved or callee-saved
            if (!freeCallerRegs.empty()) {
                chosenReg = *freeCallerRegs.begin();
                freeCallerRegs.erase(freeCallerRegs.begin());
            } else if (!freeCalleeRegs.empty()) {
                chosenReg = *freeCalleeRegs.begin();
                freeCalleeRegs.erase(freeCalleeRegs.begin());
            }
        }

        if (!chosenReg.empty()) {
            current->assignedReg = chosenReg;
            m_physRegs[current->value] = chosenReg;
            active.push_back(current);

            // Track callee-saved register usage
            if (std::find(calleeSavedRegs.begin(), calleeSavedRegs.end(), chosenReg) != calleeSavedRegs.end()) {
                if (std::find(m_usedCalleeSaved.begin(), m_usedCalleeSaved.end(), chosenReg) == m_usedCalleeSaved.end()) {
                    m_usedCalleeSaved.push_back(chosenReg);
                }
            }
        } else {
            // Spill
            current->isSpilled = true;
            m_spilled.insert(current->value);
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

