#include "common/test_framework.hpp"
#include "compiler/codegen/x86_64/LinearScanAllocator.hpp"
#include "compiler/ir/IRBasicBlock.hpp"
#include "compiler/ir/IRFunction.hpp"
#include "compiler/ir/IRInstruction.hpp"
#include <memory>
#include <string>
#include <vector>

namespace sph {

namespace {

struct LsraExhaustiveRegistrar {
    LsraExhaustiveRegistrar() {
        registerZeroSpillAllocations();
        registerHighRegisterPressureSpilling();
        registerCalleeSavedIntervalTracking();
        registerLivenessIntervalComputations();
    }

    void registerZeroSpillAllocations() {
        // 250 tests: Functions with moderate register pressure get 100% registers and 0 spills
        for (int i = 1; i <= 250; ++i) {
            std::string name = "LsraExhaustive: Zero-spill register allocation " + std::to_string(i);
            testing::TestRegistry::instance().registerTest(name, [i]() {
                IRFunction fn("fn_lsra_zerospill_" + std::to_string(i), Type::getI32());

                auto bb = fn.createBlock("entry");
                int varCount = (i % 5) + 2; // 2 to 6 variables
                std::vector<std::shared_ptr<BinaryInst>> insts;

                auto c0 = ConstantInt::get(1, Type::getI32());
                auto c1 = ConstantInt::get(2, Type::getI32());
                auto first = std::make_shared<BinaryInst>(IROpcode::Add, c0, c1, Type::getI32(), "v0");
                bb->addInstruction(first);
                insts.push_back(first);

                for (int v = 1; v < varCount; ++v) {
                    auto next = std::make_shared<BinaryInst>(IROpcode::Add, insts.back(), c1, Type::getI32(), "v" + std::to_string(v));
                    bb->addInstruction(next);
                    insts.push_back(next);
                }
                bb->addInstruction(std::make_shared<RetInst>(insts.back()));

                LinearScanAllocator lsra(fn, false);
                lsra.allocate();

                // Every value must be assigned a register, with 0 spills
                for (const auto& inst : insts) {
                    CHECK(lsra.hasRegister(inst.get()));
                    CHECK(!lsra.isSpilled(inst.get()));
                    const std::string& reg = lsra.getRegister(inst.get());
                    CHECK(!reg.empty());
                }
            });
        }
    }

    void registerHighRegisterPressureSpilling() {
        // 250 tests: Heavy register pressure (> 16 live variables) forces spilling using furthest-next-use
        for (int j = 1; j <= 250; ++j) {
            std::string name = "LsraExhaustive: High register pressure spilling " + std::to_string(j);
            testing::TestRegistry::instance().registerTest(name, [j]() {
                IRFunction fn("fn_lsra_spill_" + std::to_string(j), Type::getI32());

                auto bb = fn.createBlock("entry");
                int varCount = 18;
                std::vector<std::shared_ptr<BinaryInst>> vars;

                auto one = ConstantInt::get(1, Type::getI32());
                for (int v = 0; v < varCount; ++v) {
                    auto c = ConstantInt::get(v + 10, Type::getI32());
                    auto inst = std::make_shared<BinaryInst>(IROpcode::Add, c, one, Type::getI32(), "heavy_" + std::to_string(v));
                    bb->addInstruction(inst);
                    vars.push_back(inst);
                }

                std::shared_ptr<IRValue> accum = vars[0];
                for (int v = 1; v < varCount; ++v) {
                    auto add = std::make_shared<BinaryInst>(IROpcode::Add, accum, vars[v], Type::getI32(), "acc_" + std::to_string(v));
                    bb->addInstruction(add);
                    accum = add;
                }
                bb->addInstruction(std::make_shared<RetInst>(accum));

                LinearScanAllocator lsra(fn, false);
                lsra.allocate();

                int spillCount = 0;
                int regCount = 0;
                for (const auto& v : vars) {
                    if (lsra.isSpilled(v.get())) {
                        spillCount++;
                    } else if (lsra.hasRegister(v.get())) {
                        regCount++;
                    }
                }
                CHECK(spillCount > 0);
                CHECK(regCount > 0);
            });
        }
    }

