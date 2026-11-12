#pragma once

#include "Pass.hpp"
#include <unordered_set>

namespace sph {

class FunctionInliningPass : public ModulePass {
public:
    explicit FunctionInliningPass(size_t maxInstructions = 10)
        : m_maxInstructions(maxInstructions) {}

    [[nodiscard]] std::string_view getName() const noexcept override {
        return "FunctionInliningPass";
    }

    bool runOnModule(IRModule& module) override;

private:
    bool isInlineCandidate(const IRFunction& fn) const noexcept;
    bool inlineFunctionCall(IRFunction& caller, size_t blockIdx, size_t instIdx, const IRFunction& callee);

    size_t m_maxInstructions;
};

} // namespace sph

