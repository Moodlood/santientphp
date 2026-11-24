#include "common/test_framework.hpp"
#include "compiler/codegen/Linker.hpp"
#include "compiler/codegen/x86_64/LinearScanAllocator.hpp"
#include "compiler/codegen/x86_64/X86_64CodeGen.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/ir/IRGenerator.hpp"
#include "compiler/optimizer/PassManager.hpp"
#include "compiler/parser/Parser.hpp"
#include "compiler/semantic/SemanticAnalyzer.hpp"
#include "compiler/source/SourceManager.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

bool compileAndRunCode(const std::string& code, const std::vector<std::string>& expectedOutputLines, const std::string& tag) {
    sph::SourceManager sm;
    uint32_t fileId = sm.addSource("pcopy_" + tag + ".sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    if (!program) { std::cerr << "parse failed for " << tag << "\n"; return false; }

    sph::SemanticAnalyzer analyzer(emitter);
    if (!analyzer.analyze(*program)) { std::cerr << "analyze failed for " << tag << "\n"; return false; }

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "pcopy_" + tag);
    if (!irModule) { std::cerr << "irgen failed for " << tag << "\n"; return false; }

    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O3);
    pm.run(*irModule);

    std::filesystem::path tempAsm = "test_pcopy_" + tag + ".s";
    std::filesystem::path tempExe = "test_pcopy_" + tag + ".exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }

    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::error_code ec;
    std::filesystem::remove(tempAsm, ec);

    if (!linkOk || !std::filesystem::exists(tempExe)) {
        std::cerr << "link failed for " << tag << ": " << errorMsg << "\n";
        return false;
    }

    std::string outputLog = "test_pcopy_" + tag + "_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());

    std::ifstream outFile(outputLog);
    std::vector<std::string> actualLines;
    std::string line;
    while (std::getline(outFile, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        actualLines.push_back(line);
    }
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    if (runRes != 0) {
        std::cerr << "Execution failed for " << tag << " with exit code " << runRes << std::endl;
        return false;
    }
    if (actualLines.size() != expectedOutputLines.size()) {
        std::cerr << "Output line count mismatch for " << tag << ": got " << actualLines.size() << ", expected " << expectedOutputLines.size() << std::endl;
        for (const auto& l : actualLines) std::cerr << "  got: '" << l << "'" << std::endl;
        return false;
    }
    for (size_t i = 0; i < expectedOutputLines.size(); ++i) {
        if (actualLines[i] != expectedOutputLines[i]) {
            std::cerr << "Output mismatch for " << tag << " line " << i << ": got '" << actualLines[i] << "', expected '" << expectedOutputLines[i] << "'" << std::endl;
            return false;
        }
    }
    return true;
}

} // namespace

TEST_CASE("ParallelCopy & Cycle Resolution: 2-variable cycle swap") {
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    let mut a: i32 = 10;\n"
        "    let mut b: i32 = 20;\n"
        "    let mut i: i32 = 0;\n"
        "    while (i < 5) {\n"
        "        let tmp: i32 = a;\n"
        "        a = b;\n"
        "        b = tmp;\n"
        "        i = i + 1;\n"
        "    }\n"
        "    println(a);\n" // After 5 swaps: a was 10, becomes 20
        "    println(b);\n" // b becomes 10
        "    return 0;\n"
        "}\n";

    CHECK(compileAndRunCode(code, {"20", "10"}, "swap2"));
}

TEST_CASE("ParallelCopy & Cycle Resolution: 3-variable cycle rotation") {
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    let mut a: i32 = 1;\n"
        "    let mut b: i32 = 2;\n"
        "    let mut c: i32 = 3;\n"
        "    let mut i: i32 = 0;\n"
        "    while (i < 4) {\n"
        "        let next_a: i32 = b;\n"
        "        let next_b: i32 = c;\n"
        "        let next_c: i32 = a;\n"
        "        a = next_a;\n"
        "        b = next_b;\n"
        "        c = next_c;\n"
        "        i = i + 1;\n"
        "    }\n"
        "    // Initial: (1, 2, 3)\n"
        "    // i=1: (2, 3, 1)\n"
        "    // i=2: (3, 1, 2)\n"
        "    // i=3: (1, 2, 3)\n"
        "    // i=4: (2, 3, 1)\n"
        "    println(a);\n"
        "    println(b);\n"
        "    println(c);\n"
        "    return 0;\n"
        "}\n";

    CHECK(compileAndRunCode(code, {"2", "3", "1"}, "rot3"));
}

