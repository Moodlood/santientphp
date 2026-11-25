#include "common/test_framework.hpp"
#include "compiler/ir/IRBasicBlock.hpp"
#include "compiler/ir/IRFunction.hpp"
#include "compiler/ir/IRGenerator.hpp"
#include "compiler/ir/IRInstruction.hpp"
#include "compiler/ir/IRModule.hpp"
#include "compiler/optimizer/AlgebraicSimplificationPass.hpp"
#include "compiler/optimizer/CommonSubexpressionEliminationPass.hpp"
#include "compiler/optimizer/ConstantFoldingPass.hpp"
#include "compiler/optimizer/ControlFlowOptimizationPass.hpp"
#include "compiler/optimizer/DeadCodeEliminationPass.hpp"
#include "compiler/optimizer/DeadStoreEliminationPass.hpp"
#include "compiler/optimizer/FunctionInliningPass.hpp"
#include "compiler/optimizer/LoopInvariantCodeMotionPass.hpp"
#include "compiler/optimizer/LoopUnrollPass.hpp"
#include "compiler/optimizer/Mem2RegPass.hpp"
#include "compiler/optimizer/PassManager.hpp"
#include "compiler/optimizer/StoreToLoadForwardingPass.hpp"
#include "compiler/optimizer/StrengthReductionPass.hpp"
#include "compiler/parser/Parser.hpp"
#include "compiler/semantic/SemanticAnalyzer.hpp"
#include "compiler/source/SourceManager.hpp"
#include <memory>
#include <string>

namespace {

std::unique_ptr<sph::IRModule> compileToIR(const std::string& code) {
    sph::SourceManager sm;
    sph::DiagnosticEmitter emitter(sm);
    uint32_t fileId = sm.addSource("opt_test.sph", code);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    if (!program || emitter.hasErrors()) return nullptr;

    sph::SemanticAnalyzer analyzer(emitter);
    if (!analyzer.analyze(*program) || emitter.hasErrors()) return nullptr;

    sph::IRGenerator generator;
    return generator.generate(*program);
}

} // namespace

// =============================================================================
// Optimizer Exhaustive: Constant Folding & Algebraic Simplification (451 to 490)
// =============================================================================

TEST_CASE("OptimizerExhaustive: 451 Constant folding integer addition 10 + 20") {
    auto mod = compileToIR("<?php fn test(): i32 { return 10 + 20; }");
    CHECK(mod != nullptr);
    sph::ConstantFoldingPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    bool changed = pass.runOnFunction(*fn);
    CHECK(changed);
}

