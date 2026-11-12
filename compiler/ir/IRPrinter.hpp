#pragma once

#include "IRModule.hpp"
#include <iosfwd>
#include <string>

namespace sph {

class IRPrinter {
public:
    explicit IRPrinter(std::ostream& out) : m_out(out) {}

    void print(const IRModule& module);
    void print(const IRFunction& fn);
    void print(const IRBasicBlock& bb);
    void print(const IRInstruction& inst);

    [[nodiscard]] std::string formatValue(const IRValue& val) const;

private:
    std::ostream& m_out;
};

} // namespace sph

