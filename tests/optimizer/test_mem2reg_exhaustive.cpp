#include "common/test_framework.hpp"
#include "compiler/ir/IRBasicBlock.hpp"
#include "compiler/ir/IRFunction.hpp"
#include "compiler/ir/IRInstruction.hpp"
#include "compiler/optimizer/Mem2RegPass.hpp"
#include <memory>
#include <string>
#include <vector>

namespace sph {

namespace {

struct Mem2RegExhaustiveRegistrar {
    Mem2RegExhaustiveRegistrar() {
        registerSingleBlockPromotions();
        registerDiamondPhiPromotions();
        registerLoopPhiPromotions();
        registerMultiVariablePromotions();
    }

    void registerSingleBlockPromotions() {
        // 250 tests: Single block stores and loads promoted to SSA registers
        for (int i = 1; i <= 250; ++i) {
            std::string name = "Mem2RegExhaustive: Single block alloca promotion " + std::to_string(i);
            testing::TestRegistry::instance().registerTest(name, [i]() {
                IRFunction fn("fn_sblock_" + std::to_string(i), Type::getI32());

                auto bb = fn.createBlock("entry");
                auto alloca = std::make_shared<AllocaInst>(Type::getI32(), "var_" + std::to_string(i));
                bb->addInstruction(alloca);

                int val = i * 7 + 3;
                auto constVal = ConstantInt::get(val, Type::getI32());
                auto store = std::make_shared<StoreInst>(constVal, alloca);
                bb->addInstruction(store);

                auto load = std::make_shared<LoadInst>(alloca, "loaded");
                bb->addInstruction(load);

                auto ret = std::make_shared<RetInst>(load);
                bb->addInstruction(ret);

                Mem2RegPass pass;
                bool changed = pass.runOnFunction(fn);
                CHECK(changed);

                bool hasAlloca = false;
                bool hasStore = false;
                bool hasLoad = false;
                for (const auto& inst : bb->getInstructions()) {
                    if (inst->getOpcode() == IROpcode::Alloca) hasAlloca = true;
                    if (inst->getOpcode() == IROpcode::Store) hasStore = true;
                    if (inst->getOpcode() == IROpcode::Load) hasLoad = true;
                }
                CHECK(!hasAlloca);
                CHECK(!hasStore);
                CHECK(!hasLoad);

                auto retInst = std::static_pointer_cast<RetInst>(bb->getInstructions().back());
                CHECK(retInst->hasValue());
                CHECK_EQ(retInst->getValue().get(), constVal.get());
            });
        }
    }

    void registerDiamondPhiPromotions() {
        // 250 tests: Branching diamonds requiring Phi nodes at join blocks
        for (int j = 1; j <= 250; ++j) {
            std::string name = "Mem2RegExhaustive: Diamond phi node placement " + std::to_string(j);
            testing::TestRegistry::instance().registerTest(name, [j]() {
                IRFunction fn("fn_diamond_phi_" + std::to_string(j), Type::getI32());

                auto b0 = fn.createBlock("entry");
                auto bThen = fn.createBlock("then");
                auto bElse = fn.createBlock("else");
                auto bJoin = fn.createBlock("join");

                auto alloca = std::make_shared<AllocaInst>(Type::getI32(), "x");
                b0->addInstruction(alloca);
                auto cond = ConstantBool::get(j % 2 == 0);
                b0->addInstruction(std::make_shared<CondBranchInst>(cond, bThen, bElse));

                auto val1 = ConstantInt::get(j * 10, Type::getI32());
                bThen->addInstruction(std::make_shared<StoreInst>(val1, alloca));
                bThen->addInstruction(std::make_shared<BranchInst>(bJoin));

                auto val2 = ConstantInt::get(j * 20, Type::getI32());
                bElse->addInstruction(std::make_shared<StoreInst>(val2, alloca));
                bElse->addInstruction(std::make_shared<BranchInst>(bJoin));

                auto load = std::make_shared<LoadInst>(alloca, "res");
                bJoin->addInstruction(load);
                bJoin->addInstruction(std::make_shared<RetInst>(load));

                Mem2RegPass pass;
                bool changed = pass.runOnFunction(fn);
                CHECK(changed);

                bool hasPhi = false;
                for (const auto& inst : bJoin->getInstructions()) {
                    if (inst->getOpcode() == IROpcode::Phi) {
                        hasPhi = true;
                        auto phi = std::static_pointer_cast<PhiInst>(inst);
                        CHECK_EQ(phi->getIncoming().size(), 2u);
                    }
                }
                CHECK(hasPhi);
            });
        }
    }

