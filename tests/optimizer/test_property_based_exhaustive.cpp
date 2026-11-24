#include "common/test_framework.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/ir/IRBasicBlock.hpp"
#include "compiler/ir/IRFunction.hpp"
#include "compiler/ir/IRGenerator.hpp"
#include "compiler/ir/IRInstruction.hpp"
#include "compiler/ir/IRModule.hpp"
#include "compiler/ir/IRVerifier.hpp"
#include "compiler/optimizer/ConstantFoldingPass.hpp"
#include "compiler/optimizer/DeadCodeEliminationPass.hpp"
#include "compiler/optimizer/PassManager.hpp"
#include "compiler/parser/Parser.hpp"
#include "compiler/semantic/SemanticAnalyzer.hpp"
#include "compiler/source/SourceManager.hpp"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sph {

namespace {

size_t countTotalInstructions(const IRFunction& fn) {
    size_t count = 0;
    for (const auto& bb : fn.getBlocks()) {
        if (bb) count += bb->getInstructions().size();
    }
    return count;
}

std::vector<IROpcode> getOpcodeSequence(const IRFunction& fn) {
    std::vector<IROpcode> ops;
    for (const auto& bb : fn.getBlocks()) {
        if (!bb) continue;
        for (const auto& inst : bb->getInstructions()) {
            if (inst) ops.push_back(inst->getOpcode());
        }
    }
    return ops;
}

std::unique_ptr<IRModule> buildModuleFromSource(const std::string& code, const std::string& name) {
    SourceManager sm;
    uint32_t fileId = sm.addSource(name + ".sph", code);
    DiagnosticEmitter emitter(sm);
    Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    if (!program) return nullptr;

    SemanticAnalyzer analyzer(emitter);
    if (!analyzer.analyze(*program)) return nullptr;

    IRGenerator irGen;
    return irGen.generate(*program, name);
}

} // namespace

TEST_CASE("Property-Based: ConstantFoldingPass Idempotency Invariant") {
    std::string code =
        "<?php\n"
        "fn test_fold(): i32 {\n"
        "    let a: i32 = 10 + 20 * 3;\n"
        "    let b: i32 = 100 / 4 - 5;\n"
        "    let c: i32 = a + b;\n"
        "    return c;\n"
        "}\n";

    auto mod = buildModuleFromSource(code, "prop_fold");
    CHECK(mod != nullptr);

    auto* fn = mod->getFunction("test_fold");
    CHECK(fn != nullptr);

    ConstantFoldingPass pass;
    pass.runOnFunction(*fn);
    auto opsPass1 = getOpcodeSequence(*fn);

    // Second run must be strictly idempotent
    pass.runOnFunction(*fn);
    auto opsPass2 = getOpcodeSequence(*fn);

    CHECK_EQ(opsPass1.size(), opsPass2.size());
    CHECK(opsPass1 == opsPass2);

    IRVerifier verifier;
    std::vector<std::string> errors;
    CHECK(verifier.verify(*fn, errors));
}

TEST_CASE("Property-Based: DeadCodeEliminationPass Idempotency Invariant") {
    std::string code =
        "<?php\n"
        "fn test_dce(): i32 {\n"
        "    let dead1: i32 = 100 * 2;\n"
        "    let dead2: i32 = dead1 + 5;\n"
        "    let live: i32 = 42;\n"
        "    return live;\n"
        "}\n";

    auto mod = buildModuleFromSource(code, "prop_dce");
    CHECK(mod != nullptr);

    auto* fn = mod->getFunction("test_dce");
    CHECK(fn != nullptr);

    DeadCodeEliminationPass pass;
    pass.runOnFunction(*fn);
    auto opsPass1 = getOpcodeSequence(*fn);

    // Second run
    pass.runOnFunction(*fn);
    auto opsPass2 = getOpcodeSequence(*fn);

    CHECK_EQ(opsPass1.size(), opsPass2.size());
    CHECK(opsPass1 == opsPass2);
}

TEST_CASE("Property-Based: Full PassManager Pipeline Idempotency across O1, O2, O3") {
    std::string code =
        "<?php\n"
        "fn complex_calc(x: i32, y: i32): i32 {\n"
        "    let mut sum: i32 = 0;\n"
        "    let mut i: i32 = 0;\n"
        "    while (i < 10) {\n"
        "        let factor: i32 = x + y;\n"
        "        sum = sum + (factor * 2);\n"
        "        i = i + 1;\n"
        "    }\n"
        "    return sum;\n"
        "}\n";

    OptimizationLevel levels[] = { OptimizationLevel::O1, OptimizationLevel::O2, OptimizationLevel::O3 };

    for (int lvlIdx = 0; lvlIdx < 3; ++lvlIdx) {
        auto mod = buildModuleFromSource(code, "prop_pipeline_" + std::to_string(lvlIdx));
        CHECK(mod != nullptr);

        auto pm1 = PassManager::createForLevel(levels[lvlIdx]);
        pm1.run(*mod);

        auto* fn = mod->getFunction("complex_calc");
        CHECK(fn != nullptr);
        auto opsPass1 = getOpcodeSequence(*fn);

        // Run second pipeline pass on already-optimized module
        auto pm2 = PassManager::createForLevel(levels[lvlIdx]);
        pm2.run(*mod);
        auto opsPass2 = getOpcodeSequence(*fn);

        CHECK_EQ(opsPass1.size(), opsPass2.size());
        CHECK(opsPass1 == opsPass2);
    }
}

TEST_CASE("Property-Based: Fuzz Stream Robustness (No unhandled exceptions or crashes on corrupted inputs)") {
    uint32_t lcg = 0xDEADBEEF;

    for (int i = 0; i < 100; ++i) {
        // Generate pseudo-random ASCII and punctuation strings
        size_t len = (i % 64) + 10;
        std::string corruptedCode = "<?php ";
        for (size_t k = 0; k < len; ++k) {
            lcg = lcg * 1664525u + 1013904223u;
            char ch = static_cast<char>((lcg % 95) + 32); // printable ASCII
            corruptedCode += ch;
        }

        SourceManager sm;
        uint32_t fileId = sm.addSource("fuzz_" + std::to_string(i) + ".sph", corruptedCode);
        DiagnosticEmitter emitter(sm);

        // Lexer & parser must not throw unhandled exceptions or crash
        bool crashed = false;
        try {
            Parser parser(fileId, sm, &emitter);
            auto program = parser.parseProgram();
            (void)program;
        } catch (...) {
            crashed = true;
        }
        CHECK(!crashed);
    }
}

} // namespace sph
