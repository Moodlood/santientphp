#include "common/test_framework.hpp"
#include "compiler/codegen/Linker.hpp"
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

bool compileAndRunSPH(const std::string& code, const std::vector<std::string>& expectedOutputLines, const std::string& testTag) {
    sph::SourceManager sm;
    uint32_t fileId = sm.addSource("stress_" + testTag + ".sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    if (!program) return false;

    sph::SemanticAnalyzer analyzer(emitter);
    if (!analyzer.analyze(*program)) return false;

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "stress_" + testTag);
    if (!irModule) return false;

    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O3);
    pm.run(*irModule);

    std::filesystem::path tempAsm = "test_stress_" + testTag + ".s";
    std::filesystem::path tempExe = "test_stress_" + testTag + ".exe";
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
        return false;
    }

    std::string outputLog = "test_stress_" + testTag + "_out.txt";
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

    if (runRes != 0) return false;
    if (actualLines.size() < expectedOutputLines.size()) return false;
    for (size_t i = 0; i < expectedOutputLines.size(); ++i) {
        if (actualLines[i] != expectedOutputLines[i]) return false;
    }
    return true;
}

} // namespace

TEST_CASE("Stress 01: TCO 6-argument cyclic permutation across 1000 iterations") {
    std::string code =
        "<?php\n"
        "fn cycle6(a: i32, b: i32, c: i32, d: i32, e: i32, f: i32, steps: i32): i32 {\n"
        "    if (steps == 0) {\n"
        "        return a * 100000 + b * 10000 + c * 1000 + d * 100 + e * 10 + f;\n"
        "    }\n"
        "    return cycle6(b, c, d, e, f, a, steps - 1);\n"
        "}\n"
        "fn main(): i32 {\n"
        "    let r: i32 = cycle6(1, 2, 3, 4, 5, 6, 1000);\n"
        "    println(r);\n"
        "    return 0;\n"
        "}\n";
    CHECK(compileAndRunSPH(code, {"561234"}, "01_cycle6"));
}

TEST_CASE("Stress 02: TCO pairwise swap non-clobbering gcd") {
    std::string code =
        "<?php\n"
        "fn gcd_tail(a: i32, b: i32): i32 {\n"
        "    if (b == 0) {\n"
        "        return a;\n"
        "    }\n"
        "    return gcd_tail(b, a % b);\n"
        "}\n"
        "fn main(): i32 {\n"
        "    let g: i32 = gcd_tail(1071, 462);\n"
        "    println(g);\n"
        "    return 0;\n"
        "}\n";
    CHECK(compileAndRunSPH(code, {"21"}, "02_gcd"));
}

TEST_CASE("Stress 03: TCO tail-recursive Fibonacci with two accumulators") {
    std::string code =
        "<?php\n"
        "fn fib_acc(n: i32, a: i32, b: i32): i32 {\n"
        "    if (n == 0) {\n"
        "        return a;\n"
        "    }\n"
        "    return fib_acc(n - 1, b, a + b);\n"
        "}\n"
        "fn main(): i32 {\n"
        "    let r: i32 = fib_acc(30, 0, 1);\n"
        "    println(r);\n"
        "    return 0;\n"
        "}\n";
    CHECK(compileAndRunSPH(code, {"832040"}, "03_fib_acc"));
}

TEST_CASE("Stress 04: TCO with state accumulation across 100 frames") {
    std::string code =
        "<?php\n"
        "fn step_state(acc: i32, count: i32): i32 {\n"
        "    if (count == 0) {\n"
        "        return acc;\n"
        "    }\n"
        "    return step_state(acc + count, count - 1);\n"
        "}\n"
        "fn main(): i32 {\n"
        "    let r: i32 = step_state(0, 100);\n"
        "    println(r);\n"
        "    return 0;\n"
        "}\n";
    CHECK(compileAndRunSPH(code, {"5050"}, "04_step_state"));
}

TEST_CASE("Stress 05: Defer across multiple early return branches") {
    std::string code =
        "<?php\n"
        "fn early_defer(code: i32): i32 {\n"
        "    defer println(10);\n"
        "    if (code < 0) {\n"
        "        defer println(1);\n"
        "        return 100;\n"
        "    }\n"
        "    if (code == 0) {\n"
        "        defer println(2);\n"
        "        return 200;\n"
        "    }\n"
        "    defer println(3);\n"
        "    return 300;\n"
        "}\n"
        "fn main(): i32 {\n"
        "    let r1 = early_defer(-5);\n"
        "    println(r1);\n"
        "    let r2 = early_defer(0);\n"
        "    println(r2);\n"
        "    let r3 = early_defer(5);\n"
        "    println(r3);\n"
        "    return 0;\n"
        "}\n";
    CHECK(compileAndRunSPH(code, {"1", "10", "100", "2", "10", "200", "3", "10", "300"}, "05_early_defer"));
}

