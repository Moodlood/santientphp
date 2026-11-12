#include "common/test_framework.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/ir/IRGenerator.hpp"
#include "compiler/ir/IRPrinter.hpp"
#include "compiler/optimizer/AlgebraicSimplificationPass.hpp"
#include "compiler/optimizer/CommonSubexpressionEliminationPass.hpp"
#include "compiler/optimizer/ConstantFoldingPass.hpp"
#include "compiler/optimizer/ControlFlowOptimizationPass.hpp"
#include "compiler/optimizer/DeadCodeEliminationPass.hpp"
#include "compiler/optimizer/FunctionInliningPass.hpp"
#include "compiler/optimizer/PassManager.hpp"
#include "compiler/optimizer/StoreToLoadForwardingPass.hpp"
#include "compiler/parser/Parser.hpp"
#include "compiler/semantic/SemanticAnalyzer.hpp"
#include "compiler/source/SourceManager.hpp"

using namespace sph;

namespace {

std::unique_ptr<sph::IRModule> parseAndGenerateIR(const std::string& code, const std::string& moduleName = "test") {
    sph::SourceManager sm;
    uint32_t fileId = sm.addSource("opt_test.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    if (!program || emitter.hasErrors()) return nullptr;

    sph::SemanticAnalyzer analyzer(emitter);
    if (!analyzer.analyze(*program) || emitter.hasErrors()) return nullptr;

    sph::IRGenerator irGen;
    return irGen.generate(*program, moduleName);
}

} // namespace

TEST_CASE("Optimizer: ConstantFoldingPass folds arithmetic, comparisons, and branches") {
    std::string code =
        "<?php\n"
        "fn test_fold(): i32 {\n"
        "    let a: i32 = 10 + 20;\n"
        "    let b: i32 = 5 * 4;\n"
        "    let c: i32 = a - b;\n"
        "    let isGreater: bool = 100 > 50;\n"
        "    if (isGreater) {\n"
        "        return c;\n"
        "    } else {\n"
        "        return 0;\n"
        "    }\n"
        "}\n";

    auto irModule = parseAndGenerateIR(code);
    CHECK(irModule != nullptr);
    if (!irModule) return;

    auto* fn = irModule->getFunction("test_fold");
    CHECK(fn != nullptr);
    if (!fn) return;

    sph::StoreToLoadForwardingPass fwdPass;
    fwdPass.runOnFunction(*fn);

    sph::ConstantFoldingPass foldPass;
    bool changed = foldPass.runOnFunction(*fn);
    CHECK(changed);

    // After constant folding, the branch condition is folded to true
    // And instructions are replaced with constants
    sph::DeadCodeEliminationPass dcePass;
    dcePass.runOnFunction(*fn);

    // The function should now have only the entry block and true path, with return of 10
    bool hasBranch = false;
    for (const auto& bb : fn->getBlocks()) {
        for (const auto& inst : bb->getInstructions()) {
            if (inst->getOpcode() == IROpcode::CondBr) {
                hasBranch = true;
            }
        }
    }
    CHECK(!hasBranch); // Condition was folded away!
}

TEST_CASE("Optimizer: AlgebraicSimplificationPass simplifies identities and strength-reduces multiplication") {
    std::string code =
        "<?php\n"
        "fn test_algebraic(x: i32): i32 {\n"
        "    let a = x + 0;\n"
        "    let b = a * 1;\n"
        "    let c = b * 8;\n" // Strength reduction: x * 8 -> x << 3
        "    let d = c - c;\n" // Self subtraction: c - c -> 0
        "    return d;\n"
        "}\n";

    auto irModule = parseAndGenerateIR(code);
    CHECK(irModule != nullptr);
    if (!irModule) return;

    auto* fn = irModule->getFunction("test_algebraic");
    CHECK(fn != nullptr);
    if (!fn) return;

    sph::StoreToLoadForwardingPass fwdPass;
    fwdPass.runOnFunction(*fn);

    sph::AlgebraicSimplificationPass algPass;
    bool changed = algPass.runOnFunction(*fn);
    CHECK(changed);

    sph::DeadCodeEliminationPass dcePass;
    dcePass.runOnFunction(*fn);

    // Check that return value got simplified to 0
    auto& retInst = fn->getBlocks().back()->getInstructions().back();
    CHECK_EQ(retInst->getOpcode(), IROpcode::Ret);
    auto ret = std::static_pointer_cast<sph::RetInst>(retInst);
    CHECK(ret->hasValue());
    auto cInt = dynamic_cast<const sph::ConstantInt*>(ret->getValue().get());
    CHECK(cInt != nullptr);
    if (cInt) {
        CHECK_EQ(cInt->getValue(), 0);
    }
}

TEST_CASE("Optimizer: CommonSubexpressionEliminationPass eliminates duplicate expressions") {
    std::string code =
        "<?php\n"
        "fn test_cse(a: i32, b: i32): i32 {\n"
        "    let x = a + b;\n"
        "    let y = b + a;\n" // Commutative match!
        "    let z = x * y;\n"
        "    return z;\n"
        "}\n";

    auto irModule = parseAndGenerateIR(code);
    CHECK(irModule != nullptr);
    if (!irModule) return;

    auto* fn = irModule->getFunction("test_cse");
    CHECK(fn != nullptr);
    if (!fn) return;

    sph::StoreToLoadForwardingPass fwdPass;
    fwdPass.runOnFunction(*fn);

    sph::CommonSubexpressionEliminationPass csePass;
    bool changed = csePass.runOnFunction(*fn);
    CHECK(changed);

    sph::DeadCodeEliminationPass dcePass;
    dcePass.runOnFunction(*fn);

    // Count Add instructions: should now only be 1 instead of 2!
    int addCount = 0;
    for (const auto& bb : fn->getBlocks()) {
        for (const auto& inst : bb->getInstructions()) {
            if (inst->getOpcode() == IROpcode::Add) {
                addCount++;
            }
        }
    }
    CHECK_EQ(addCount, 1);
}

TEST_CASE("Optimizer: FunctionInliningPass inlines small leaf functions") {
    std::string code =
        "<?php\n"
        "fn square(x: i32): i32 {\n"
        "    return x * x;\n"
        "}\n"
        "fn compute(v: i32): i32 {\n"
        "    let res = square(v);\n"
        "    return res;\n"
        "}\n";

    auto irModule = parseAndGenerateIR(code);
    CHECK(irModule != nullptr);
    if (!irModule) return;

    sph::FunctionInliningPass inlinePass;
    bool changed = inlinePass.runOnModule(*irModule);
    CHECK(changed);

    auto* computeFn = irModule->getFunction("compute");
    CHECK(computeFn != nullptr);
    if (!computeFn) return;

    // Call instruction inside compute should be replaced by inlined multiplication!
    int callCount = 0;
    int mulCount = 0;
    for (const auto& bb : computeFn->getBlocks()) {
        for (const auto& inst : bb->getInstructions()) {
            if (inst->getOpcode() == IROpcode::Call) callCount++;
            if (inst->getOpcode() == IROpcode::Mul) mulCount++;
        }
    }
    CHECK_EQ(callCount, 0);
    CHECK_EQ(mulCount, 1);
}

TEST_CASE("Optimizer: PassManager O2 pipeline optimizes complex computation to constant") {
    std::string code =
        "<?php\n"
        "fn compute_const(): i32 {\n"
        "    let a: i32 = 10 * 2;\n"
        "    let b: i32 = a + 5;\n"
        "    let c: i32 = b * 0;\n" // -> 0
        "    let d: i32 = c + 42;\n" // -> 42
        "    return d;\n"
        "}\n";

    auto irModule = parseAndGenerateIR(code);
    CHECK(irModule != nullptr);
    if (!irModule) return;

    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    bool changed = pm.run(*irModule);
    CHECK(changed);

    auto* fn = irModule->getFunction("compute_const");
    CHECK(fn != nullptr);
    if (!fn) return;

    auto& retInst = fn->getBlocks().front()->getInstructions().back();
    CHECK_EQ(retInst->getOpcode(), IROpcode::Ret);
    auto ret = std::static_pointer_cast<sph::RetInst>(retInst);
    CHECK(ret->hasValue());
    auto cInt = dynamic_cast<const sph::ConstantInt*>(ret->getValue().get());
    CHECK(cInt != nullptr);
    if (cInt) {
        CHECK_EQ(cInt->getValue(), 42);
    }
}

TEST_CASE("Optimizer: StoreToLoadForwardingPass eliminates redundant loads and forwards stored values") {
    std::string code =
        "<?php\n"
        "fn forward_test(x: i32): i32 {\n"
        "    let a = x;\n"
        "    let b = a;\n"
        "    let c = b;\n"
        "    return c;\n"
        "}\n";

    auto irModule = parseAndGenerateIR(code);
    CHECK(irModule != nullptr);
    if (!irModule) return;

    auto* fn = irModule->getFunction("forward_test");
    CHECK(fn != nullptr);
    if (!fn) return;

    sph::StoreToLoadForwardingPass fwdPass;
    bool changed = fwdPass.runOnFunction(*fn);
    CHECK(changed);

    // After forwarding, loads should be eliminated and return instruction should directly use parameter x!
    auto& retInst = fn->getBlocks().front()->getInstructions().back();
    CHECK_EQ(retInst->getOpcode(), IROpcode::Ret);
    auto ret = std::static_pointer_cast<sph::RetInst>(retInst);
    CHECK(ret->hasValue());
    // The returned value should not be a LoadInst!
    CHECK(dynamic_cast<const sph::LoadInst*>(ret->getValue().get()) == nullptr);
}


