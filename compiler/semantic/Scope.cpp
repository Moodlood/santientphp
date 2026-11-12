#include "Scope.hpp"

namespace sph {

bool Scope::define(Symbol sym) {
    if (m_symbols.find(sym.name) != m_symbols.end()) {
        return false;
    }
    m_symbols.emplace(sym.name, std::move(sym));
    return true;
}

const Symbol* Scope::lookup(const std::string& name) const {
    auto it = m_symbols.find(name);
    if (it != m_symbols.end()) {
        return &it->second;
    }
    if (m_parent) {
        return m_parent->lookup(name);
    }
    return nullptr;
}

Symbol* Scope::lookup(const std::string& name) {
    auto it = m_symbols.find(name);
    if (it != m_symbols.end()) {
        return &it->second;
    }
    if (m_parent) {
        return m_parent->lookup(name);
    }
    return nullptr;
}

const Symbol* Scope::lookupCurrent(const std::string& name) const {
    auto it = m_symbols.find(name);
    if (it != m_symbols.end()) {
        return &it->second;
    }
    return nullptr;
}

} // namespace sph

