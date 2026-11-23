#include "common/test_framework.hpp"
#include "compiler/ir/IRBasicBlock.hpp"
#include "compiler/ir/IRFunction.hpp"
#include "compiler/ir/IRInstruction.hpp"
#include "compiler/optimizer/EscapeAnalysisPass.hpp"
#include <memory>
#include <string>
#include <vector>

namespace sph {

namespace {

struct EscapeAnalysisExhaustiveRegistrar {
    EscapeAnalysisExhaustiveRegistrar() {
        registerNonEscapingDemotions();
        registerEscapingPointerPreservations();
    }

    void registerNonEscapingDemotions() {
        // 200 tests: Local short-lived sph_alloc demoted to AllocaInst and sph_free pruned
        for (int i = 1; i <= 200; ++i) {
            std::string name = "EscapeAnalysisExhaustive: Non-escaping sph_alloc demotion " + std::to_string(i);
            testing::TestRegistry::instance().registerTest(name, [i]() {
                IRFunction fn("fn_ea_demote_" + std::to_string(i), Type::getI32());

                auto bb = fn.createBlock("entry");
                int sizeBytes = (i % 32) * 8 + 8; // <= 256 bytes
                auto sz = ConstantInt::get(sizeBytes, Type::getI64());
                auto allocCall = std::make_shared<CallInst>(
                    "sph_alloc",
                    std::vector<std::shared_ptr<IRValue>>{sz},
                    PointerType::get(Type::getVoid()),
                    "ptr_" + std::to_string(i));
                bb->addInstruction(allocCall);

                auto intPtrType = PointerType::get(Type::getI32());
                auto cast = std::make_shared<CastInst>(allocCall, intPtrType, "castPtr");
                bb->addInstruction(cast);

                auto fortyTwo = ConstantInt::get(42, Type::getI32());
                bb->addInstruction(std::make_shared<StoreInst>(fortyTwo, cast));
                auto load = std::make_shared<LoadInst>(cast, "val");
                bb->addInstruction(load);

                auto freeCall = std::make_shared<CallInst>(
                    "sph_free",
                    std::vector<std::shared_ptr<IRValue>>{allocCall},
                    Type::getVoid(),
                    "freeRes");
                bb->addInstruction(freeCall);

                bb->addInstruction(std::make_shared<RetInst>(load));

                EscapeAnalysisPass pass;
                bool changed = pass.runOnFunction(fn);
                CHECK(changed);

                bool hasAllocCall = false;
                bool hasFreeCall = false;
                bool hasAlloca = false;
                for (const auto& inst : bb->getInstructions()) {
                    if (inst->getOpcode() == IROpcode::Call) {
                        const auto& call = static_cast<const CallInst&>(*inst);
                        if (call.getCallee() == "sph_alloc") hasAllocCall = true;
                        if (call.getCallee() == "sph_free") hasFreeCall = true;
                    }
                    if (inst->getOpcode() == IROpcode::Alloca) {
                        hasAlloca = true;
                    }
                }
                CHECK(!hasAllocCall);
                CHECK(!hasFreeCall);
                CHECK(hasAlloca);
            });
        }
    }

    void registerEscapingPointerPreservations() {
        // 200 tests: Escaping pointers (returned) must NOT be demoted
        for (int j = 1; j <= 200; ++j) {
            std::string name = "EscapeAnalysisExhaustive: Escaping pointer preserved " + std::to_string(j);
            testing::TestRegistry::instance().registerTest(name, [j]() {
                auto ptrType = PointerType::get(Type::getVoid());
                IRFunction fn("fn_ea_escape_" + std::to_string(j), ptrType);

                auto bb = fn.createBlock("entry");
                auto sz = ConstantInt::get(64, Type::getI64());
                auto allocCall = std::make_shared<CallInst>(
                    "sph_alloc",
                    std::vector<std::shared_ptr<IRValue>>{sz},
                    ptrType,
                    "escapingPtr");
                bb->addInstruction(allocCall);

                bb->addInstruction(std::make_shared<RetInst>(allocCall));

                EscapeAnalysisPass pass;
                bool changed = pass.runOnFunction(fn);
                CHECK(!changed);

                bool hasAllocCall = false;
                for (const auto& inst : bb->getInstructions()) {
                    if (inst->getOpcode() == IROpcode::Call) {
                        const auto& call = static_cast<const CallInst&>(*inst);
                        if (call.getCallee() == "sph_alloc") hasAllocCall = true;
                    }
                }
                CHECK(hasAllocCall);
            });
        }
    }
};

static EscapeAnalysisExhaustiveRegistrar s_eaRegistrar;

} // namespace

} // namespace sph
