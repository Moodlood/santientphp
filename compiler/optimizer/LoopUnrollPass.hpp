#pragma once

#include "Pass.hpp"

namespace sph {

class LoopUnrollPass : public FunctionPass {
public:
    explicit LoopUnrollPass(size_t unrollFactor = 4) noexcept
        : m_unrollFactor(unrollFactor) {}

    [[nodiscard]] std::string_view getName() const noexcept override {
        return "LoopUnrollPass";
    }

    bool runOnFunction(IRFunction& fn) override;

private:
    size_t m_unrollFactor;
    size_t m_counter = 0;
};

} // namespace sph

