#include "common/test_framework.hpp"
#include "compiler/codegen/Linker.hpp"
#include "compiler/codegen/llvm/LLVMEmitter.hpp"
#include "compiler/codegen/x86_64/X86_64CodeGen.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/ir/IRGenerator.hpp"
#include "compiler/optimizer/PassManager.hpp"
#include "compiler/parser/Parser.hpp"
#include "compiler/source/SourceManager.hpp"
#include "compiler/semantic/SemanticAnalyzer.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace sph {

namespace {

bool compileAndRunNative(const IRModule& module, const std::string& tag, std::vector<std::string>& outLines) {
    std::filesystem::path tempAsm = "test_parity_" + tag + ".s";
    std::filesystem::path tempExe = "test_parity_" + tag + ".exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    {
        std::ofstream asmFile(tempAsm);
        X86_64CodeGen codegen(asmFile);
        codegen.generate(module);
    }

    Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::error_code ec;
    std::filesystem::remove(tempAsm, ec);

    if (!linkOk || !std::filesystem::exists(tempExe)) {
        return false;
    }

    std::string outputLog = "test_parity_" + tag + "_out.txt";
    std::string runCmd = std::string(SPH_RUN_PREFIX) + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());

    std::ifstream outFile(outputLog);
    std::string line;
    outLines.clear();
    while (std::getline(outFile, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        outLines.push_back(line);
    }
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    return runRes == 0;
}

std::string emitLLVMIR(const IRModule& module) {
    std::ostringstream ss;
    LLVMEmitter emitter(ss);
    emitter.emit(module);
    return ss.str();
}

bool verifyBackendParity(const std::string& code, const std::vector<std::string>& expectedOutput, const std::string& testName) {
    SourceManager sm;
    uint32_t fileId = sm.addSource("parity_" + testName + ".sph", code);
    DiagnosticEmitter emitter(sm);
    Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    if (!program) return false;

    SemanticAnalyzer analyzer(emitter);
    if (!analyzer.analyze(*program)) return false;

    // Test across all optimization levels: -O0, -O1, -O2, -O3
    OptimizationLevel optLevels[] = {
        OptimizationLevel::O0,
        OptimizationLevel::O1,
        OptimizationLevel::O2,
        OptimizationLevel::O3
    };

    std::vector<std::string> baselineLines;

    for (int i = 0; i < 4; ++i) {
        OptimizationLevel lvl = optLevels[i];
        std::string tag = testName + "_O" + std::to_string(i);

        IRGenerator irGen;
        auto irMod = irGen.generate(*program, "mod_" + tag);
        if (!irMod) return false;

        auto pm = PassManager::createForLevel(lvl);
        pm.run(*irMod);

        // Native backend execution
        std::vector<std::string> nativeLines;
        if (!compileAndRunNative(*irMod, tag, nativeLines)) {
            return false;
        }

        if (nativeLines.size() != expectedOutput.size()) return false;
        for (size_t k = 0; k < expectedOutput.size(); ++k) {
            if (nativeLines[k] != expectedOutput[k]) return false;
        }

        // Verify LLVM IR backend emission
        std::string llvmIR = emitLLVMIR(*irMod);
        if (llvmIR.empty()) return false;
        if (llvmIR.find("target triple") == std::string::npos) return false;
        if (llvmIR.find("define") == std::string::npos) return false;

        // Optimization consistency check: outputs across -O0..-O3 must be identical
        if (i == 0) {
            baselineLines = nativeLines;
        } else {
            if (nativeLines != baselineLines) return false;
        }
    }

    return true;
}

} // namespace

