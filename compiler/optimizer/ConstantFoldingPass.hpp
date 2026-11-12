#pragma once

#include "Pass.hpp"

namespace sph {

class ConstantFoldingPass : public FunctionPass {
public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "ConstantFoldingPass";
    }

    bool runOnFunction(IRFunction& fn) override;

private:
    std::shared_ptr<IRValue> foldBinary(const BinaryInst& bin);
    std::shared_ptr<IRValue> foldUnary(const UnaryInst& un);
    std::shared_ptr<IRValue> foldCast(const CastInst& cast);
};

} // namespace sph

