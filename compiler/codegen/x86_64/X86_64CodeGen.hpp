#pragma once

#include "compiler/ir/IRBasicBlock.hpp"
#include "compiler/ir/IRFunction.hpp"
#include "compiler/ir/IRInstruction.hpp"
#include "compiler/ir/IRModule.hpp"
#include "compiler/target/Target.hpp"
#include <iosfwd>
#include <string>
#include <unordered_map>

namespace sph {

class X86_64CodeGen {
public:
    explicit X86_64CodeGen(std::ostream& out) : m_out(out) {}

    void setTarget(const Target& target) noexcept { m_target = target; }
    [[nodiscard]] const Target& getTarget() const noexcept { return m_target; }

    void setDebugInfo(bool enable) noexcept { m_debugInfo = enable; }
    [[nodiscard]] bool isDebugInfo() const noexcept { return m_debugInfo; }

    void generate(const IRModule& module);

private:
    void generateFunction(const IRFunction& fn);
    void generateBasicBlock(const IRBasicBlock& bb, const IRFunction& fn);
    void generateInstruction(const IRInstruction& inst, const IRFunction& fn);

    // Calling Convention Helpers
    [[nodiscard]] bool isSysV() const noexcept {
        return m_target.getCallingConvention() == TargetCallingConvention::SysV64;
    }
    [[nodiscard]] size_t getMaxRegisterParams() const noexcept {
        return isSysV() ? 6 : 4;
    }
    [[nodiscard]] const char* getParamReg(size_t index) const noexcept;
    [[nodiscard]] std::string getReg32(const std::string& reg64) const;

    // Helpers
    int32_t getSlotOffset(const IRValue& val);
    void loadValueToReg(const IRValue& val, const std::string& reg);
    void storeRegToSlot(const std::string& reg, int32_t offset);
    std::string getLabel(const IRBasicBlock& bb, const IRFunction& fn) const;

    std::ostream& m_out;
    Target m_target;
    std::unordered_map<const IRValue*, int32_t> m_slots;
    std::unordered_map<std::string, std::string> m_stringLabels;
    int32_t m_currentOffset = 0;
    int32_t m_frameSize = 0;
    bool m_debugInfo = false;
    SourceLocation m_lastEmittedLoc;
};

} // namespace sph

