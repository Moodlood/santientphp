#pragma once

#include "compiler/ir/IRBasicBlock.hpp"
#include "compiler/ir/IRFunction.hpp"
#include "compiler/ir/IRInstruction.hpp"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sph {

struct LiveInterval {
    const IRValue* value = nullptr;
    int start = 0;
    int end = 0;
    std::string assignedReg;
    bool isSpilled = false;
};

class LinearScanAllocator {
public:
    LinearScanAllocator(const IRFunction& fn, bool isSysV);

    void allocate();

    [[nodiscard]] const std::string& getRegister(const IRValue* val) const;
    [[nodiscard]] bool hasRegister(const IRValue* val) const;
    [[nodiscard]] bool isSpilled(const IRValue* val) const;
    [[nodiscard]] const std::vector<std::string>& getUsedCalleeSavedRegs() const noexcept {
        return m_usedCalleeSaved;
    }

private:
    void computeLiveIntervals();
    void runLinearScan();

    const IRFunction& m_fn;
    bool m_isSysV;
    std::vector<LiveInterval> m_intervals;
    std::unordered_map<const IRValue*, size_t> m_intervalMap;
    std::unordered_map<const IRValue*, std::string> m_physRegs;
    std::unordered_set<const IRValue*> m_spilled;
    std::vector<std::string> m_usedCalleeSaved;
    std::string m_emptyStr;
};

} // namespace sph