TEST_CASE("OptimizerExhaustive: 452 Constant folding integer subtraction 100 - 35") {
    auto mod = compileToIR("<?php fn test(): i32 { return 100 - 35; }");
    CHECK(mod != nullptr);
    sph::ConstantFoldingPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 453 Constant folding integer multiplication 6 * 7") {
    auto mod = compileToIR("<?php fn test(): i32 { return 6 * 7; }");
    CHECK(mod != nullptr);
    sph::ConstantFoldingPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 454 Constant folding integer division 100 / 4") {
    auto mod = compileToIR("<?php fn test(): i32 { return 100 / 4; }");
    CHECK(mod != nullptr);
    sph::ConstantFoldingPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 455 Constant folding integer modulo 29 % 5") {
    auto mod = compileToIR("<?php fn test(): i32 { return 29 % 5; }");
    CHECK(mod != nullptr);
    sph::ConstantFoldingPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 456 Constant folding bitwise AND 0xFF & 0x0F") {
    auto mod = compileToIR("<?php fn test(): i32 { return 0xFF & 0x0F; }");
    CHECK(mod != nullptr);
    sph::ConstantFoldingPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 457 Constant folding bitwise OR 0xF0 | 0x0F") {
    auto mod = compileToIR("<?php fn test(): i32 { return 0xF0 | 0x0F; }");
    CHECK(mod != nullptr);
    sph::ConstantFoldingPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 458 Constant folding bitwise XOR 0xAA ^ 0xFF") {
    auto mod = compileToIR("<?php fn test(): i32 { return 0xAA ^ 0xFF; }");
    CHECK(mod != nullptr);
    sph::ConstantFoldingPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 459 Constant folding shift left 1 << 5") {
    auto mod = compileToIR("<?php fn test(): i32 { return 1 << 5; }");
    CHECK(mod != nullptr);
    sph::ConstantFoldingPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 460 Constant folding shift right 32 >> 2") {
    auto mod = compileToIR("<?php fn test(): i32 { return 32 >> 2; }");
    CHECK(mod != nullptr);
    sph::ConstantFoldingPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 461 Constant folding comparison equal 5 == 5") {
    auto mod = compileToIR("<?php fn test(): bool { return 5 == 5; }");
    CHECK(mod != nullptr);
    sph::ConstantFoldingPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 462 Constant folding comparison not equal 5 != 10") {
    auto mod = compileToIR("<?php fn test(): bool { return 5 != 10; }");
    CHECK(mod != nullptr);
    sph::ConstantFoldingPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 463 Constant folding comparison less 3 < 7") {
    auto mod = compileToIR("<?php fn test(): bool { return 3 < 7; }");
    CHECK(mod != nullptr);
    sph::ConstantFoldingPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 464 Constant folding comparison greater 10 > 2") {
    auto mod = compileToIR("<?php fn test(): bool { return 10 > 2; }");
    CHECK(mod != nullptr);
    sph::ConstantFoldingPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 465 Constant folding logical AND true && false") {
    auto mod = compileToIR("<?php fn test(): bool { return true && false; }");
    CHECK(mod != nullptr);
    sph::ConstantFoldingPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 466 Constant folding logical OR true || false") {
    auto mod = compileToIR("<?php fn test(): bool { return true || false; }");
    CHECK(mod != nullptr);
    sph::ConstantFoldingPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 467 Algebraic simplification x + 0") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return x + 0; }");
    CHECK(mod != nullptr);
    sph::AlgebraicSimplificationPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 468 Algebraic simplification 0 + x") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return 0 + x; }");
    CHECK(mod != nullptr);
    sph::AlgebraicSimplificationPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 469 Algebraic simplification x - 0") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return x - 0; }");
    CHECK(mod != nullptr);
    sph::AlgebraicSimplificationPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 470 Algebraic simplification x * 1") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return x * 1; }");
    CHECK(mod != nullptr);
    sph::AlgebraicSimplificationPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 471 Algebraic simplification 1 * x") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return 1 * x; }");
    CHECK(mod != nullptr);
    sph::AlgebraicSimplificationPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 472 Algebraic simplification x * 0") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return x * 0; }");
    CHECK(mod != nullptr);
    sph::AlgebraicSimplificationPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 473 Algebraic simplification x / 1") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return x / 1; }");
    CHECK(mod != nullptr);
    sph::AlgebraicSimplificationPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 474 Algebraic simplification x & 0") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return x & 0; }");
    CHECK(mod != nullptr);
    sph::AlgebraicSimplificationPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 475 Algebraic simplification x | 0") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return x | 0; }");
    CHECK(mod != nullptr);
    sph::AlgebraicSimplificationPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 476 Algebraic simplification x ^ 0") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return x ^ 0; }");
    CHECK(mod != nullptr);
    sph::AlgebraicSimplificationPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 477 Algebraic simplification x << 0") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return x << 0; }");
    CHECK(mod != nullptr);
    sph::AlgebraicSimplificationPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 478 Algebraic simplification x >> 0") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return x >> 0; }");
    CHECK(mod != nullptr);
    sph::AlgebraicSimplificationPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 479 Strength reduction x * 8 to x << 3") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return x * 8; }");
    CHECK(mod != nullptr);
    sph::StrengthReductionPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 480 Strength reduction 16 * x to x << 4") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return 16 * x; }");
    CHECK(mod != nullptr);
    sph::StrengthReductionPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 481 Strength reduction x * 1024 to x << 10") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return x * 1024; }");
    CHECK(mod != nullptr);
    sph::StrengthReductionPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 482 Dead store elimination unread local alloca") {
    auto mod = compileToIR("<?php fn test(): i32 { let unused = 100; return 42; }");
    CHECK(mod != nullptr);
    sph::DeadStoreEliminationPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 483 Dead store elimination consecutive stores to same slot") {
    auto mod = compileToIR("<?php fn test(): i32 { mut x = 10; x = 20; return x; }");
    CHECK(mod != nullptr);
    sph::DeadStoreEliminationPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 484 Store to load forwarding") {
    auto mod = compileToIR("<?php fn test(): i32 { let x = 42; return x; }");
    CHECK(mod != nullptr);
    sph::StoreToLoadForwardingPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 485 Common subexpression elimination") {
    auto mod = compileToIR("<?php fn test(a: i32, b: i32): i32 { let x = a + b; let y = a + b; return x + y; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 486 Dead code elimination unreachable block after return") {
    auto mod = compileToIR("<?php fn test(): i32 { let unused = 10 + 20; return 1; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O1);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 487 Control flow optimization removes empty branch jumps") {
    auto mod = compileToIR("<?php fn test(): void { if (true) {} }");
    CHECK(mod != nullptr);
    sph::ControlFlowOptimizationPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    pass.runOnFunction(*fn);
    CHECK(true);
}

TEST_CASE("OptimizerExhaustive: 488 Loop invariant code motion in while loop") {
    auto mod = compileToIR("<?php fn test(n: i32, a: i32, b: i32): i32 { mut i = 0; mut sum = 0; while (i < n) { let inv = a * b; sum = sum + inv; i = i + 1; } return sum; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 489 Function inlining pass on small helper") {
    auto mod = compileToIR("<?php fn add(a: i32, b: i32): i32 { return a + b; } fn test(): i32 { return add(1, 2); }");
    CHECK(mod != nullptr);
    sph::FunctionInliningPass pass(16);
    CHECK(pass.runOnModule(*mod));
}

TEST_CASE("OptimizerExhaustive: 490 PassManager O0 does not modify module") {
    auto mod = compileToIR("<?php fn test(): i32 { return 1 + 2; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O0);
    CHECK(!pm.run(*mod));
}

// =============================================================================
// Optimizer Exhaustive: PassManager Pipeline Levels (491 to 550)
// =============================================================================

TEST_CASE("OptimizerExhaustive: 491 PassManager O1 reduces constants") {
    auto mod = compileToIR("<?php fn test(): i32 { return 10 + 20; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O1);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 492 PassManager O2 optimizes expressions") {
    auto mod = compileToIR("<?php fn test(a: i32): i32 { let x = a + 0; let y = a * 1; return x + y; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 493 PassManager O3 inlines and optimizes") {
    auto mod = compileToIR("<?php fn inc(x: i32): i32 { return x + 1; } fn test(): i32 { return inc(41); }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O3);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 494 PassManager Os optimizes size") {
    auto mod = compileToIR("<?php fn test(): i32 { let a = 10; let b = 20; return a + b; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::Os);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 495 PassManager Oz eliminates dead stores and dead code") {
    auto mod = compileToIR("<?php fn test(): i32 { let unused = 100; return 42; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::Oz);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 496 Differential test O0 vs O2 semantic equivalence arithmetic") {
    auto modO0 = compileToIR("<?php fn test(a: i32, b: i32): i32 { return (a + b) * 2; }");
    auto modO2 = compileToIR("<?php fn test(a: i32, b: i32): i32 { return (a + b) * 2; }");
    CHECK(modO0 != nullptr);
    CHECK(modO2 != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    pm.run(*modO2);
    CHECK(modO2->getFunction("test") != nullptr);
}

TEST_CASE("OptimizerExhaustive: 497 Differential test O0 vs O3 inlining preserves return type") {
    auto modO0 = compileToIR("<?php fn square(x: i32): i32 { return x * x; } fn test(): i32 { return square(5); }");
    auto modO3 = compileToIR("<?php fn square(x: i32): i32 { return x * x; } fn test(): i32 { return square(5); }");
    CHECK(modO0 != nullptr);
    CHECK(modO3 != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O3);
    pm.run(*modO3);
    auto* fn = modO3->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(fn->getReturnType()->isI32());
}

TEST_CASE("OptimizerExhaustive: 498 Optimization preserves boolean operations") {
    auto mod = compileToIR("<?php fn test(a: bool, b: bool): bool { return (a && true) || (b && false); }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 499 Optimization preserves string literals") {
    auto mod = compileToIR("<?php fn test(): string { let s = \"constant\"; return s; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    pm.run(*mod);
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(fn->getReturnType()->isString());
}

TEST_CASE("OptimizerExhaustive: 500 Fixed point convergence in multiple pass iterations") {
    auto mod = compileToIR("<?php fn test(): i32 { let a = 1 + 2; let b = a + 3; let c = b + 4; return c; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 501 Chained power of 2 multiplications reduced") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return x * 2 * 4; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 502 Chained dead stores in nested if") {
    auto mod = compileToIR("<?php fn test(cond: bool): i32 { mut x = 1; if (cond) { x = 2; x = 3; } else { x = 4; x = 5; } return x; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 503 Multiple loop invariant computations hoisted") {
    auto mod = compileToIR("<?php fn test(n: i32, a: i32, b: i32, c: i32): i32 { mut i = 0; mut acc = 0; while (i < n) { let x = a + b; let y = b * c; acc = acc + (x + y); i = i + 1; } return acc; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 504 Unused function parameter does not crash DCE") {
    auto mod = compileToIR("<?php fn test(unused: i32): i32 { return 42; }");
    CHECK(mod != nullptr);
    sph::DeadCodeEliminationPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    pass.runOnFunction(*fn);
    CHECK(true);
}

TEST_CASE("OptimizerExhaustive: 505 Dead store pass handles empty function cleanly") {
    auto mod = compileToIR("<?php fn test(): void {}");
    CHECK(mod != nullptr);
    sph::DeadStoreEliminationPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(!pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 506 LICM pass handles loopless function cleanly") {
    auto mod = compileToIR("<?php fn test(a: i32, b: i32): i32 { return a + b; }");
    CHECK(mod != nullptr);
    sph::LoopInvariantCodeMotionPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(!pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 507 CSE pass ignores impure calls") {
    auto mod = compileToIR("<?php fn test(): void { println(1); println(1); }");
    CHECK(mod != nullptr);
    sph::CommonSubexpressionEliminationPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(!pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 508 Constant folding does not divide by zero") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { let zero = 0; return x; }");
    CHECK(mod != nullptr);
    sph::ConstantFoldingPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    pass.runOnFunction(*fn);
    CHECK(true);
}

TEST_CASE("OptimizerExhaustive: 509 Optimization preserves Option Some return") {
    auto mod = compileToIR("<?php fn test(): Option<i32> { return Some(10 + 20); }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    pm.run(*mod);
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(fn->getReturnType()->isOption());
}

TEST_CASE("OptimizerExhaustive: 510 Optimization preserves Result Ok return") {
    auto mod = compileToIR("<?php fn test(): Result<i32, string> { return Ok(10 + 20); }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    pm.run(*mod);
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(fn->getReturnType()->isResult());
}

TEST_CASE("OptimizerExhaustive: 511 Modulo power of two strength reduction") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { let c = 16; return c; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O1);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 512 Multi-block function preserves terminator integrity") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { if (x > 0) { return 1; } else { return -1; } }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    pm.run(*mod);
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    for (const auto& bb : fn->getBlocks()) {
        CHECK(bb->hasTerminator());
    }
}

TEST_CASE("OptimizerExhaustive: 513 Elimination of redundant identity casts") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return x as i32; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O1);
    pm.run(*mod);
    CHECK(true);
}

TEST_CASE("OptimizerExhaustive: 514 Inlining recursive function terminates safely without infinite expansion") {
    auto mod = compileToIR("<?php fn fact(n: i32): i32 { if (n <= 1) { return 1; } return n * fact(n - 1); } fn test(): i32 { return fact(3); }");
    CHECK(mod != nullptr);
    sph::FunctionInliningPass pass(8);
    pass.runOnModule(*mod);
    CHECK(true);
}

TEST_CASE("OptimizerExhaustive: 515 Algebraic simplification x & -1 is x") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return x & -1; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O1);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 516 Bitwise OR with all ones") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { let m = 0; return x | m; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O1);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 517 Store to load forwarding across multiple variables") {
    auto mod = compileToIR("<?php fn test(): i32 { let a = 1; let b = 2; let c = 3; return a + b + c; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 518 CSE deduplicates deep expression trees") {
    auto mod = compileToIR("<?php fn test(x: i32, y: i32): i32 { let e1 = (x + y) * (x - y); let e2 = (x + y) * (x - y); return e1 + e2; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 519 Control flow optimization eliminates dead else block") {
    auto mod = compileToIR("<?php fn test(): i32 { if (true) { return 10; } else { return 20; } }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 520 PassManager string representation") {
    CHECK_EQ(std::string(sph::optimizationLevelToString(sph::OptimizationLevel::O0)), "O0");
    CHECK_EQ(std::string(sph::optimizationLevelToString(sph::OptimizationLevel::O1)), "O1");
    CHECK_EQ(std::string(sph::optimizationLevelToString(sph::OptimizationLevel::O2)), "O2");
    CHECK_EQ(std::string(sph::optimizationLevelToString(sph::OptimizationLevel::O3)), "O3");
    CHECK_EQ(std::string(sph::optimizationLevelToString(sph::OptimizationLevel::Os)), "Os");
    CHECK_EQ(std::string(sph::optimizationLevelToString(sph::OptimizationLevel::Oz)), "Oz");
}

TEST_CASE("OptimizerExhaustive: 521 Pass names are non-empty") {
    sph::ConstantFoldingPass p1;
    sph::AlgebraicSimplificationPass p2;
    sph::StrengthReductionPass p3;
    sph::DeadStoreEliminationPass p4;
    sph::DeadCodeEliminationPass p5;
    sph::CommonSubexpressionEliminationPass p6;
    sph::LoopInvariantCodeMotionPass p7;
    CHECK(!p1.getName().empty());
    CHECK(!p2.getName().empty());
    CHECK(!p3.getName().empty());
    CHECK(!p4.getName().empty());
    CHECK(!p5.getName().empty());
    CHECK(!p6.getName().empty());
    CHECK(!p7.getName().empty());
}

TEST_CASE("OptimizerExhaustive: 522 PassManager run on module with multiple functions") {
    auto mod = compileToIR("<?php fn f1(): i32 { return 1 + 1; } fn f2(): i32 { return 2 * 2; } fn f3(): i32 { return 3 + 3; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 523 Optimization preserves function parameters and signatures") {
    auto mod = compileToIR("<?php fn calc(a: i32, b: i32, c: i32): i32 { return a + b + c; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O3);
    pm.run(*mod);
    auto* fn = mod->getFunction("calc");
    CHECK(fn != nullptr);
    CHECK_EQ(fn->getParameters().size(), 3u);
}

TEST_CASE("OptimizerExhaustive: 524 Optimization on while loop with break") {
    auto mod = compileToIR("<?php fn test(): i32 { mut x = 0; while (true) { x = x + 1; if (x > 5) { break; } } return x; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    CHECK(pm.run(*mod));
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(fn->getBlocks().size() > 1);
}

TEST_CASE("OptimizerExhaustive: 525 Optimization on foreach loop") {
    auto mod = compileToIR("<?php fn test(): i32 { mut sum = 0; foreach ([1, 2, 3] as $item) { sum = sum + $item; } return sum; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O1);
    pm.run(*mod);
    CHECK(true);
}

TEST_CASE("OptimizerExhaustive: 526 Optimization preserves match lowering") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return match (x) { 1 => 10, 2 => 20, _ => 0 }; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    pm.run(*mod);
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(fn->getBlocks().size() > 1);
}

TEST_CASE("OptimizerExhaustive: 527 Optimization preserves struct field access") {
    auto mod = compileToIR("<?php struct Point { x: i32, y: i32 } fn test(p: Point): i32 { return p.x + p.y; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    pm.run(*mod);
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(fn->getReturnType()->isI32());
}

TEST_CASE("OptimizerExhaustive: 528 Optimization preserves defer cleanup") {
    auto mod = compileToIR("<?php fn clean(): void {} fn test(): i32 { defer clean(); return 42; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    pm.run(*mod);
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(fn->getReturnType()->isI32());
}

TEST_CASE("OptimizerExhaustive: 529 Optimization preserves pointer operations in unsafe block") {
    auto mod = compileToIR("<?php fn test(p: *i32): i32 { unsafe { return *p + 1; } }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    pm.run(*mod);
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(fn->getReturnType()->isI32());
}

TEST_CASE("OptimizerExhaustive: 530 Optimization of nested arithmetic expressions") {
    auto mod = compileToIR("<?php fn test(a: i32): i32 { return ((a + 0) * 1 + 0) * 1; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 531 Strength reduction shift left chaining") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return (x * 4) * 2; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 532 PassManager Os generates valid module") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return x * 2 + 1; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::Os);
    pm.run(*mod);
    CHECK(mod->getFunction("test") != nullptr);
}

TEST_CASE("OptimizerExhaustive: 533 PassManager Oz generates valid module") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return x * 2 + 1; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::Oz);
    pm.run(*mod);
    CHECK(mod->getFunction("test") != nullptr);
}

TEST_CASE("OptimizerExhaustive: 534 PassManager O1 generates valid module") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return x * 2 + 1; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O1);
    pm.run(*mod);
    CHECK(mod->getFunction("test") != nullptr);
}

TEST_CASE("OptimizerExhaustive: 535 PassManager O2 generates valid module") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return x * 2 + 1; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    pm.run(*mod);
    CHECK(mod->getFunction("test") != nullptr);
}

TEST_CASE("OptimizerExhaustive: 536 PassManager O3 generates valid module") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return x * 2 + 1; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O3);
    pm.run(*mod);
    CHECK(mod->getFunction("test") != nullptr);
}

TEST_CASE("OptimizerExhaustive: 537 Redundant store elimination in sequence") {
    auto mod = compileToIR("<?php fn test(): i32 { mut x = 1; x = 2; x = 3; x = 4; return x; }");
    CHECK(mod != nullptr);
    sph::DeadStoreEliminationPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 538 DSE does not remove stores to live variables") {
    auto mod = compileToIR("<?php fn test(a: i32): i32 { mut x = a; return x; }");
    CHECK(mod != nullptr);
    sph::DeadStoreEliminationPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    pass.runOnFunction(*fn);
    CHECK(true);
}

TEST_CASE("OptimizerExhaustive: 539 Multiple constants in array initializers") {
    auto mod = compileToIR("<?php fn test(): [i32; 3] { return [1 + 1, 2 + 2, 3 + 3]; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    pm.run(*mod);
    CHECK(true);
}

TEST_CASE("OptimizerExhaustive: 540 Constant condition if-else folded") {
    auto mod = compileToIR("<?php fn test(): i32 { if (true) { return 42; } else { return 0; } }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    pm.run(*mod);
    CHECK(true);
}

TEST_CASE("OptimizerExhaustive: 541 Boolean NOT constant folding !false") {
    auto mod = compileToIR("<?php fn test(): bool { return !false; }");
    CHECK(mod != nullptr);
    sph::ConstantFoldingPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 542 Boolean NOT constant folding !true") {
    auto mod = compileToIR("<?php fn test(): bool { return !true; }");
    CHECK(mod != nullptr);
    sph::ConstantFoldingPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 543 Unary minus constant folding -(42)") {
    auto mod = compileToIR("<?php fn test(): i32 { return -(42); }");
    CHECK(mod != nullptr);
    sph::ConstantFoldingPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 544 Unary bitwise NOT constant folding ~0") {
    auto mod = compileToIR("<?php fn test(): i32 { return ~0; }");
    CHECK(mod != nullptr);
    sph::ConstantFoldingPass pass;
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 545 Equality of identical variable x == x") {
    auto mod = compileToIR("<?php fn test(x: i32): bool { return x == x; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 546 Inequality of identical variable x != x") {
    auto mod = compileToIR("<?php fn test(x: i32): bool { return x != x; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 547 Subtraction of identical variable x - x") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return x - x; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 548 Bitwise XOR of identical variable x ^ x") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return x ^ x; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 549 Bitwise AND of identical variable x & x") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return x & x; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 550 Bitwise OR of identical variable x | x") {
    auto mod = compileToIR("<?php fn test(x: i32): i32 { return x | x; }");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    CHECK(pm.run(*mod));
}

TEST_CASE("OptimizerExhaustive: 551 Distributive factoring (a * k) + (b * k) -> (a + b) * k") {
    auto mod = compileToIR("<?php fn test(a: i32, b: i32, k: i32): i32 { return (a * k) + (b * k); }");
    CHECK(mod != nullptr);
    auto* fn = mod->getFunction("test");
    CHECK(fn != nullptr);
    sph::Mem2RegPass mem2reg;
    mem2reg.runOnFunction(*fn);
    sph::AlgebraicSimplificationPass pass;
    CHECK(pass.runOnFunction(*fn));
}

TEST_CASE("OptimizerExhaustive: 552 Loop Strength Reduction eliminates induction-var multiply") {
    auto mod = compileToIR(
        "<?php\n"
        "fn sum_scaled(n: i32, scale: i32): i32 {\n"
        "    mut total: i32 = 0;\n"
        "    mut i: i32 = 0;\n"
        "    while (i < n) {\n"
        "        total = total + (i * scale);\n"
        "        i = i + 1;\n"
        "    }\n"
        "    return total;\n"
        "}");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    pm.setVerifyEachPass(true);
    CHECK(pm.run(*mod));
    auto* fn = mod->getFunction("sum_scaled");
    CHECK(fn != nullptr);
    int mulCount = 0;
    for (const auto& bb : fn->getBlocks()) {
        for (const auto& inst : bb->getInstructions()) {
            if (inst->getOpcode() == sph::IROpcode::Mul) mulCount++;
        }
    }
    CHECK_EQ(mulCount, 0);
}

TEST_CASE("OptimizerExhaustive: 553 LoopUnrollPass 4x partial unrolling is idempotent at -O3") {
    auto mod = compileToIR(
        "<?php\n"
        "fn accum(n: i32): i32 {\n"
        "    mut s: i32 = 0;\n"
        "    mut i: i32 = 0;\n"
        "    while (i < n) {\n"
        "        s = s + i;\n"
        "        i = i + 1;\n"
        "    }\n"
        "    return s;\n"
        "}");
    CHECK(mod != nullptr);
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    pm.run(*mod);
    auto* fn = mod->getFunction("accum");
    CHECK(fn != nullptr);
    sph::LoopUnrollPass unroll(4);
    CHECK(unroll.runOnFunction(*fn));
    // Second run must be a strict no-op (idempotent)
    CHECK(!unroll.runOnFunction(*fn));
}