    void registerLoopPhiPromotions() {
        // 250 tests: Loop-carried variables promoted to Phi nodes at loop header
        for (int k = 1; k <= 250; ++k) {
            std::string name = "Mem2RegExhaustive: Loop phi node promotion " + std::to_string(k);
            testing::TestRegistry::instance().registerTest(name, [k]() {
                IRFunction fn("fn_loop_phi_" + std::to_string(k), Type::getI32());

                auto bEntry = fn.createBlock("entry");
                auto bHdr = fn.createBlock("loop.hdr");
                auto bBody = fn.createBlock("loop.body");
                auto bExit = fn.createBlock("loop.exit");

                auto alloca = std::make_shared<AllocaInst>(Type::getI32(), "counter");
                bEntry->addInstruction(alloca);
                auto initVal = ConstantInt::get(0, Type::getI32());
                bEntry->addInstruction(std::make_shared<StoreInst>(initVal, alloca));
                bEntry->addInstruction(std::make_shared<BranchInst>(bHdr));

                auto cond = ConstantBool::get(true);
                bHdr->addInstruction(std::make_shared<CondBranchInst>(cond, bBody, bExit));

                auto loadCur = std::make_shared<LoadInst>(alloca, "cur");
                bBody->addInstruction(loadCur);
                auto one = ConstantInt::get(1, Type::getI32());
                auto nextVal = std::make_shared<BinaryInst>(IROpcode::Add, loadCur, one, Type::getI32(), "next");
                bBody->addInstruction(nextVal);
                bBody->addInstruction(std::make_shared<StoreInst>(nextVal, alloca));
                bBody->addInstruction(std::make_shared<BranchInst>(bHdr));

                auto finalLoad = std::make_shared<LoadInst>(alloca, "final");
                bExit->addInstruction(finalLoad);
                bExit->addInstruction(std::make_shared<RetInst>(finalLoad));

                Mem2RegPass pass;
                bool changed = pass.runOnFunction(fn);
                CHECK(changed);

                bool hdrHasPhi = false;
                for (const auto& inst : bHdr->getInstructions()) {
                    if (inst->getOpcode() == IROpcode::Phi) {
                        hdrHasPhi = true;
                        auto phi = std::static_pointer_cast<PhiInst>(inst);
                        CHECK_EQ(phi->getIncoming().size(), 2u);
                    }
                }
                CHECK(hdrHasPhi);
            });
        }
    }

    void registerMultiVariablePromotions() {
        // 250 tests: Multiple simultaneous alloca promotions across complex basic blocks
        for (int m = 1; m <= 250; ++m) {
            std::string name = "Mem2RegExhaustive: Multiple allocas in flight " + std::to_string(m);
            testing::TestRegistry::instance().registerTest(name, [m]() {
                IRFunction fn("fn_multi_alloca_" + std::to_string(m), Type::getI32());

                auto bb = fn.createBlock("entry");
                int varCount = (m % 8) + 2;
                std::vector<std::shared_ptr<AllocaInst>> allocas;

                for (int v = 0; v < varCount; ++v) {
                    auto a = std::make_shared<AllocaInst>(Type::getI32(), "var_" + std::to_string(v));
                    bb->addInstruction(a);
                    allocas.push_back(a);
                    auto c = ConstantInt::get(v * 100 + m, Type::getI32());
                    bb->addInstruction(std::make_shared<StoreInst>(c, a));
                }

                std::shared_ptr<IRValue> lastLoad = nullptr;
                for (int v = 0; v < varCount; ++v) {
                    auto l = std::make_shared<LoadInst>(allocas[v], "ld_" + std::to_string(v));
                    bb->addInstruction(l);
                    lastLoad = l;
                }
                bb->addInstruction(std::make_shared<RetInst>(lastLoad));

                Mem2RegPass pass;
                bool changed = pass.runOnFunction(fn);
                CHECK(changed);

                for (const auto& inst : bb->getInstructions()) {
                    CHECK(inst->getOpcode() != IROpcode::Alloca);
                    CHECK(inst->getOpcode() != IROpcode::Load);
                    CHECK(inst->getOpcode() != IROpcode::Store);
                }
            });
        }
    }
};

static Mem2RegExhaustiveRegistrar s_mem2regRegistrar;

} // namespace

} // namespace sph