TEST_CASE("Stress 06: Defer with LIFO order on early return") {
    std::string code =
        "<?php\n"
        "fn test_lifo(n: i32): i32 {\n"
        "    defer println(10);\n"
        "    defer println(20);\n"
        "    if (n > 0) {\n"
        "        defer println(30);\n"
        "        return 1;\n"
        "    }\n"
        "    return 0;\n"
        "}\n"
        "fn main(): i32 {\n"
        "    let r = test_lifo(5);\n"
        "    println(r);\n"
        "    return 0;\n"
        "}\n";
    CHECK(compileAndRunSPH(code, {"30", "20", "10", "1"}, "06_lifo_defer"));
}

TEST_CASE("Stress 07: LSRA register pressure with 12 live variables across helper call") {
    std::string code =
        "<?php\n"
        "fn helper_mul(x: i32): i32 {\n"
        "    return x * 2;\n"
        "}\n"
        "fn compute_heavy(): i32 {\n"
        "    let a1: i32 = 1;\n"
        "    let a2: i32 = 2;\n"
        "    let a3: i32 = 3;\n"
        "    let a4: i32 = 4;\n"
        "    let a5: i32 = 5;\n"
        "    let a6: i32 = 6;\n"
        "    let a7: i32 = 7;\n"
        "    let a8: i32 = 8;\n"
        "    let a9: i32 = 9;\n"
        "    let a10: i32 = 10;\n"
        "    let a11: i32 = 11;\n"
        "    let a12: i32 = 12;\n"
        "    let mid = helper_mul(100);\n"
        "    return a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9 + a10 + a11 + a12 + mid;\n"
        "}\n"
        "fn main(): i32 {\n"
        "    println(compute_heavy());\n"
        "    return 0;\n"
        "}\n";
    CHECK(compileAndRunSPH(code, {"278"}, "07_heavy_lsra"));
}

TEST_CASE("Stress 08: LSRA Callee-saved register preservation with early returns") {
    std::string code =
        "<?php\n"
        "fn heavy_early(n: i32): i32 {\n"
        "    let a1: i32 = 10;\n"
        "    let a2: i32 = 20;\n"
        "    let a3: i32 = 30;\n"
        "    let a4: i32 = 40;\n"
        "    let a5: i32 = 50;\n"
        "    let a6: i32 = 60;\n"
        "    if (n < 0) {\n"
        "        return a1 + a2;\n"
        "    }\n"
        "    if (n == 0) {\n"
        "        return a3 + a4;\n"
        "    }\n"
        "    return a1 + a2 + a3 + a4 + a5 + a6;\n"
        "}\n"
        "fn main(): i32 {\n"
        "    println(heavy_early(-1));\n"
        "    println(heavy_early(0));\n"
        "    println(heavy_early(1));\n"
        "    return 0;\n"
        "}\n";
    CHECK(compileAndRunSPH(code, {"30", "70", "210"}, "08_callee_saved_early"));
}

TEST_CASE("Stress 09: Mem2Reg 3-level nested loop with breaks and phis") {
    std::string code =
        "<?php\n"
        "fn nested_loops(n: i32): i32 {\n"
        "    mut sum: i32 = 0;\n"
        "    mut i: i32 = 0;\n"
        "    while (i < n) {\n"
        "        mut j: i32 = 0;\n"
        "        while (j < n) {\n"
        "            mut k: i32 = 0;\n"
        "            while (k < n) {\n"
        "                if (k == 3) {\n"
        "                    break;\n"
        "                }\n"
        "                sum = sum + 1;\n"
        "                k = k + 1;\n"
        "            } \n"
        "            j = j + 1;\n"
        "        }\n"
        "        i = i + 1;\n"
        "    }\n"
        "    return sum;\n"
        "}\n"
        "fn main(): i32 {\n"
        "    println(nested_loops(5));\n"
        "    return 0;\n"
        "}\n";
    CHECK(compileAndRunSPH(code, {"75"}, "09_nested_loops"));
}

