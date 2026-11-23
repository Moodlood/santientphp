#pragma once

#include "compiler/ir/IRBasicBlock.hpp"
#include "compiler/ir/IRFunction.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sph {

// Control Flow Graph Helper Functions
std::vector<IRBasicBlock*> getPredecessors(const IRBasicBlock& bb, const IRFunction& fn);
std::vector<IRBasicBlock*> getSuccessors(const IRBasicBlock& bb);

class DominatorTree {
public:
    explicit DominatorTree(const IRFunction& fn);

    [[nodiscard]] IRBasicBlock* getIdom(const IRBasicBlock* bb) const;
    [[nodiscard]] bool dominates(const IRBasicBlock* a, const IRBasicBlock* b) const;
    [[nodiscard]] bool strictlyDominates(const IRBasicBlock* a, const IRBasicBlock* b) const;
    [[nodiscard]] const std::unordered_set<IRBasicBlock*>& getDominanceFrontier(const IRBasicBlock* bb) const;

    // Iterated Dominance Frontier (IDF) for a set of defining blocks
    [[nodiscard]] std::unordered_set<IRBasicBlock*> computeIDF(
        const std::unordered_set<IRBasicBlock*>& defBlocks) const;

    [[nodiscard]] const std::vector<IRBasicBlock*>& getChildren(const IRBasicBlock* bb) const;
    [[nodiscard]] const std::vector<IRBasicBlock*>& getRPOBlocks() const noexcept { return m_rpoBlocks; }

private:
    void computeRPO(const IRFunction& fn);
    void computeDominators(const IRFunction& fn);
    void computeFrontiers(const IRFunction& fn);
    IRBasicBlock* intersect(IRBasicBlock* b1, IRBasicBlock* b2) const;

    std::vector<IRBasicBlock*> m_rpoBlocks;
    std::unordered_map<const IRBasicBlock*, int> m_rpoIndex;
    std::unordered_map<const IRBasicBlock*, IRBasicBlock*> m_idom;
    std::unordered_map<const IRBasicBlock*, std::vector<IRBasicBlock*>> m_domChildren;
    std::unordered_map<const IRBasicBlock*, std::unordered_set<IRBasicBlock*>> m_frontiers;
    std::unordered_set<IRBasicBlock*> m_emptySet;
    std::vector<IRBasicBlock*> m_emptyVec;
};

} // namespace sph

