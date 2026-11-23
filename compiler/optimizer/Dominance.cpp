#include "Dominance.hpp"
#include "compiler/ir/IRInstruction.hpp"
#include <algorithm>
#include <functional>
#include <queue>

namespace sph {

std::vector<IRBasicBlock*> getSuccessors(const IRBasicBlock& bb) {
    std::vector<IRBasicBlock*> succs;
    const auto* term = bb.getTerminator();
    if (!term) return succs;

    if (term->getOpcode() == IROpcode::Br) {
        const auto* br = static_cast<const BranchInst*>(term);
        if (br->getTarget()) succs.push_back(br->getTarget());
    } else if (term->getOpcode() == IROpcode::CondBr) {
        const auto* condBr = static_cast<const CondBranchInst*>(term);
        if (condBr->getTrueTarget()) succs.push_back(condBr->getTrueTarget());
        if (condBr->getFalseTarget() && condBr->getFalseTarget() != condBr->getTrueTarget()) {
            succs.push_back(condBr->getFalseTarget());
        }
    }
    return succs;
}

std::vector<IRBasicBlock*> getPredecessors(const IRBasicBlock& bb, const IRFunction& fn) {
    std::vector<IRBasicBlock*> preds;
    for (const auto& other : fn.getBlocks()) {
        auto succs = getSuccessors(*other);
        for (auto* s : succs) {
            if (s == &bb) {
                preds.push_back(other.get());
                break;
            }
        }
    }
    return preds;
}

DominatorTree::DominatorTree(const IRFunction& fn) {
    if (fn.getBlocks().empty()) return;
    computeRPO(fn);
    computeDominators(fn);
    computeFrontiers(fn);
}

void DominatorTree::computeRPO(const IRFunction& fn) {
    m_rpoBlocks.clear();
    m_rpoIndex.clear();

    const auto* entry = fn.getEntryBlock();
    if (!entry) entry = fn.getBlocks().front().get();

    std::unordered_set<const IRBasicBlock*> visited;
    std::vector<IRBasicBlock*> postOrder;

    std::function<void(IRBasicBlock*)> dfs = [&](IRBasicBlock* bb) {
        visited.insert(bb);
        for (auto* succ : getSuccessors(*bb)) {
            if (!visited.count(succ)) {
                dfs(succ);
            }
        }
        postOrder.push_back(bb);
    };

    dfs(const_cast<IRBasicBlock*>(entry));

    std::reverse(postOrder.begin(), postOrder.end());
    m_rpoBlocks = std::move(postOrder);

    for (int i = 0; i < static_cast<int>(m_rpoBlocks.size()); ++i) {
        m_rpoIndex[m_rpoBlocks[i]] = i;
    }
}

IRBasicBlock* DominatorTree::intersect(IRBasicBlock* b1, IRBasicBlock* b2) const {
    auto finger1 = b1;
    auto finger2 = b2;

    while (finger1 != finger2) {
        while (m_rpoIndex.at(finger1) > m_rpoIndex.at(finger2)) {
            auto it = m_idom.find(finger1);
            if (it == m_idom.end() || !it->second) break;
            finger1 = it->second;
        }
        while (m_rpoIndex.at(finger2) > m_rpoIndex.at(finger1)) {
            auto it = m_idom.find(finger2);
            if (it == m_idom.end() || !it->second) break;
            finger2 = it->second;
        }
    }
    return finger1;
}

void DominatorTree::computeDominators(const IRFunction& fn) {
    if (m_rpoBlocks.empty()) return;

    auto* root = m_rpoBlocks[0];
    m_idom[root] = root;

    bool changed = true;
    while (changed) {
        changed = false;

        for (size_t i = 1; i < m_rpoBlocks.size(); ++i) {
            auto* b = m_rpoBlocks[i];
            auto preds = getPredecessors(*b, fn);

            IRBasicBlock* newIdom = nullptr;
            for (auto* p : preds) {
                if (m_idom.count(p)) {
                    newIdom = p;
                    break;
                }
            }

            if (!newIdom) continue;

            for (auto* p : preds) {
                if (p != newIdom && m_idom.count(p)) {
                    newIdom = intersect(p, newIdom);
                }
            }

            if (!m_idom.count(b) || m_idom[b] != newIdom) {
                m_idom[b] = newIdom;
                changed = true;
            }
        }
    }

    // Build children tree
    for (auto* b : m_rpoBlocks) {
        if (b != root && m_idom.count(b)) {
            m_domChildren[m_idom[b]].push_back(b);
        }
    }
}

void DominatorTree::computeFrontiers(const IRFunction& fn) {
    for (auto* b : m_rpoBlocks) {
        auto preds = getPredecessors(*b, fn);
        if (preds.size() >= 2) {
            auto* bIdom = getIdom(b);
            for (auto* p : preds) {
                auto* runner = p;
                while (runner && runner != bIdom && m_rpoIndex.count(runner)) {
                    m_frontiers[runner].insert(b);
                    runner = getIdom(runner);
                }
            }
        }
    }
}

IRBasicBlock* DominatorTree::getIdom(const IRBasicBlock* bb) const {
    auto it = m_idom.find(bb);
    if (it != m_idom.end()) return it->second;
    return nullptr;
}

bool DominatorTree::dominates(const IRBasicBlock* a, const IRBasicBlock* b) const {
    if (!a || !b) return false;
    if (a == b) return true;
    const auto* curr = b;
    while (curr && curr != m_rpoBlocks[0]) {
        auto it = m_idom.find(curr);
        if (it == m_idom.end() || it->second == curr) break;
        curr = it->second;
        if (curr == a) return true;
    }
    return false;
}

bool DominatorTree::strictlyDominates(const IRBasicBlock* a, const IRBasicBlock* b) const {
    return (a != b) && dominates(a, b);
}

const std::unordered_set<IRBasicBlock*>& DominatorTree::getDominanceFrontier(const IRBasicBlock* bb) const {
    auto it = m_frontiers.find(bb);
    if (it != m_frontiers.end()) return it->second;
    return m_emptySet;
}

std::unordered_set<IRBasicBlock*> DominatorTree::computeIDF(
    const std::unordered_set<IRBasicBlock*>& defBlocks) const {
    std::unordered_set<IRBasicBlock*> idf;
    std::queue<IRBasicBlock*> worklist;

    for (auto* b : defBlocks) {
        worklist.push(b);
    }

    while (!worklist.empty()) {
        auto* x = worklist.front();
        worklist.pop();

        for (auto* y : getDominanceFrontier(x)) {
            if (!idf.count(y)) {
                idf.insert(y);
                worklist.push(y);
            }
        }
    }

    return idf;
}

const std::vector<IRBasicBlock*>& DominatorTree::getChildren(const IRBasicBlock* bb) const {
    auto it = m_domChildren.find(bb);
    if (it != m_domChildren.end()) return it->second;
    return m_emptyVec;
}

} // namespace sph

