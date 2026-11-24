#pragma once

#include "LinearScanAllocator.hpp"
#include "compiler/ir/IRBasicBlock.hpp"
#include "compiler/ir/IRFunction.hpp"
#include "compiler/ir/IRInstruction.hpp"
#include "compiler/ir/IRModule.hpp"
#include "compiler/target/Target.hpp"
#include <iosfwd>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace sph {

class X86_64CodeGen {
public:
    explicit X86_64CodeGen(std::ostream& out) : m_out(out), m_pOut(&out) {}

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
    [[nodiscard]] bool isInternalFastCC(const IRFunction& fn) const noexcept {
        return fn.isInternal();
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
    void storeRegToValue(const std::string& reg, const IRValue& val);
    bool tryDirectRegisterBinaryOp(const BinaryInst& binInst);
    void emitParallelPhiCopies(const IRBasicBlock& target, const IRBasicBlock& fromBlock);
    std::string getLabel(const IRBasicBlock& bb, const IRFunction& fn) const;
    void emitEpilogue(const IRFunction& fn);

    std::ostream& m_out;
    std::ostream* m_pOut;
    Target m_target;
    std::unique_ptr<LinearScanAllocator> m_lsra;
    std::unordered_map<const IRValue*, int32_t> m_slots;
    std::unordered_map<std::string, std::string> m_stringLabels;
    std::vector<std::string> m_usedCalleeSaved;
    std::unordered_map<std::string, int32_t> m_calleeSavedSlots;
    int32_t m_currentOffset = 0;
    int32_t m_frameSize = 0;
    bool m_currentFnReturnsSret = false;
    int32_t m_currentFnSretSlot = 0;
    int32_t m_currentFnSretSize = 0;
    int32_t m_tcoScratchOffset = 0;
    bool m_debugInfo = false;
    SourceLocation m_lastEmittedLoc;
};

} // namespace sph

