#include "compiler/codegen/Linker.hpp"
#include "compiler/codegen/llvm/LLVMEmitter.hpp"
#include "compiler/codegen/x86_64/X86_64CodeGen.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/ir/IRGenerator.hpp"
#include "compiler/ir/IRVerifier.hpp"
#include "compiler/optimizer/PassManager.hpp"
#include "compiler/parser/Parser.hpp"
#include "compiler/semantic/SemanticAnalyzer.hpp"
#include "compiler/source/SourceManager.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Prng {
    uint64_t state;
    explicit Prng(uint64_t seed) : state(seed ? seed : 0x9E3779B97F4A7C15ULL) {}
    uint32_t next() {
        uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return static_cast<uint32_t>(z ^ (z >> 31));
    }
    int range(int lo, int hi) {
        if (hi <= lo) return lo;
        return lo + static_cast<int>(next() % static_cast<uint32_t>(hi - lo + 1));
    }
};

static std::string synthesizeProgram(uint64_t seed) {
    Prng rng(seed);
    std::ostringstream os;
    int a = rng.range(1, 19);
    int b = rng.range(2, 13);
    int k = rng.range(2, 9);
    int limit = rng.range(4, 16);
    int depth = rng.range(2, 6);
    int op = rng.range(0, 3);
    std::string arithExpr;
    if (op == 0) arithExpr = "(x * " + std::to_string(k) + ") + (y * " + std::to_string(k) + ")";
    else if (op == 1) arithExpr = "((x + y) ^ (x * " + std::to_string(k) + ")) & 1023";
    else if (op == 2) arithExpr = "(x * y) - (y * " + std::to_string(a) + ")";
    else arithExpr = "(x << 2) + (y * " + std::to_string(b) + ")";

    os << "<?php\n"
       << "struct DiffNode {\n"
       << "    v: i32,\n"
       << "    w: i32,\n"
       << "}\n"
       << "fn mix6(a: i32, b: i32, c: i32, d: i32, e: i32, f: i32): i32 {\n"
       << "    mut acc: i32 = (a + b * 2) - (c + d);\n"
       << "    if ((e & 1) == 0) { acc = acc + (f * 3); } else { acc = acc - f; }\n"
       << "    return acc;\n"
       << "}\n"
       << "fn rec_fold(n: i32, acc: i32): i32 {\n"
       << "    if (n <= 0) { return acc; }\n"
       << "    return rec_fold(n - 1, acc + (n * " + std::to_string(a) + "));\n"
       << "}\n"
       << "fn main(): i32 {\n"
       << "    mut total: i32 = " << rng.range(0, 50) << ";\n"
       << "    mut i: i32 = 0;\n"
       << "    while (i < " << limit << ") {\n"
       << "        let node = Box::new(DiffNode { v: i + " << a << ", w: i * " << b << " });\n"
       << "        mut j: i32 = 0;\n"
       << "        while (j < " << depth << ") {\n"
       << "            let x: i32 = node.v + j;\n"
       << "            let y: i32 = node.w + " << b << ";\n"
       << "            total = total + (" << arithExpr << ");\n"
       << "            j = j + 1;\n"
       << "        }\n"
       << "        if ((i % 3) == 1) {\n"
       << "            total = total + mix6(i, node.v, node.w, " << a << ", " << b << ", " << k << ");\n"
       << "        }\n"
       << "        i = i + 1;\n"
       << "    }\n"
       << "    total = total + rec_fold(" << rng.range(3, 12) << ", " << rng.range(1, 9) << ");\n"
       << "    println(total);\n"
       << "    return 0;\n"
       << "}\n";
    return os.str();
}

static std::filesystem::path findCoreRuntimeArchive() {
    std::vector<std::filesystem::path> candidates = {
        "build/lib/libsph_core.a",
        "lib/libsph_core.a",
        "../lib/libsph_core.a",
        "build/lib/libsph_runtime.a",
        "lib/libsph_runtime.a",
    };
    std::error_code ec;
    for (const auto& c : candidates) {
        if (std::filesystem::exists(c, ec)) return c;
    }
    return {};
}

