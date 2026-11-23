#include "common/test_framework.hpp"
#include "compiler/ir/IRBasicBlock.hpp"
#include "compiler/ir/IRFunction.hpp"
#include "compiler/ir/IRInstruction.hpp"
#include "compiler/optimizer/TailCallPass.hpp"
#include <memory>
#include <string>
#include <vector>

namespace sph {

namespace {

struct TcoExhaustiveRegistrar {
    TcoExhaustiveRegistrar() {
        registerVoidTailCalls();
        registerValueTailCalls();
        registerNonTailCalls();
        registerInternalCallingConventionTests();
    }

    void registerVoidTailCalls() {
        // 125 tests: Call followed directly by RetInst in void functions marked isTailCall
        for (int i = 1; i <= 125; ++i) {
            std::string name = "TcoExhaustive: Void return tail call detection " + std::to_string(i);
            testing::TestRegistry::instance().registerTest(name, [i]() {
                IRFunction fn("fn_tail_void_" + std::to_string(i), Type::getVoid());

                auto bb = fn.createBlock("entry");
                auto arg = ConstantInt::get(i, Type::getI32());
                auto call = std::make_shared<CallInst>("helper", std::vector<std::shared_ptr<IRValue>>{arg}, Type::getVoid(), "callRes");
                bb->addInstruction(call);
                bb->addInstruction(std::make_shared<RetInst>());

                TailCallPass pass;
                bool changed = pass.runOnFunction(fn);
                CHECK(changed);
                CHECK(call->isTailCall());
            });
        }
    }

    void registerValueTailCalls() {
        // 125 tests: Return value directly fed from CallInst marked isTailCall
        for (int j = 1; j <= 125; ++j) {
            std::string name = "TcoExhaustive: Value return tail call detection " + std::to_string(j);
            testing::TestRegistry::instance().registerTest(name, [j]() {
                IRFunction fn("fn_tail_val_" + std::to_string(j), Type::getI32());

                auto bb = fn.createBlock("entry");
                auto arg = ConstantInt::get(j * 3, Type::getI32());
                auto call = std::make_shared<CallInst>("compute", std::vector<std::shared_ptr<IRValue>>{arg}, Type::getI32(), "callVal");
                bb->addInstruction(call);
                bb->addInstruction(std::make_shared<RetInst>(call));

                TailCallPass pass;
                bool changed = pass.runOnFunction(fn);
                CHECK(changed);
                CHECK(call->isTailCall());
            });
        }
    }

    void registerNonTailCalls() {
        // 125 tests: Computation after CallInst must NOT be marked isTailCall
        for (int k = 1; k <= 125; ++k) {
            std::string name = "TcoExhaustive: Non-tail call preserved " + std::to_string(k);
            testing::TestRegistry::instance().registerTest(name, [k]() {
                IRFunction fn("fn_nontail_" + std::to_string(k), Type::getI32());

                auto bb = fn.createBlock("entry");
                auto arg = ConstantInt::get(k, Type::getI32());
                auto call = std::make_shared<CallInst>("calc", std::vector<std::shared_ptr<IRValue>>{arg}, Type::getI32(), "cVal");
                bb->addInstruction(call);

                // Arithmetic after call: 1 + call(...) -> NOT a tail call!
                auto one = ConstantInt::get(1, Type::getI32());
                auto add = std::make_shared<BinaryInst>(IROpcode::Add, call, one, Type::getI32(), "sum");
                bb->addInstruction(add);
                bb->addInstruction(std::make_shared<RetInst>(add));

                TailCallPass pass;
                bool changed = pass.runOnFunction(fn);
                CHECK(!changed);
                CHECK(!call->isTailCall());
            });
        }
    }

    void registerInternalCallingConventionTests() {
        // 125 tests: Internal functions marked isInternal for Win64 shadow space optimization
        for (int m = 1; m <= 125; ++m) {
            std::string name = "TcoExhaustive: Internal calling convention " + std::to_string(m);
            testing::TestRegistry::instance().registerTest(name, [m]() {
                std::vector<std::shared_ptr<IRParameter>> params;
                for (size_t p = 0; p < 6; ++p) {
                    params.push_back(std::make_shared<IRParameter>(Type::getI32(), "arg" + std::to_string(p), p));
                }
                IRFunction fn("fn_internal_" + std::to_string(m), Type::getI32(), params);

                CHECK(!fn.isInternal());
                fn.setInternal(true);
                CHECK(fn.isInternal());

                auto bb = fn.createBlock("entry");
                auto p0 = fn.getParameters()[0];
                bb->addInstruction(std::make_shared<RetInst>(p0));

                CHECK_EQ(fn.getParameters().size(), 6u);
            });
        }
    }
};

static TcoExhaustiveRegistrar s_tcoRegistrar;

} // namespace

} // namespace sph
