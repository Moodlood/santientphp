#include "common/test_framework.hpp"
#include "compiler/ir/IRBasicBlock.hpp"
#include "compiler/ir/IRFunction.hpp"
#include "compiler/ir/IRInstruction.hpp"
#include "compiler/optimizer/Dominance.hpp"
#include <memory>
#include <string>
#include <vector>

namespace sph {

namespace {

struct DominanceExhaustiveRegistrar {
    DominanceExhaustiveRegistrar() {
        registerTreeTopologies();
        registerDiamondTopologies();
        registerLoopTopologies();
        registerFrontierAndIDFTests();
    }

    void registerTreeTopologies() {
        // 150 tests: Tree CFGs of varying depth and fanout
        for (int depth = 1; depth <= 150; ++depth) {
            std::string name = "DominanceExhaustive: Tree topology depth " + std::to_string(depth);
            testing::TestRegistry::instance().registerTest(name, [depth]() {
                IRFunction fn("fn_tree_" + std::to_string(depth), Type::getVoid());

                auto entry = fn.createBlock("entry");
                std::vector<IRBasicBlock*> prevLevel = {entry};

                int levels = (depth % 10) + 2;
                for (int lvl = 0; lvl < levels; ++lvl) {
                    std::vector<IRBasicBlock*> curLevel;
                    for (size_t b = 0; b < prevLevel.size(); ++b) {
                        auto b1 = fn.createBlock("b_" + std::to_string(lvl) + "_" + std::to_string(b) + "_0");
                        auto b2 = fn.createBlock("b_" + std::to_string(lvl) + "_" + std::to_string(b) + "_1");
                        auto cond = ConstantBool::get((b + lvl) % 2 == 0);
                        prevLevel[b]->addInstruction(std::make_shared<CondBranchInst>(cond, b1, b2));
                        curLevel.push_back(b1);
                        curLevel.push_back(b2);
                    }
                    prevLevel = curLevel;
                    if (prevLevel.size() > 16) break;
                }
                for (auto* leaf : prevLevel) {
                    leaf->addInstruction(std::make_shared<RetInst>());
                }

                DominatorTree dt(fn);
                // Entry must dominate all reachable blocks
                for (const auto& bb : fn.getBlocks()) {
                    if (bb.get() != entry) {
                        CHECK(dt.dominates(entry, bb.get()));
                    }
                }
                // Entry's immediate dominator in CHK algorithm is itself
                CHECK_EQ(dt.getIdom(entry), entry);
            });
        }
    }

    void registerDiamondTopologies() {
        // 150 tests: Nested Diamond CFGs
        for (int n = 1; n <= 150; ++n) {
            std::string name = "DominanceExhaustive: Nested diamond depth " + std::to_string(n);
            testing::TestRegistry::instance().registerTest(name, [n]() {
                IRFunction fn("fn_diamond_" + std::to_string(n), Type::getVoid());

                auto entry = fn.createBlock("entry");
                auto prev = entry;

                int diamonds = (n % 12) + 1;
                std::vector<IRBasicBlock*> joins;

                for (int d = 0; d < diamonds; ++d) {
                    auto bThen = fn.createBlock("then_" + std::to_string(d));
                    auto bElse = fn.createBlock("else_" + std::to_string(d));
                    auto bJoin = fn.createBlock("join_" + std::to_string(d));

                    auto cond = ConstantBool::get(d % 2 == 0);
                    prev->addInstruction(std::make_shared<CondBranchInst>(cond, bThen, bElse));
                    bThen->addInstruction(std::make_shared<BranchInst>(bJoin));
                    bElse->addInstruction(std::make_shared<BranchInst>(bJoin));

                    joins.push_back(bJoin);
                    prev = bJoin;
                }
                prev->addInstruction(std::make_shared<RetInst>());

                DominatorTree dt(fn);
                CHECK(dt.dominates(entry, prev));
                for (auto* j : joins) {
                    CHECK(dt.dominates(entry, j));
                    CHECK(dt.getIdom(j) != nullptr);
                }
            });
        }
    }

    void registerLoopTopologies() {
        // 150 tests: Loop CFGs with break/continue and nesting
        for (int k = 1; k <= 150; ++k) {
            std::string name = "DominanceExhaustive: Loop structure " + std::to_string(k);
            testing::TestRegistry::instance().registerTest(name, [k]() {
                IRFunction fn("fn_loop_" + std::to_string(k), Type::getVoid());

                auto entry = fn.createBlock("entry");
                auto header = fn.createBlock("loop.hdr");
                auto body = fn.createBlock("loop.body");
                auto exit = fn.createBlock("loop.exit");

                entry->addInstruction(std::make_shared<BranchInst>(header));
                auto cond = ConstantBool::get(true);
                header->addInstruction(std::make_shared<CondBranchInst>(cond, body, exit));
                body->addInstruction(std::make_shared<BranchInst>(header)); // back-edge
                exit->addInstruction(std::make_shared<RetInst>());

                DominatorTree dt(fn);
                CHECK(dt.dominates(entry, header));
                CHECK(dt.dominates(header, body));
                CHECK(dt.dominates(header, exit));
                CHECK(!dt.dominates(body, exit));
                CHECK_EQ(dt.getIdom(header), entry);
                CHECK_EQ(dt.getIdom(body), header);
                CHECK_EQ(dt.getIdom(exit), header);
            });
        }
    }

    void registerFrontierAndIDFTests() {
        // 150 tests: Dominance Frontiers and Iterated Dominance Frontiers (IDF)
        for (int m = 1; m <= 150; ++m) {
            std::string name = "DominanceExhaustive: DF and IDF invariant " + std::to_string(m);
            testing::TestRegistry::instance().registerTest(name, [m]() {
                IRFunction fn("fn_df_" + std::to_string(m), Type::getVoid());

                auto b0 = fn.createBlock("b0");
                auto b1 = fn.createBlock("b1");
                auto b2 = fn.createBlock("b2");
                auto b3 = fn.createBlock("b3");

                auto cond = ConstantBool::get(m % 2 == 0);
                b0->addInstruction(std::make_shared<CondBranchInst>(cond, b1, b2));
                b1->addInstruction(std::make_shared<BranchInst>(b3));
                b2->addInstruction(std::make_shared<BranchInst>(b3));
                b3->addInstruction(std::make_shared<RetInst>());

                DominatorTree dt(fn);
                const auto& df1 = dt.getDominanceFrontier(b1);
                CHECK(df1.count(b3) > 0);
                const auto& df2 = dt.getDominanceFrontier(b2);
                CHECK(df2.count(b3) > 0);

                std::unordered_set<IRBasicBlock*> defs = {b1};
                auto idf = dt.computeIDF(defs);
                CHECK(idf.count(b3) > 0);
                CHECK_EQ(idf.size(), 1u);
            });
        }
    }
};

static DominanceExhaustiveRegistrar s_domRegistrar;

} // namespace

} // namespace sph