static bool verifyInMemoryPipeline(const std::string& source, const std::string& name, std::string& errOut) {
    sph::SourceManager sm;
    uint32_t fileId = sm.addSource(name, source);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    if (!program || emitter.hasErrors()) {
        errOut = "parse failed";
        return false;
    }

    sph::SemanticAnalyzer analyzer(emitter);
    if (!analyzer.analyze(*program) || emitter.hasErrors()) {
        errOut = "semantic analysis failed";
        return false;
    }

    const sph::OptimizationLevel levels[] = {
        sph::OptimizationLevel::O0,
        sph::OptimizationLevel::O1,
        sph::OptimizationLevel::O2,
        sph::OptimizationLevel::O3,
    };

    for (auto opt : levels) {
        sph::IRGenerator irGen;
        auto mod = irGen.generate(*program, name);
        if (!mod) {
            errOut = "IR generation failed";
            return false;
        }
        sph::IRVerifier baselineVerifier;
        std::vector<std::string> verifyErrors;
        if (!baselineVerifier.verify(*mod, verifyErrors)) {
            errOut = verifyErrors.empty() ? "baseline IR verification failed" : verifyErrors.front();
            return false;
        }

        auto pm = sph::PassManager::createForLevel(opt);
        pm.setVerifyEachPass(true);
        (void)pm.run(*mod);
        if (pm.hasVerificationErrors()) {
            errOut = "PassManager verifyEachPass failed";
            return false;
        }
        verifyErrors.clear();
        if (!baselineVerifier.verify(*mod, verifyErrors)) {
            errOut = verifyErrors.empty() ? "post-pass IR verification failed" : verifyErrors.front();
            return false;
        }

        std::ostringstream llvmOut;
        sph::LLVMEmitter llvmEmitter(llvmOut);
        llvmEmitter.emit(*mod);
        if (llvmOut.str().empty()) {
            errOut = "empty LLVM IR output";
            return false;
        }

        std::ostringstream asmOut;
        sph::X86_64CodeGen codegen(asmOut);
        codegen.setOptimizationLevel(opt);
        codegen.generate(*mod);
        if (asmOut.str().empty()) {
            errOut = "empty x86_64 assembly output";
            return false;
        }
    }
    return true;
}

static bool compileAndCaptureOutput(const std::string& source,
                                    sph::OptimizationLevel opt,
                                    const std::filesystem::path& runtimeLib,
                                    const std::filesystem::path& workDir,
                                    const std::string& tag,
                                    std::string& stdoutOut,
                                    int& exitOut,
                                    std::string& errOut) {
    sph::SourceManager sm;
    uint32_t fileId = sm.addSource(tag + ".sph", source);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    if (!program || emitter.hasErrors()) {
        errOut = "parse error in native diff";
        return false;
    }
    sph::SemanticAnalyzer analyzer(emitter);
    if (!analyzer.analyze(*program) || emitter.hasErrors()) {
        errOut = "semantic error in native diff";
        return false;
    }
    sph::IRGenerator irGen;
    auto mod = irGen.generate(*program, tag);
    if (!mod) {
        errOut = "IR gen failed";
        return false;
    }
    auto pm = sph::PassManager::createForLevel(opt);
    pm.setVerifyEachPass(true);
    (void)pm.run(*mod);
    if (pm.hasVerificationErrors()) {
        errOut = "PassManager verify failed";
        return false;
    }

    auto asmPath = workDir / (tag + ".s");
    auto exePath = workDir / (tag + sph::Target::getHostTarget().getExecutableExtension());
    auto logPath = workDir / (tag + ".out");

    {
        std::ofstream os(asmPath);
        sph::X86_64CodeGen codegen(os);
        codegen.setOptimizationLevel(opt);
        codegen.generate(*mod);
    }

    sph::Linker linker;
    std::string linkErr;
    bool ok = linker.linkExecutable(asmPath, exePath, runtimeLib, linkErr);
    std::error_code ec;
    std::filesystem::remove(asmPath, ec);
    if (!ok) {
        errOut = "link failed: " + linkErr;
        return false;
    }

#if defined(_WIN32)
    std::string cmd = "cmd /c \"\"" + exePath.string() + "\" > \"" + logPath.string() + "\"\"";
#else
    std::string cmd = "\"" + exePath.string() + "\" > \"" + logPath.string() + "\"";
#endif
    exitOut = std::system(cmd.c_str());

    std::ifstream ifs(logPath);
    std::ostringstream ss;
    ss << ifs.rdbuf();
    stdoutOut = ss.str();
    std::filesystem::remove(exePath, ec);
    std::filesystem::remove(logPath, ec);
    if (exitOut != 0 || stdoutOut.empty()) {
        errOut = "executable returned rc=" + std::to_string(exitOut) + " out='" + stdoutOut + "'";
        return false;
    }
    return true;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (!data || size == 0 || size > 65536) return 0;

    // 1. Raw crash/robustness check on arbitrary bytes
    try {
        std::string raw(reinterpret_cast<const char*>(data), size);
        sph::SourceManager sm;
        uint32_t fileId = sm.addSource("fuzz_raw.sph", raw);
        sph::DiagnosticEmitter emitter(sm);
        sph::Parser parser(fileId, sm, &emitter);
        auto program = parser.parseProgram();
        if (program && !emitter.hasErrors()) {
            sph::SemanticAnalyzer analyzer(emitter);
            if (analyzer.analyze(*program) && !emitter.hasErrors()) {
                std::string err;
                verifyInMemoryPipeline(raw, "fuzz_raw.sph", err);
            }
        }
    } catch (...) {
        // Parser/semantic error handling must never abort.
    }

    // 2. Seeded structured IR/codegen differential fuzz check
    uint64_t seed = 14695981039346656037ULL;
    for (size_t i = 0; i < size && i < 64; ++i) {
        seed ^= static_cast<uint64_t>(data[i]);
        seed *= 1099511628211ULL;
    }
    std::string syn = synthesizeProgram(seed);
    std::string err;
    if (!verifyInMemoryPipeline(syn, "fuzz_syn.sph", err)) {
        std::cerr << "[sph_diff_fuzz] IR/Codegen invariant failure: " << err << "\n";
        std::abort();
    }
    return 0;
}

