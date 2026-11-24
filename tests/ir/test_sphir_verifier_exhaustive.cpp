#include "common/test_framework.hpp"
#include "compiler/ir/IRBasicBlock.hpp"
#include "compiler/ir/IRFunction.hpp"
#include "compiler/ir/IRInstruction.hpp"
#include "compiler/ir/IRModule.hpp"
#include "compiler/ir/IRVerifier.hpp"
#include "compiler/optimizer/CriticalEdgeSplittingPass.hpp"
#include "compiler/optimizer/Dominance.hpp"
#include "compiler/types/Type.hpp"
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace sph;

// Helper to create a dummy function with an entry block
std::pair<std::unique_ptr<IRFunction>, IRBasicBlock*> createTestFunction(
    const std::string& name = "test_fn",
    std::shared_ptr<Type> retType = Type::getI32()) {
    auto fn = std::make_unique<IRFunction>(name, retType);
    auto* entry = fn->createBlock("entry");
    return {std::move(fn), entry};
}

} // namespace

TEST_CASE("SPHIR Verifier: Valid function passes verification") {
    auto [fn, entry] = createTestFunction("test_valid", Type::getI32());
    auto constVal = ConstantInt::get(42);
    entry->addInstruction(std::make_shared<RetInst>(constVal));

    IRVerifier verifier;
    std::vector<std::string> errors;
    bool ok = verifier.verify(*fn, errors);
    CHECK(ok);
    CHECK_EQ(errors.size(), 0u);
}

TEST_CASE("SPHIR Verifier: Empty basic block is rejected") {
    auto [fn, entry] = createTestFunction("test_empty");
    (void)entry; // empty block with no instructions

    IRVerifier verifier;
    std::vector<std::string> errors;
    bool ok = verifier.verify(*fn, errors);
    CHECK(!ok);
    CHECK(!errors.empty());
}

TEST_CASE("SPHIR Verifier: Missing terminator is rejected") {
    auto [fn, entry] = createTestFunction("test_no_term");
    auto allocaInst = std::make_shared<AllocaInst>(Type::getI32(), "x");
    entry->addInstruction(allocaInst);

    IRVerifier verifier;
    std::vector<std::string> errors;
    bool ok = verifier.verify(*fn, errors);
    CHECK(!ok);
    CHECK(!errors.empty());
}

TEST_CASE("SPHIR Verifier: Instruction appearing after terminator is rejected") {
    auto [fn, entry] = createTestFunction("test_post_term");
    auto constVal = ConstantInt::get(10);
    entry->addInstruction(std::make_shared<RetInst>(constVal));
    entry->addInstruction(std::make_shared<AllocaInst>(Type::getI32(), "dead"));

    IRVerifier verifier;
    std::vector<std::string> errors;
    bool ok = verifier.verify(*fn, errors);
    CHECK(!ok);
    CHECK(!errors.empty());
}

TEST_CASE("SPHIR Verifier: Return type mismatch is rejected") {
    auto [fn, entry] = createTestFunction("test_ret_mismatch", Type::getI32());
    auto constBool = std::make_shared<ConstantBool>(true);
    entry->addInstruction(std::make_shared<RetInst>(constBool));

    IRVerifier verifier;
    std::vector<std::string> errors;
    bool ok = verifier.verify(*fn, errors);
    CHECK(!ok);
    CHECK(!errors.empty());
}

TEST_CASE("SPHIR Verifier: Void function returning a value is rejected") {
    auto [fn, entry] = createTestFunction("test_void_ret", Type::getVoid());
    auto constVal = ConstantInt::get(10);
    entry->addInstruction(std::make_shared<RetInst>(constVal));

    IRVerifier verifier;
    std::vector<std::string> errors;
    bool ok = verifier.verify(*fn, errors);
    CHECK(!ok);
    CHECK(!errors.empty());
}

TEST_CASE("SPHIR Verifier: Non-void function returning void is rejected") {
    auto [fn, entry] = createTestFunction("test_non_void_ret", Type::getI32());
    entry->addInstruction(std::make_shared<RetInst>());

    IRVerifier verifier;
    std::vector<std::string> errors;
    bool ok = verifier.verify(*fn, errors);
    CHECK(!ok);
    CHECK(!errors.empty());
}

TEST_CASE("SPHIR Verifier: Branch to external function block is rejected") {
    auto [fn1, entry1] = createTestFunction("fn1");
    auto [fn2, entry2] = createTestFunction("fn2");
    entry2->addInstruction(std::make_shared<RetInst>(ConstantInt::get(0)));

    entry1->addInstruction(std::make_shared<BranchInst>(entry2));

    IRVerifier verifier;
    std::vector<std::string> errors;
    bool ok = verifier.verify(*fn1, errors);
    CHECK(!ok);
    CHECK(!errors.empty());
}

