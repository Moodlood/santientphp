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
        registerLoopScopedSafetyChecks();
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

    void registerLoopScopedSafetyChecks() {
        // Loop-local allocation freed on every path before latch is demoted to entry-block AllocaInst
        for (int k = 1; k <= 50; ++k) {
            std::string name = "EscapeAnalysisExhaustive: Loop-scoped iteration-local demotion " + std::to_string(k);
            testing::TestRegistry::instance().registerTest(name, [k]() {
                IRFunction fn("fn_ea_loop_local_" + std::to_string(k), Type::getI32());
                auto* bEntry = fn.createBlock("entry");
                auto* bHdr = fn.createBlock("loop.hdr");
                auto* bBody = fn.createBlock("loop.body");
                auto* bExit = fn.createBlock("loop.exit");

                bEntry->addInstruction(std::make_shared<BranchInst>(bHdr));
                auto cond = ConstantBool::get(true);
                bHdr->addInstruction(std::make_shared<CondBranchInst>(cond, bBody, bExit));

                auto sz = ConstantInt::get((k % 8 + 1) * 8, Type::getI64());
                auto allocCall = std::make_shared<CallInst>(
                    "sph_alloc",
                    std::vector<std::shared_ptr<IRValue>>{sz},
                    PointerType::get(Type::getI32()),
                    "loopPtr_" + std::to_string(k));
                bBody->addInstruction(allocCall);
                bBody->addInstruction(std::make_shared<StoreInst>(ConstantInt::get(k, Type::getI32()), allocCall));
                auto freeCall = std::make_shared<CallInst>(
                    "sph_free",
                    std::vector<std::shared_ptr<IRValue>>{allocCall},
                    Type::getVoid(),
                    "");
                bBody->addInstruction(freeCall);
                bBody->addInstruction(std::make_shared<BranchInst>(bHdr));

                bExit->addInstruction(std::make_shared<RetInst>(ConstantInt::get(0, Type::getI32())));

                EscapeAnalysisPass pass;
                CHECK(pass.runOnFunction(fn));

                bool hasAllocInEntry = false;
                for (const auto& inst : bEntry->getInstructions()) {
                    if (inst->getOpcode() == IROpcode::Alloca) hasAllocInEntry = true;
                }
                CHECK(hasAllocInEntry);
                for (const auto& inst : bBody->getInstructions()) {
                    if (inst->getOpcode() == IROpcode::Call) {
                        const auto& c = static_cast<const CallInst&>(*inst);
                        CHECK(c.getCallee() != "sph_alloc");
                        CHECK(c.getCallee() != "sph_free");
                    }
                }
            });
        }

        // Loop allocation WITHOUT dominating sph_free before latch must NOT be demoted
        testing::TestRegistry::instance().registerTest(
            "EscapeAnalysisExhaustive: Loop allocation missing sph_free preserved", []() {
                IRFunction fn("fn_ea_loop_missing_free", Type::getI32());
                auto* bEntry = fn.createBlock("entry");
                auto* bHdr = fn.createBlock("loop.hdr");
                auto* bBody = fn.createBlock("loop.body");
                auto* bExit = fn.createBlock("loop.exit");

                bEntry->addInstruction(std::make_shared<BranchInst>(bHdr));
                bHdr->addInstruction(std::make_shared<CondBranchInst>(ConstantBool::get(true), bBody, bExit));

                auto allocCall = std::make_shared<CallInst>(
                    "sph_alloc",
                    std::vector<std::shared_ptr<IRValue>>{ConstantInt::get(16, Type::getI64())},
                    PointerType::get(Type::getI32()),
                    "unfreed");
                bBody->addInstruction(allocCall);
                bBody->addInstruction(std::make_shared<StoreInst>(ConstantInt::get(7, Type::getI32()), allocCall));
                bBody->addInstruction(std::make_shared<BranchInst>(bHdr));
                bExit->addInstruction(std::make_shared<RetInst>(ConstantInt::get(0, Type::getI32())));

                EscapeAnalysisPass pass;
                CHECK(!pass.runOnFunction(fn));
            });

        // Loop allocation crossing backedge via PhiInst must NOT be demoted
        testing::TestRegistry::instance().registerTest(
            "EscapeAnalysisExhaustive: Loop allocation crossing backedge Phi preserved", []() {
                auto ptrType = PointerType::get(Type::getI32());
                IRFunction fn("fn_ea_loop_phi_escape", Type::getI32());
                auto* bEntry = fn.createBlock("entry");
                auto* bHdr = fn.createBlock("loop.hdr");
                auto* bBody = fn.createBlock("loop.body");
                auto* bExit = fn.createBlock("loop.exit");

                auto initAlloc = std::make_shared<CallInst>(
                    "sph_alloc",
                    std::vector<std::shared_ptr<IRValue>>{ConstantInt::get(8, Type::getI64())},
                    ptrType,
                    "initPtr");
                bEntry->addInstruction(initAlloc);
                bEntry->addInstruction(std::make_shared<BranchInst>(bHdr));

                auto phi = std::make_shared<PhiInst>(ptrType, "carried");
                bHdr->addInstruction(phi);
                bHdr->addInstruction(std::make_shared<CondBranchInst>(ConstantBool::get(true), bBody, bExit));

                auto iterAlloc = std::make_shared<CallInst>(
                    "sph_alloc",
                    std::vector<std::shared_ptr<IRValue>>{ConstantInt::get(8, Type::getI64())},
                    ptrType,
                    "iterPtr");
                bBody->addInstruction(iterAlloc);
                bBody->addInstruction(std::make_shared<CallInst>(
                    "sph_free", std::vector<std::shared_ptr<IRValue>>{phi}, Type::getVoid(), ""));
                bBody->addInstruction(std::make_shared<BranchInst>(bHdr));

                phi->addIncoming(bEntry, initAlloc);
                phi->addIncoming(bBody, iterAlloc);

                bExit->addInstruction(std::make_shared<CallInst>(
                    "sph_free", std::vector<std::shared_ptr<IRValue>>{phi}, Type::getVoid(), ""));
                bExit->addInstruction(std::make_shared<RetInst>(ConstantInt::get(0, Type::getI32())));

                EscapeAnalysisPass pass;
                pass.runOnFunction(fn);

                // iterAlloc crosses backedge via phi so it MUST remain a heap sph_alloc
                bool iterStillHeap = false;
                for (const auto& inst : bBody->getInstructions()) {
                    if (inst.get() == iterAlloc.get()) iterStillHeap = true;
                }
                CHECK(iterStillHeap);
            });

        // Pointer cast to integer escapes and must NOT be demoted
        testing::TestRegistry::instance().registerTest(
            "EscapeAnalysisExhaustive: Ptr-to-int cast escape preserved", []() {
                IRFunction fn("fn_ea_ptrtoint", Type::getI64());
                auto* bEntry = fn.createBlock("entry");
                auto allocCall = std::make_shared<CallInst>(
                    "sph_alloc",
                    std::vector<std::shared_ptr<IRValue>>{ConstantInt::get(16, Type::getI64())},
                    PointerType::get(Type::getI32()),
                    "raw");
                bEntry->addInstruction(allocCall);
                auto asInt = std::make_shared<CastInst>(allocCall, Type::getI64(), "addr");
                bEntry->addInstruction(asInt);
                bEntry->addInstruction(std::make_shared<CallInst>(
                    "sph_free", std::vector<std::shared_ptr<IRValue>>{allocCall}, Type::getVoid(), ""));
                bEntry->addInstruction(std::make_shared<RetInst>(asInt));

                EscapeAnalysisPass pass;
                CHECK(!pass.runOnFunction(fn));
            });
    }
};

static EscapeAnalysisExhaustiveRegistrar s_eaRegistrar;

} // namespace

} // namespace sph