TEST_CASE("ParallelCopy & Cycle Resolution: 6-variable cyclic permutation with diamond join") {
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    let mut v1: i32 = 100;\n"
        "    let mut v2: i32 = 200;\n"
        "    let mut v3: i32 = 300;\n"
        "    let mut v4: i32 = 400;\n"
        "    let mut v5: i32 = 500;\n"
        "    let mut v6: i32 = 600;\n"
        "    let mut count: i32 = 0;\n"
        "    while (count < 3) {\n"
        "        if (count % 2 == 0) {\n"
        "            let n1: i32 = v2;\n"
        "            let n2: i32 = v3;\n"
        "            let n3: i32 = v4;\n"
        "            let n4: i32 = v5;\n"
        "            let n5: i32 = v6;\n"
        "            let n6: i32 = v1;\n"
        "            v1 = n1; v2 = n2; v3 = n3; v4 = n4; v5 = n5; v6 = n6;\n"
        "        } else {\n"
        "            let n1: i32 = v6;\n"
        "            let n2: i32 = v1;\n"
        "            let n3: i32 = v2;\n"
        "            let n4: i32 = v3;\n"
        "            let n5: i32 = v4;\n"
        "            let n6: i32 = v5;\n"
        "            v1 = n1; v2 = n2; v3 = n3; v4 = n4; v5 = n5; v6 = n6;\n"
        "        }\n"
        "        count = count + 1;\n"
        "    }\n"
        "    // count=0: rotate left -> (200, 300, 400, 500, 600, 100)\n"
        "    // count=1: rotate right -> back to (100, 200, 300, 400, 500, 600)\n"
        "    // count=2: rotate left -> (200, 300, 400, 500, 600, 100)\n"
        "    println(v1);\n"
        "    println(v2);\n"
        "    println(v6);\n"
        "    return 0;\n"
        "}\n";

    CHECK(compileAndRunCode(code, {"200", "300", "100"}, "rot6"));
}

TEST_CASE("LSRA Register Pressure: Furthest-Next-Use eviction across helper call") {
    std::string code =
        "<?php\n"
        "fn helper(x: i32): i32 {\n"
        "    return x * 2;\n"
        "}\n"
        "\n"
        "fn main(): i32 {\n"
        "    let a: i32 = 1;\n"
        "    let b: i32 = 2;\n"
        "    let c: i32 = 3;\n"
        "    let d: i32 = 4;\n"
        "    let e: i32 = 5;\n"
        "    let f: i32 = 6;\n"
        "    let g: i32 = 7;\n"
        "    let h: i32 = 8;\n"
        "    let i: i32 = 9;\n"
        "    let j: i32 = 10;\n"
        "    let k: i32 = 11;\n"
        "    let l: i32 = 12;\n"
        "    let m: i32 = 13;\n"
        "    let n: i32 = 14;\n"
        "\n"
        "    let res: i32 = helper(a + b + c + d + e + f + g + h + i + j + k + l + m + n);\n"
        "    // 1+2+3+4+5+6+7+8+9+10+11+12+13+14 = 105; helper(105) = 210\n"
        "    println(res);\n"
        "    // Verify all original live values remain uncorrupted\n"
        "    let sum2: i32 = a + n;\n" // 1 + 14 = 15
        "    println(sum2);\n"
        "    return 0;\n"
        "}\n";

    CHECK(compileAndRunCode(code, {"210", "15"}, "pressure14"));
}

TEST_CASE("ABI & Calling Convention: Passing 8 arguments to internal fast function") {
    std::string code =
        "<?php\n"
        "fn sum8(a: i32, b: i32, c: i32, d: i32, e: i32, f: i32, g: i32, h: i32): i32 {\n"
        "    return a + b + c + d + e + f + g + h;\n"
        "}\n"
        "\n"
        "fn main(): i32 {\n"
        "    let total: i32 = sum8(10, 20, 30, 40, 50, 60, 70, 80);\n"
        "    println(total);\n" // 360
        "    return 0;\n"
        "}\n";

    CHECK(compileAndRunCode(code, {"360"}, "abi8args"));
}

TEST_CASE("ABI & Calling Convention: Mixed scalar and floating-point parameters") {
    std::string code =
        "<?php\n"
        "fn calc(a: i32, b: f64, c: i32, d: f64): f64 {\n"
        "    return (a as f64) + b + (c as f64) + d;\n"
        "}\n"
        "\n"
        "fn main(): i32 {\n"
        "    let res: f64 = calc(10, 2.5, 20, 7.5);\n"
        "    // 10 + 2.5 + 20 + 7.5 = 40.0\n"
        "    println(res as i32);\n"
        "    return 0;\n"
        "}\n";

    CHECK(compileAndRunCode(code, {"40"}, "mixed_abi"));
}

TEST_CASE("ABI & Calling Convention: Callee-saved register preservation across early return") {
    std::string code =
        "<?php\n"
        "fn callee(mode: i32): i32 {\n"
        "    let x1: i32 = 100;\n"
        "    let x2: i32 = 200;\n"
        "    let x3: i32 = 300;\n"
        "    let x4: i32 = 400;\n"
        "    let x5: i32 = 500;\n"
        "    if (mode == 1) {\n"
        "        return x1 + x2;\n"
        "    }\n"
        "    return x3 + x4 + x5;\n"
        "}\n"
        "\n"
        "fn main(): i32 {\n"
        "    let r1: i32 = callee(1);\n" // 300
        "    let r2: i32 = callee(2);\n" // 1200
        "    println(r1);\n"
        "    println(r2);\n"
        "    return 0;\n"
        "}\n";

    CHECK(compileAndRunCode(code, {"300", "1200"}, "callee_save_ret"));
}

TEST_CASE("ABI & Calling Convention: Deep recursive calls with 16-byte stack alignment") {
    std::string code =
        "<?php\n"
        "fn factorial(n: i32): i32 {\n"
        "    if (n <= 1) {\n"
        "        return 1;\n"
        "    }\n"
        "    return n * factorial(n - 1);\n"
        "}\n"
        "\n"
        "fn main(): i32 {\n"
        "    let f7: i32 = factorial(7);\n" // 5040
        "    println(f7);\n"
        "    return 0;\n"
        "}\n";

    CHECK(compileAndRunCode(code, {"5040"}, "recursive_align"));
}

