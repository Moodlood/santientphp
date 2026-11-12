#ifndef SPH_STORE_TO_LOAD_FORWARDING_PASS_HPP
#define SPH_STORE_TO_LOAD_FORWARDING_PASS_HPP

#include "Pass.hpp"

namespace sph {

class StoreToLoadForwardingPass : public FunctionPass {
public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "StoreToLoadForwardingPass";
    }
    bool runOnFunction(IRFunction& function) override;
};

} // namespace sph

#endif // SPH_STORE_TO_LOAD_FORWARDING_PASS_HPP
