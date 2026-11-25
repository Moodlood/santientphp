#pragma once

#include "Pass.hpp"

namespace sph {

class StrengthReductionPass : public FunctionPass {
public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "StrengthReductionPass";
    }

    bool runOnFunction(IRFunction& fn) override;

private:
    std::shared_ptr<IRValue> reduceBinary(BinaryInst& bin, bool& inPlaceChanged);
    bool reduceLoopInductionVars(IRFunction& fn);
    size_t m_lsrCounter = 0;
};

} // namespace sph