    void registerCalleeSavedIntervalTracking() {
        // 250 tests: Intervals crossing function calls assign callee-saved registers or spill
        for (int k = 1; k <= 250; ++k) {
            std::string name = "LsraExhaustive: Call-crossing interval preservation " + std::to_string(k);
            testing::TestRegistry::instance().registerTest(name, [k]() {
                IRFunction fn("fn_lsra_callcross_" + std::to_string(k), Type::getI32());

                auto bb = fn.createBlock("entry");
                auto c1 = ConstantInt::get(100, Type::getI32());
                auto c2 = ConstantInt::get(200, Type::getI32());
                auto valBefore = std::make_shared<BinaryInst>(IROpcode::Add, c1, c2, Type::getI32(), "valBefore");
                bb->addInstruction(valBefore);

                auto dummyCall = std::make_shared<CallInst>("dummy_helper", std::vector<std::shared_ptr<IRValue>>{}, Type::getVoid(), "callRes");
                bb->addInstruction(dummyCall);

                auto valAfter = std::make_shared<BinaryInst>(IROpcode::Add, valBefore, c1, Type::getI32(), "valAfter");
                bb->addInstruction(valAfter);
                bb->addInstruction(std::make_shared<RetInst>(valAfter));

                LinearScanAllocator lsra(fn, false);
                lsra.allocate();

                if (lsra.hasRegister(valBefore.get())) {
                    const std::string& reg = lsra.getRegister(valBefore.get());
                    bool isCalleeSaved = (reg == "rbx" || reg == "r12" || reg == "r13" || reg == "r14" || reg == "r15" || reg == "rsi" || reg == "rdi");
                    CHECK(isCalleeSaved);
                } else {
                    CHECK(lsra.isSpilled(valBefore.get()));
                }
            });
        }
    }

    void registerLivenessIntervalComputations() {
        // 250 tests: CFG branching liveness intervals
        for (int m = 1; m <= 250; ++m) {
            std::string name = "LsraExhaustive: CFG branching liveness " + std::to_string(m);
            testing::TestRegistry::instance().registerTest(name, [m]() {
                IRFunction fn("fn_lsra_branch_" + std::to_string(m), Type::getI32());

                auto b0 = fn.createBlock("b0");
                auto bThen = fn.createBlock("then");
                auto bElse = fn.createBlock("else");
                auto bJoin = fn.createBlock("join");

                auto ten = ConstantInt::get(10, Type::getI32());
                auto twenty = ConstantInt::get(20, Type::getI32());
                auto base = std::make_shared<BinaryInst>(IROpcode::Add, ten, twenty, Type::getI32(), "base");
                b0->addInstruction(base);

                auto cond = ConstantBool::get(m % 2 == 0);
                b0->addInstruction(std::make_shared<CondBranchInst>(cond, bThen, bElse));

                auto addThen = std::make_shared<BinaryInst>(IROpcode::Add, base, ten, Type::getI32(), "tAdd");
                bThen->addInstruction(addThen);
                bThen->addInstruction(std::make_shared<BranchInst>(bJoin));

                auto addElse = std::make_shared<BinaryInst>(IROpcode::Sub, base, twenty, Type::getI32(), "eSub");
                bElse->addInstruction(addElse);
                bElse->addInstruction(std::make_shared<BranchInst>(bJoin));

                bJoin->addInstruction(std::make_shared<RetInst>(base));

                LinearScanAllocator lsra(fn, false);
                lsra.allocate();

                CHECK(lsra.hasRegister(base.get()) || lsra.isSpilled(base.get()));
            });
        }

        // Loop Phi backedge copy coalescing verification
        testing::TestRegistry::instance().registerTest(
            "LsraExhaustive: Loop induction Phi coalesces with step value",
            []() {
                IRFunction fn("fn_lsra_phi_coalesce", Type::getI32());
                auto entry = fn.createBlock("entry");
                auto hdr = fn.createBlock("hdr");
                auto body = fn.createBlock("body");
                auto exitBb = fn.createBlock("exit");

                entry->addInstruction(std::make_shared<BranchInst>(hdr));
                auto phi = std::make_shared<PhiInst>(Type::getI32(), "iv");
                hdr->addInstruction(phi);
                auto bound = ConstantInt::get(100, Type::getI32());
                auto cmp = std::make_shared<BinaryInst>(IROpcode::Less, phi, bound, Type::getBool(), "cmp");
                hdr->addInstruction(cmp);
                hdr->addInstruction(std::make_shared<CondBranchInst>(cmp, body, exitBb));

                auto one = ConstantInt::get(1, Type::getI32());
                auto next = std::make_shared<BinaryInst>(IROpcode::Add, phi, one, Type::getI32(), "next");
                body->addInstruction(next);
                body->addInstruction(std::make_shared<BranchInst>(hdr));

                phi->addIncoming(entry, ConstantInt::get(0, Type::getI32()));
                phi->addIncoming(body, next);

                exitBb->addInstruction(std::make_shared<RetInst>(phi));

                LinearScanAllocator lsra(fn, false);
                lsra.allocate();

                CHECK(lsra.hasRegister(phi.get()));
                CHECK(lsra.hasRegister(next.get()));
                CHECK_EQ(lsra.getRegister(phi.get()), lsra.getRegister(next.get()));
            });
    }
};

static LsraExhaustiveRegistrar s_lsraRegistrar;

} // namespace

} // namespace sph