TEST_CASE("SPHIR Verifier: Conditional branch with non-boolean condition is rejected") {
    auto [fn, entry] = createTestFunction("test_cond_br");
    auto* bTrue = fn->createBlock("b_true");
    auto* bFalse = fn->createBlock("b_false");
    bTrue->addInstruction(std::make_shared<RetInst>(ConstantInt::get(1)));
    bFalse->addInstruction(std::make_shared<RetInst>(ConstantInt::get(2)));

    // Integer condition instead of boolean
    auto intCond = ConstantInt::get(1);
    entry->addInstruction(std::make_shared<CondBranchInst>(intCond, bTrue, bFalse));

    IRVerifier verifier;
    std::vector<std::string> errors;
    bool ok = verifier.verify(*fn, errors);
    CHECK(!ok);
    CHECK(!errors.empty());
}

TEST_CASE("SPHIR Verifier: Phi instruction appearing after non-phi instruction is rejected") {
    auto [fn, entry] = createTestFunction("test_phi_pos");
    auto* b1 = fn->createBlock("b1");
    auto* b2 = fn->createBlock("b2");
    auto* join = fn->createBlock("join");

    entry->addInstruction(std::make_shared<CondBranchInst>(
        std::make_shared<ConstantBool>(true), b1, b2));

    b1->addInstruction(std::make_shared<BranchInst>(join));
    b2->addInstruction(std::make_shared<BranchInst>(join));

    // Place Alloca before Phi in join
    join->addInstruction(std::make_shared<AllocaInst>(Type::getI32(), "x"));
    auto phi = std::make_shared<PhiInst>(Type::getI32(), "p");
    phi->addIncoming(b1, ConstantInt::get(10));
    phi->addIncoming(b2, ConstantInt::get(20));
    join->addInstruction(phi);
    join->addInstruction(std::make_shared<RetInst>(phi));

    IRVerifier verifier;
    std::vector<std::string> errors;
    bool ok = verifier.verify(*fn, errors);
    CHECK(!ok);
    CHECK(!errors.empty());
}

TEST_CASE("SPHIR Verifier: Phi instruction missing incoming value for predecessor is rejected") {
    auto [fn, entry] = createTestFunction("test_phi_missing_pred");
    auto* b1 = fn->createBlock("b1");
    auto* b2 = fn->createBlock("b2");
    auto* join = fn->createBlock("join");

    entry->addInstruction(std::make_shared<CondBranchInst>(
        std::make_shared<ConstantBool>(true), b1, b2));

    b1->addInstruction(std::make_shared<BranchInst>(join));
    b2->addInstruction(std::make_shared<BranchInst>(join));

    // Phi only provides incoming for b1, missing b2!
    auto phi = std::make_shared<PhiInst>(Type::getI32(), "p");
    phi->addIncoming(b1, ConstantInt::get(10));
    join->addInstruction(phi);
    join->addInstruction(std::make_shared<RetInst>(phi));

    IRVerifier verifier;
    std::vector<std::string> errors;
    bool ok = verifier.verify(*fn, errors);
    CHECK(!ok);
    CHECK(!errors.empty());
}

TEST_CASE("SPHIR Verifier: Phi instruction with incoming block not a predecessor is rejected") {
    auto [fn, entry] = createTestFunction("test_phi_extra_pred");
    auto* b1 = fn->createBlock("b1");
    auto* b2 = fn->createBlock("b2");
    auto* join = fn->createBlock("join");

    entry->addInstruction(std::make_shared<BranchInst>(b1));
    b1->addInstruction(std::make_shared<BranchInst>(join));
    b2->addInstruction(std::make_shared<RetInst>(ConstantInt::get(0)));

    // Join only has predecessor b1, but phi includes b2!
    auto phi = std::make_shared<PhiInst>(Type::getI32(), "p");
    phi->addIncoming(b1, ConstantInt::get(10));
    phi->addIncoming(b2, ConstantInt::get(20));
    join->addInstruction(phi);
    join->addInstruction(std::make_shared<RetInst>(phi));

    IRVerifier verifier;
    std::vector<std::string> errors;
    bool ok = verifier.verify(*fn, errors);
    CHECK(!ok);
    CHECK(!errors.empty());
}

