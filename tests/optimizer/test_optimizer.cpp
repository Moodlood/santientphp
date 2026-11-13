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

    sph::FunctionInliningPass inlinePass(15);
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

TEST_CASE("Optimizer: FunctionInliningPass inlines leaf functions returning structs with GetFieldPtr") {
    std::string code =
        "<?php\n"
        "struct Pair { a: i32, b: i32 }\n"
        "fn make_pair(x: i32, y: i32): Pair {\n"
        "    return Pair { a: x, b: y };\n"
        "}\n"
        "fn compute(): i32 {\n"
        "    let p = make_pair(10, 20);\n"
        "    return p.a + p.b;\n"
        "}\n";

    auto irModule = parseAndGenerateIR(code);
    CHECK(irModule != nullptr);
    if (!irModule) return;

    sph::FunctionInliningPass inlinePass(15);
    bool changed = inlinePass.runOnModule(*irModule);
    CHECK(changed);

    auto* computeFn = irModule->getFunction("compute");
    CHECK(computeFn != nullptr);
    if (!computeFn) return;

    int callCount = 0;
    int getFieldCount = 0;
    for (const auto& bb : computeFn->getBlocks()) {
        for (const auto& inst : bb->getInstructions()) {
            if (inst->getOpcode() == IROpcode::Call) callCount++;
            if (inst->getOpcode() == IROpcode::GetFieldPtr) getFieldCount++;
        }
    }
    CHECK_EQ(callCount, 0);
    CHECK(getFieldCount >= 2);
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

TEST_CASE("Optimizer: StoreToLoadForwardingPass respects memory clobbers and escaped pointers") {
    auto fn = std::make_unique<sph::IRFunction>("test_escape", sph::Type::getI32(), std::vector<std::shared_ptr<sph::IRParameter>>{});
    auto* bb = fn->createBlock("entry");

    auto allocaInst = std::make_shared<sph::AllocaInst>(sph::Type::getI32(), "x");
    bb->addInstruction(allocaInst);

    auto store1 = std::make_shared<sph::StoreInst>(sph::ConstantInt::get(10, sph::Type::getI32()), allocaInst);
    bb->addInstruction(store1);

    // Call instruction passing allocaInst as argument (address escapes!)
    auto callInst = std::make_shared<sph::CallInst>("mutate_ptr", std::vector<std::shared_ptr<sph::IRValue>>{allocaInst}, sph::Type::getVoid(), "call_res");
    bb->addInstruction(callInst);

    // Load instruction after the call
    auto loadAfter = std::make_shared<sph::LoadInst>(sph::Type::getI32(), allocaInst, "load_after");
    bb->addInstruction(loadAfter);

    auto retInst = std::make_shared<sph::RetInst>(loadAfter);
    bb->addInstruction(retInst);

    sph::StoreToLoadForwardingPass fwdPass;
    fwdPass.runOnFunction(*fn);

    // Because allocaInst escaped into callInst, loadAfter MUST NOT be eliminated or replaced with 10!
    auto& finalRet = fn->getBlocks().front()->getInstructions().back();
    CHECK_EQ(finalRet->getOpcode(), sph::IROpcode::Ret);
    auto ret = std::static_pointer_cast<sph::RetInst>(finalRet);
    CHECK(ret->hasValue());
    // The returned value MUST still be the loadAfter instruction!
    CHECK_EQ(ret->getValue().get(), loadAfter.get());
}

TEST_CASE("Language: Defer statement lowers in reverse LIFO order at block exit and return") {
    std::string code =
        "<?php\n"
        "fn test_defer(): i32 {\n"
        "    mut res: i32 = 0;\n"
        "    defer res = 100;\n"
        "    defer res = 200;\n"
        "    return res;\n"
        "}\n";

    auto irModule = parseAndGenerateIR(code);
    CHECK(irModule != nullptr);
    if (!irModule) return;

    auto* fn = irModule->getFunction("test_defer");
    CHECK(fn != nullptr);
    if (!fn) return;

    // In the generated IR, before the Ret instruction, deferred assignments should be emitted
    // in LIFO order (res = 200, then res = 100).
    const auto& insts = fn->getBlocks().front()->getInstructions();
    std::vector<int64_t> storedConsts;
    for (const auto& inst : insts) {
        if (inst->getOpcode() == IROpcode::Store) {
            auto store = std::static_pointer_cast<sph::StoreInst>(inst);
            if (auto cInt = dynamic_cast<const sph::ConstantInt*>(store->getValue().get())) {
                storedConsts.push_back(cInt->getValue());
            }
        }
    }
    // Should see initial store (0), then deferred store 200, then deferred store 100
    CHECK_EQ(storedConsts.size(), static_cast<size_t>(3));
    if (storedConsts.size() == 3) {
        CHECK_EQ(storedConsts[0], 0);
        CHECK_EQ(storedConsts[1], 200);
        CHECK_EQ(storedConsts[2], 100);
    }
}

TEST_CASE("Language: Dynamic-to-Static boundary conversions and intrinsics") {
    std::string code =
        "<?php\n"
        "fn add_native(a: i32, b: i32): i32 {\n"
        "    return a + b;\n"
        "}\n"
        "fn test_boundary(): i32 {\n"
        "    $dyn = 42;\n"
        "    let x: i32 = intval($dyn);\n"
        "    let y: i32 = ($dyn as i32);\n"
        "    return add_native($dyn, y);\n"
        "}\n";

    auto irModule = parseAndGenerateIR(code);
    CHECK(irModule != nullptr);
    if (!irModule) return;

    auto* fn = irModule->getFunction("test_boundary");
    CHECK(fn != nullptr);
    if (!fn) return;

    // Check that calls to sph_value_to_int are emitted for unboxing
    bool foundUnbox = false;
    for (const auto& bb : fn->getBlocks()) {
        for (const auto& inst : bb->getInstructions()) {
            if (inst->getOpcode() == IROpcode::Call) {
                auto call = std::static_pointer_cast<sph::CallInst>(inst);
                if (call->getCallee() == "sph_value_to_int") {
                    foundUnbox = true;
                }
            }
        }
    }
    CHECK(foundUnbox);
}

TEST_CASE("Optimizer: AlgebraicSimplificationPass respects IEEE-754 floats and signed integer division") {
    std::string code =
        "<?php\n"
        "fn test_float_safe(f: f64): f64 {\n"
        "    let a = f - f;\n" // Must NOT fold to 0 for floats (NaN, Inf)
        "    let b = a * 0.0;\n" // Must NOT fold to 0 for floats
        "    return b;\n"
        "}\n"
        "fn test_signed_div(x: i32): i32 {\n"
        "    let d = x / 2;\n" // Must NOT reduce to x >> 1 for signed variable
        "    return d;\n"
        "}\n";

    auto irModule = parseAndGenerateIR(code);
    CHECK(irModule != nullptr);
    if (!irModule) return;

    auto* fnFloat = irModule->getFunction("test_float_safe");
    CHECK(fnFloat != nullptr);
    if (fnFloat) {
        sph::StoreToLoadForwardingPass fwdPass;
        fwdPass.runOnFunction(*fnFloat);

        sph::AlgebraicSimplificationPass algPass;
        algPass.runOnFunction(*fnFloat);

        // Verify that Sub and Mul instructions are still present (not collapsed to 0)
        bool hasSub = false;
        bool hasMul = false;
        for (const auto& bb : fnFloat->getBlocks()) {
            for (const auto& inst : bb->getInstructions()) {
                if (inst->getOpcode() == IROpcode::Sub) hasSub = true;
                if (inst->getOpcode() == IROpcode::Mul) hasMul = true;
            }
        }
        CHECK(hasSub);
        CHECK(hasMul);
    }

    auto* fnDiv = irModule->getFunction("test_signed_div");
    CHECK(fnDiv != nullptr);
    if (fnDiv) {
        sph::StoreToLoadForwardingPass fwdPass;
        fwdPass.runOnFunction(*fnDiv);

        sph::AlgebraicSimplificationPass algPass;
        algPass.runOnFunction(*fnDiv);

        // Verify that Div instruction is NOT replaced with ShiftRight for signed variable
        bool hasDiv = false;
        bool hasShift = false;
        for (const auto& bb : fnDiv->getBlocks()) {
            for (const auto& inst : bb->getInstructions()) {
                if (inst->getOpcode() == IROpcode::Div) hasDiv = true;
                if (inst->getOpcode() == IROpcode::ShiftRight) hasShift = true;
            }
        }
        CHECK(hasDiv);
        CHECK(!hasShift);
    }
}



