#include "IRFunction.hpp"

namespace sph {

IRBasicBlock* IRFunction::createBlock(const std::string& nameHint) {
    std::string uniqueName = nameHint;
    if (nameHint != "entry") {
        uniqueName += "." + std::to_string(m_blockCounter++);
    }

    auto bb = std::make_unique<IRBasicBlock>(std::move(uniqueName), this);
    IRBasicBlock* ptr = bb.get();
    m_blocks.push_back(std::move(bb));
    return ptr;
}

IRBasicBlock* IRFunction::getEntryBlock() const noexcept {
    if (m_blocks.empty()) return nullptr;
    return m_blocks.front().get();
}

} // namespace sph