#ifndef LIBFUZZER_ENTRY
int main(int argc, char** argv) {
    int iterations = 100;
    int nativeChecks = 8;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--iterations" && i + 1 < argc) {
            iterations = std::max(1, std::atoi(argv[++i]));
        } else if (arg == "--native" && i + 1 < argc) {
            nativeChecks = std::max(0, std::atoi(argv[++i]));
        } else if (arg.rfind("--iterations=", 0) == 0) {
            iterations = std::max(1, std::atoi(arg.substr(13).c_str()));
        } else if (arg.rfind("--native=", 0) == 0) {
            nativeChecks = std::max(0, std::atoi(arg.substr(9).c_str()));
        } else {
            std::ifstream ifs(arg, std::ios::binary);
            if (ifs) {
                std::vector<uint8_t> buf((std::istreambuf_iterator<char>(ifs)),
                                         std::istreambuf_iterator<char>());
                return LLVMFuzzerTestOneInput(buf.data(), buf.size());
            }
        }
    }

    std::cout << "[sph_diff_fuzz] Running " << iterations << " IR/pass/emitter fuzz iterations...\n";
    for (int i = 0; i < iterations; ++i) {
        std::string prog = synthesizeProgram(0xA5A50000ULL + static_cast<uint64_t>(i));
        std::string err;
        if (!verifyInMemoryPipeline(prog, "diff_iter_" + std::to_string(i) + ".sph", err)) {
            std::cerr << "[sph_diff_fuzz] FAIL at iteration " << i << ": " << err << "\n";
            return 1;
        }
    }
    std::cout << "[sph_diff_fuzz] All " << iterations << " IR/VerifyEachPass/CodeGen iterations passed.\n";

    auto runtimeLib = findCoreRuntimeArchive();
    if (nativeChecks > 0 && !runtimeLib.empty()) {
        std::filesystem::path workDir = std::filesystem::temp_directory_path() / "sph_diff_fuzz";
        std::error_code ec;
        std::filesystem::create_directories(workDir, ec);
        std::cout << "[sph_diff_fuzz] Running " << nativeChecks << " -O0 vs -O3 native differential checks...\n";
        for (int i = 0; i < nativeChecks; ++i) {
            std::string prog = synthesizeProgram(0xC0DE0000ULL + static_cast<uint64_t>(i));
            std::string out0, out3, err0, err3;
            int rc0 = -1, rc3 = -1;
            if (!compileAndCaptureOutput(prog, sph::OptimizationLevel::O0, runtimeLib, workDir,
                                         "diff_" + std::to_string(i) + "_o0", out0, rc0, err0) ||
                !compileAndCaptureOutput(prog, sph::OptimizationLevel::O3, runtimeLib, workDir,
                                         "diff_" + std::to_string(i) + "_o3", out3, rc3, err3)) {
                std::cerr << "[sph_diff_fuzz] Native differential compile failed (" << err0 << " / " << err3 << ")\n";
                std::filesystem::remove_all(workDir, ec);
                return 1;
            }
            if (rc0 != rc3 || out0 != out3) {
                std::cerr << "[sph_diff_fuzz] DIFFERENTIAL MISMATCH on case " << i << "!\n"
                          << "  O0 rc=" << rc0 << " out=" << out0 << "\n"
                          << "  O3 rc=" << rc3 << " out=" << out3 << "\n";
                std::filesystem::remove_all(workDir, ec);
                return 1;
            }
        }
        std::filesystem::remove_all(workDir, ec);
        std::cout << "[sph_diff_fuzz] All " << nativeChecks << " native -O0 vs -O3 checks matched.\n";
    } else if (nativeChecks > 0) {
        std::cout << "[sph_diff_fuzz] Skipping native binary execution (libsph_core.a not found in cwd).\n";
    }

    return 0;
}
#endif