TEST_CASE("Stress 10: Mem2Reg diamond phi merge with 6 variables") {
    std::string code =
        "<?php\n"
        "fn diamond6(flag: i32): i32 {\n"
        "    mut a: i32 = 1;\n"
        "    mut b: i32 = 2;\n"
        "    mut c: i32 = 3;\n"
        "    mut d: i32 = 4;\n"
        "    mut e: i32 = 5;\n"
        "    mut f: i32 = 6;\n"
        "    if (flag > 0) {\n"
        "        a = a + 10;\n"
        "        b = b + 20;\n"
        "        c = c + 30;\n"
        "        d = d + 40;\n"
        "        e = e + 50;\n"
        "        f = f + 60;\n"
        "    } else {\n"
        "        a = a * 2;\n"
        "        b = b * 2;\n"
        "        c = c * 2;\n"
        "        d = d * 2;\n"
        "        e = e * 2;\n"
        "        f = f * 2;\n"
        "    }\n"
        "    return a + b + c + d + e + f;\n"
        "}\n"
        "fn main(): i32 {\n"
        "    println(diamond6(1));\n"
        "    println(diamond6(0));\n"
        "    return 0;\n"
        "}\n";
    CHECK(compileAndRunSPH(code, {"231", "42"}, "10_diamond6"));
}

TEST_CASE("Stress 11: Escape analysis demotion in loop with conditional early return") {
    std::string code =
        "<?php\n"
        "struct Item {\n"
        "    val: i32,\n"
        "}\n"
        "fn process_items(limit: i32): i32 {\n"
        "    mut i: i32 = 0;\n"
        "    mut sum: i32 = 0;\n"
        "    while (i < 100) {\n"
        "        let b = Box::new(Item { val: i * 2 });\n"
        "        if (b.val > limit) {\n"
        "            return sum;\n"
        "        }\n"
        "        sum = sum + b.val;\n"
        "        i = i + 1;\n"
        "    }\n"
        "    return sum;\n"
        "}\n"
        "fn main(): i32 {\n"
        "    println(process_items(10));\n"
        "    return 0;\n"
        "}\n";
    CHECK(compileAndRunSPH(code, {"30"}, "11_escape_early_ret"));
}

TEST_CASE("Stress 12: Escape analysis demotion struct member mutation across blocks") {
    std::string code =
        "<?php\n"
        "struct Counter {\n"
        "    c: i32,\n"
        "}\n"
        "fn mutate_box(): i32 {\n"
        "    mut b = Box::new(Counter { c: 10 });\n"
        "    mut i: i32 = 0;\n"
        "    while (i < 5) {\n"
        "        b.c = b.c + i;\n"
        "        i = i + 1;\n"
        "    }\n"
        "    return b.c;\n"
        "}\n"
        "fn main(): i32 {\n"
        "    println(mutate_box());\n"
        "    return 0;\n"
        "}\n";
    CHECK(compileAndRunSPH(code, {"20"}, "12_escape_struct_mutation"));
}

TEST_CASE("Stress 13: Slab allocator churn and recycling under 10000 cycles") {
    std::string code =
        "<?php\n"
        "struct Node64 {\n"
        "    v1: i64,\n"
        "    v2: i64,\n"
        "    v3: i64,\n"
        "}\n"
        "fn main(): i32 {\n"
        "    mut i: i32 = 0;\n"
        "    mut total: i64 = 0;\n"
        "    while (i < 10000) {\n"
        "        let n = Box::new(Node64 { v1: 1 as i64, v2: 2 as i64, v3: 3 as i64 });\n"
        "        total = total + n.v1 + n.v2 + n.v3;\n"
        "        i = i + 1;\n"
        "    }\n"
        "    println(total);\n"
        "    return 0;\n"
        "}\n";
    CHECK(compileAndRunSPH(code, {"60000"}, "13_slab_churn"));
}

TEST_CASE("Stress 14: BorrowChecker NLL loop disjoint borrows") {
    std::string code =
        "<?php\n"
        "fn use_val(val: *i32): i32 {\n"
        "    unsafe {\n"
        "        return *val * 2;\n"
        "    }\n"
        "}\n"
        "fn main(): i32 {\n"
        "    mut x: i32 = 100;\n"
        "    mut i: i32 = 0;\n"
        "    while (i < 5) {\n"
        "        let r: *i32 = &x;\n"
        "        let v = use_val(r);\n"
        "        x = x + v;\n"
        "        i = i + 1;\n"
        "    }\n"
        "    println(x);\n"
        "    return 0;\n"
        "}\n";
    CHECK(compileAndRunSPH(code, {"24300"}, "14_nll_loop"));
}

