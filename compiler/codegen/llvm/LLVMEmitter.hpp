#pragma once

#include "compiler/ir/IRModule.hpp"
#include "compiler/target/Target.hpp"
#include <iosfwd>
#include <string>
#include <unordered_map>

namespace sph {

class LLVMEmitter {
public:
    explicit LLVMEmitter(std::ostream& out) : m_out(out) {}

    void setTarget(const Target& target) noexcept { m_target = target; }
    [[nodiscard]] const Target& getTarget() const noexcept { return m_target; }

    void setDebugInfo(bool enable) noexcept { m_debugInfo = enable; }
    [[nodiscard]] bool isDebugInfo() const noexcept { return m_debugInfo; }

    void emit(const IRModule& module);

    [[nodiscard]] static std::string mapType(const Type& type);
    [[nodiscard]] std::string formatValue(const IRValue& val) const;

private:
    void emitFunction(const IRFunction& fn);
    void emitBasicBlock(const IRBasicBlock& bb);
    void emitInstruction(const IRInstruction& inst);

    std::ostream& m_out;
    Target m_target;
    bool m_debugInfo = false;
};

} // namespace sph