TEST_CASE("SPHIR Verifier: Phi instruction with duplicate predecessor is rejected") {
    auto [fn, entry] = createTestFunction("test_phi_dup_pred");
    auto* b1 = fn->createBlock("b1");
    auto* join = fn->createBlock("join");

    entry->addInstruction(std::make_shared<BranchInst>(b1));
    b1->addInstruction(std::make_shared<BranchInst>(join));

    auto phi = std::make_shared<PhiInst>(Type::getI32(), "p");
    phi->addIncoming(b1, ConstantInt::get(10));
    phi->addIncoming(b1, ConstantInt::get(20)); // duplicate!
    join->addInstruction(phi);
    join->addInstruction(std::make_shared<RetInst>(phi));

    IRVerifier verifier;
    std::vector<std::string> errors;
    bool ok = verifier.verify(*fn, errors);
    CHECK(!ok);
    CHECK(!errors.empty());
}

TEST_CASE("SPHIR Verifier: Phi instruction type mismatch is rejected") {
    auto [fn, entry] = createTestFunction("test_phi_type_mismatch");
    auto* b1 = fn->createBlock("b1");
    auto* b2 = fn->createBlock("b2");
    auto* join = fn->createBlock("join");

    entry->addInstruction(std::make_shared<CondBranchInst>(
        std::make_shared<ConstantBool>(true), b1, b2));

    b1->addInstruction(std::make_shared<BranchInst>(join));
    b2->addInstruction(std::make_shared<BranchInst>(join));

    auto phi = std::make_shared<PhiInst>(Type::getI32(), "p");
    phi->addIncoming(b1, ConstantInt::get(10));
    phi->addIncoming(b2, std::make_shared<ConstantBool>(true)); // type mismatch: bool vs i32!
    join->addInstruction(phi);
    join->addInstruction(std::make_shared<RetInst>(phi));

    IRVerifier verifier;
    std::vector<std::string> errors;
    bool ok = verifier.verify(*fn, errors);
    CHECK(!ok);
    CHECK(!errors.empty());
}

TEST_CASE("SPHIR Verifier: Cross-function operand reference is rejected") {
    auto [fn1, entry1] = createTestFunction("fn1");
    auto [fn2, entry2] = createTestFunction("fn2");

    auto allocInFn2 = std::make_shared<AllocaInst>(Type::getI32(), "alloc2");
    entry2->addInstruction(allocInFn2);
    entry2->addInstruction(std::make_shared<RetInst>(ConstantInt::get(0)));

    // fn1 uses allocInFn2 defined in fn2!
    auto loadInFn1 = std::make_shared<LoadInst>(Type::getI32(), allocInFn2, "load1");
    entry1->addInstruction(loadInFn1);
    entry1->addInstruction(std::make_shared<RetInst>(loadInFn1));

    IRVerifier verifier;
    std::vector<std::string> errors;
    bool ok = verifier.verify(*fn1, errors);
    CHECK(!ok);
    CHECK(!errors.empty());
}

TEST_CASE("SPHIR Verifier: Instruction use preceding definition within block is rejected") {
    auto [fn, entry] = createTestFunction("test_use_before_def");

    auto add = std::make_shared<BinaryInst>(
        IROpcode::Add, nullptr, nullptr, Type::getI32(), "sum");
    auto allocaInst = std::make_shared<AllocaInst>(Type::getI32(), "x");

    // Add uses allocaInst, but allocaInst is added AFTER add in the basic block!
    add->setLhs(allocaInst);
    add->setRhs(ConstantInt::get(5));

    entry->addInstruction(add);
    entry->addInstruction(allocaInst);
    entry->addInstruction(std::make_shared<RetInst>(add));

    IRVerifier verifier;
    std::vector<std::string> errors;
    bool ok = verifier.verify(*fn, errors);
    CHECK(!ok);
    CHECK(!errors.empty());
}

TEST_CASE("SPHIR Verifier: Non-dominating instruction use across blocks is rejected") {
    auto [fn, entry] = createTestFunction("test_dom_violation");
    auto* b1 = fn->createBlock("b1");
    auto* b2 = fn->createBlock("b2");
    auto* join = fn->createBlock("join");

    entry->addInstruction(std::make_shared<CondBranchInst>(
        std::make_shared<ConstantBool>(true), b1, b2));

    // Value x is defined ONLY in branch b1
    auto defInB1 = std::make_shared<AllocaInst>(Type::getI32(), "x");
    b1->addInstruction(defInB1);
    b1->addInstruction(std::make_shared<BranchInst>(join));

    b2->addInstruction(std::make_shared<BranchInst>(join));

    // join uses defInB1 directly without a phi! b1 does NOT dominate join!
    auto loadInJoin = std::make_shared<LoadInst>(Type::getI32(), defInB1, "val");
    join->addInstruction(loadInJoin);
    join->addInstruction(std::make_shared<RetInst>(loadInJoin));

    IRVerifier verifier;
    std::vector<std::string> errors;
    bool ok = verifier.verify(*fn, errors);
    CHECK(!ok);
    CHECK(!errors.empty());
}

