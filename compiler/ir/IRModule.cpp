#include "IRModule.hpp"

namespace sph {

const IRFunction* IRModule::getFunction(const std::string& name) const noexcept {
    for (const auto& fn : m_functions) {
        if (fn->getName() == name) return fn.get();
    }
    return nullptr;
}

IRFunction* IRModule::getFunction(const std::string& name) noexcept {
    for (auto& fn : m_functions) {
        if (fn->getName() == name) return fn.get();
    }
    return nullptr;
}

} // namespace sph