TEST_CASE("Backend Parity: Integer and Bitwise Operations across -O0 to -O3") {
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    let a: i32 = 120;\n"
        "    let b: i32 = 15;\n"
        "    println(a + b);\n"
        "    println(a - b);\n"
        "    println(a * b);\n"
        "    println(a / b);\n"
        "    println(a % b);\n"
        "    println(a & b);\n"
        "    println(a | b);\n"
        "    println(a ^ b);\n"
        "    println(b << 2);\n"
        "    println(a >> 2);\n"
        "    return 0;\n"
        "}\n";

    std::vector<std::string> expected = {
        "135", "105", "1800", "8", "0",
        "8", "127", "119", "60", "30"
    };

    CHECK(verifyBackendParity(code, expected, "int_bitwise"));
}

TEST_CASE("Backend Parity: Floating-Point Arithmetic and Comparisons across -O0 to -O3") {
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    let x: f64 = 10.5;\n"
        "    let y: f64 = 2.5;\n"
        "    println(x + y);\n"
        "    println(x - y);\n"
        "    println(x * y);\n"
        "    println(x / y);\n"
        "    if (x > y) { println(1); } else { println(0); }\n"
        "    if (x <= y) { println(1); } else { println(0); }\n"
        "    return 0;\n"
        "}\n";

    std::vector<std::string> expected = {
        "13", "8", "26.25", "4.2", "1", "0"
    };

    CHECK(verifyBackendParity(code, expected, "fp_ops"));
}

TEST_CASE("Backend Parity: Control Flow, Loops, and Break/Continue across -O0 to -O3") {
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    let mut sum: i32 = 0;\n"
        "    let mut i: i32 = 0;\n"
        "    while (i < 10) {\n"
        "        i = i + 1;\n"
        "        if (i == 3) { continue; }\n"
        "        if (i == 8) { break; }\n"
        "        sum = sum + i;\n"
        "    }\n"
        "    println(sum);\n" // 1 + 2 + 4 + 5 + 6 + 7 = 25
        "    println(i);\n"   // 8
        "    return 0;\n"
        "}\n";

    CHECK(verifyBackendParity(code, {"25", "8"}, "loop_ctrl"));
}

TEST_CASE("Backend Parity: Struct Construction, Mutation, and Field Reads across -O0 to -O3") {
    std::string code =
        "<?php\n"
        "struct Point {\n"
        "    x: i32,\n"
        "    y: i32,\n"
        "}\n"
        "fn manhattan(p: Point): i32 {\n"
        "    return p.x + p.y;\n"
        "}\n"
        "fn main(): i32 {\n"
        "    let mut pt = Point { x: 14, y: 28 };\n"
        "    println(manhattan(pt));\n"
        "    pt.x = 100;\n"
        "    println(manhattan(pt));\n"
        "    return 0;\n"
        "}\n";

    CHECK(verifyBackendParity(code, {"42", "128"}, "struct_ops"));
}

TEST_CASE("Backend Parity: Recursion and Nested Functions across -O0 to -O3") {
    std::string code =
        "<?php\n"
        "fn fib(n: i32): i32 {\n"
        "    if (n <= 1) { return n; }\n"
        "    return fib(n - 1) + fib(n - 2);\n"
        "}\n"
        "fn fact(n: i32): i32 {\n"
        "    if (n <= 1) { return 1; }\n"
        "    return n * fact(n - 1);\n"
        "}\n"
        "fn main(): i32 {\n"
        "    println(fib(7));\n"  // 13
        "    println(fact(5));\n" // 120
        "    return 0;\n"
        "}\n";

    CHECK(verifyBackendParity(code, {"13", "120"}, "recursion_parity"));
}

TEST_CASE("Backend Parity: Heap Box Allocations and Determinisitic Deallocations across -O0 to -O3") {
    std::string code =
        "<?php\n"
        "struct BoxItem {\n"
        "    val: i32,\n"
        "}\n"
        "fn compute(): i32 {\n"
        "    let b = Box::new(BoxItem { val: 777 });\n"
        "    return b.val;\n"
        "}\n"
        "fn main(): i32 {\n"
        "    let val = compute();\n"
        "    println(val);\n"
        "    return 0;\n"
        "}\n";

    CHECK(verifyBackendParity(code, {"777"}, "box_heap_parity"));
}

} // namespace sph