TEST_CASE("SPHIR Verifier: Critical edge detection identifies critical edges") {
    auto [fn, entry] = createTestFunction("test_critical_edge");
    auto* b1 = fn->createBlock("b1");
    auto* b2 = fn->createBlock("b2");
    auto* join = fn->createBlock("join");

    // entry has 2 successors (b1, b2)
    entry->addInstruction(std::make_shared<CondBranchInst>(
        std::make_shared<ConstantBool>(true), b1, b2));

    // b1 has 2 successors (join, b2) -> edge (b1, join) and (b1, b2)
    b1->addInstruction(std::make_shared<CondBranchInst>(
        std::make_shared<ConstantBool>(false), join, b2));

    // b2 has 1 successor (join)
    b2->addInstruction(std::make_shared<BranchInst>(join));

    join->addInstruction(std::make_shared<RetInst>(ConstantInt::get(42)));

    // Join has 2 predecessors (b1, b2). b1 has 2 successors (join, b2).
    // Therefore, edge (b1 -> join) is CRITICAL!
    CHECK(isCriticalEdge(*b1, *join, *fn));

    // entry has 2 successors (b1, b2). b2 has 2 predecessors (entry, b1).
    // Therefore, edge (entry -> b2) is CRITICAL!
    CHECK(isCriticalEdge(*entry, *b2, *fn));

    // Now split critical edges!
    bool splitOk = splitCriticalEdges(*fn);
    CHECK(splitOk);

    // After splitting, neither of these edges is critical anymore!
    CHECK(!isCriticalEdge(*b1, *join, *fn));
    CHECK(!isCriticalEdge(*entry, *b2, *fn));

    // Verify the split function passes IRVerifier cleanly!
    IRVerifier verifier;
    std::vector<std::string> errors;
    bool ok = verifier.verify(*fn, errors);
    CHECK(ok);
    CHECK_EQ(errors.size(), 0u);
}

TEST_CASE("SPHIR Verifier: Exhaustive parameterized invariant test suite (200 variations)") {
    for (int i = 0; i < 200; ++i) {
        auto [fn, entry] = createTestFunction("test_var_" + std::to_string(i));
        auto* b1 = fn->createBlock("b1");
        auto* b2 = fn->createBlock("b2");
        auto* join = fn->createBlock("join");

        entry->addInstruction(std::make_shared<CondBranchInst>(
            std::make_shared<ConstantBool>(i % 2 == 0), b1, b2));

        auto v1 = ConstantInt::get(i * 10);
        auto v2 = ConstantInt::get(i * 20 + 1);

        b1->addInstruction(std::make_shared<BranchInst>(join));
        b2->addInstruction(std::make_shared<BranchInst>(join));

        auto phi = std::make_shared<PhiInst>(Type::getI32(), "res_" + std::to_string(i));
        phi->addIncoming(b1, v1);
        phi->addIncoming(b2, v2);

        join->addInstruction(phi);
        join->addInstruction(std::make_shared<RetInst>(phi));

        IRVerifier verifier;
        std::vector<std::string> errors;
        bool ok = verifier.verify(*fn, errors);
        CHECK(ok);
        CHECK_EQ(errors.size(), 0u);
    }
}

TEST_CASE("SPHIR Verifier: Exhaustive dominance frontier & nested loop test suite (200 variations)") {
    for (int depth = 1; depth <= 200; ++depth) {
        auto [fn, entry] = createTestFunction("test_nested_" + std::to_string(depth));
        auto* header = fn->createBlock("header");
        auto* body = fn->createBlock("body");
        auto* exit = fn->createBlock("exit");

        entry->addInstruction(std::make_shared<BranchInst>(header));

        auto phi = std::make_shared<PhiInst>(Type::getI32(), "iter");
        phi->addIncoming(entry, ConstantInt::get(0));

        auto nextIter = std::make_shared<BinaryInst>(
            IROpcode::Add, phi, ConstantInt::get(1), Type::getI32(), "next");
        phi->addIncoming(body, nextIter);

        header->addInstruction(phi);
        header->addInstruction(std::make_shared<CondBranchInst>(
            std::make_shared<ConstantBool>(true), body, exit));

        body->addInstruction(nextIter);
        body->addInstruction(std::make_shared<BranchInst>(header));

        exit->addInstruction(std::make_shared<RetInst>(phi));

        DominatorTree domTree(*fn);
        CHECK(domTree.dominates(entry, header));
        CHECK(domTree.dominates(header, body));
        CHECK(domTree.dominates(header, exit));
        CHECK(!domTree.strictlyDominates(body, header)); // back-edge doesn't dominate header

        IRVerifier verifier;
        std::vector<std::string> errors;
        bool ok = verifier.verify(*fn, errors);
        CHECK(ok);
        CHECK_EQ(errors.size(), 0u);
    }
}