TEST_CASE("Stress 15: BorrowChecker NLL branch-disjoint mutable borrows") {
    std::string code =
        "<?php\n"
        "fn branch_borrow(flag: bool): i32 {\n"
        "    mut x: i32 = 50;\n"
        "    if (flag) {\n"
        "        let m1: *i32 = &mut x;\n"
        "        unsafe { *m1 = *m1 + 10; }\n"
        "    } else {\n"
        "        let m2: *i32 = &mut x;\n"
        "        unsafe { *m2 = *m2 + 20; }\n"
        "    }\n"
        "    return x;\n"
        "}\n"
        "fn main(): i32 {\n"
        "    println(branch_borrow(true));\n"
        "    println(branch_borrow(false));\n"
        "    return 0;\n"
        "}\n";
    CHECK(compileAndRunSPH(code, {"60", "70"}, "15_nll_branch"));
}

TEST_CASE("Stress 16: Internal fast calling convention passing 6 integer arguments") {
    std::string code =
        "<?php\n"
        "fn sum6(a: i32, b: i32, c: i32, d: i32, e: i32, f: i32): i32 {\n"
        "    return a + b + c + d + e + f;\n"
        "}\n"
        "fn main(): i32 {\n"
        "    println(sum6(10, 20, 30, 40, 50, 60));\n"
        "    return 0;\n"
        "}\n";
    CHECK(compileAndRunSPH(code, {"210"}, "16_fast_abi6"));
}

TEST_CASE("Stress 17: Internal fast calling convention mixed scalar and float types") {
    std::string code =
        "<?php\n"
        "fn mixed_calc(a: i32, b: f32, c: i32, d: f32): f32 {\n"
        "    return (a as f32) + b + (c as f32) + d;\n"
        "}\n"
        "fn main(): i32 {\n"
        "    let res = mixed_calc(10, 2.5 as f32, 20, 7.5 as f32);\n"
        "    println(res as i32);\n"
        "    return 0;\n"
        "}\n";
    CHECK(compileAndRunSPH(code, {"40"}, "17_mixed_abi"));
}

TEST_CASE("Stress 18: Exception throw and catch across TCO recursive frames") {
    std::string code =
        "<?php\n"
        "fn rec_throw(n: i32): i32 {\n"
        "    if (n == 0) {\n"
        "        throw \"unwind from tail loop\";\n"
        "    }\n"
        "    return rec_throw(n - 1);\n"
        "}\n"
        "fn main(): i32 {\n"
        "    try {\n"
        "        rec_throw(20);\n"
        "    } catch ($e) {\n"
        "        println(999);\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    CHECK(compileAndRunSPH(code, {"999"}, "18_exception_tco"));
}

TEST_CASE("Stress 19: Mutually recursive tail calls") {
    std::string code =
        "<?php\n"
        "fn is_even(n: i32): bool {\n"
        "    if (n == 0) { return true; }\n"
        "    return is_odd(n - 1);\n"
        "}\n"
        "fn is_odd(n: i32): bool {\n"
        "    if (n == 0) { return false; }\n"
        "    return is_even(n - 1);\n"
        "}\n"
        "fn main(): i32 {\n"
        "    if (is_even(100)) { println(1); } else { println(0); }\n"
        "    if (is_odd(100)) { println(1); } else { println(0); }\n"
        "    return 0;\n"
        "}\n";
    CHECK(compileAndRunSPH(code, {"1", "0"}, "19_mutual_rec"));
}

TEST_CASE("Stress 20: Full pipeline stress matrix alloc TCO defer combined") {
    std::string code =
        "<?php\n"
        "struct Point {\n"
        "    x: i32,\n"
        "    y: i32,\n"
        "}\n"
        "fn transform_tail(x: i32, y: i32, steps: i32): i32 {\n"
        "    if (steps == 0) {\n"
        "        return x + y;\n"
        "    }\n"
        "    let p = Box::new(Point { x: x + 1, y: y + 2 });\n"
        "    return transform_tail(p.x, p.y, steps - 1);\n"
        "}\n"
        "fn main(): i32 {\n"
        "    defer println(42);\n"
        "    let res = transform_tail(0, 0, 100);\n"
        "    println(res);\n"
        "    return 0;\n"
        "}\n";
    CHECK(compileAndRunSPH(code, {"300", "42"}, "20_full_pipeline"));
}
