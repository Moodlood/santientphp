#include "common/test_framework.hpp"
#include "compiler/codegen/Linker.hpp"
#include "compiler/codegen/llvm/LLVMEmitter.hpp"
#include "compiler/codegen/x86_64/X86_64CodeGen.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/ir/IRGenerator.hpp"
#include "compiler/ir/IRVerifier.hpp"
#include "compiler/parser/Parser.hpp"
#include "compiler/semantic/SemanticAnalyzer.hpp"
#include "compiler/source/SourceManager.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace sph;

TEST_CASE("Debugger: SourceLocation propagation from AST to IR instructions") {
    SourceManager sm;
    std::string code =
        "<?php\n"
        "fn calculate(a: i32, b: i32): i32 {\n"
        "    let x: i32 = a + 10;\n"
        "    let y: i32 = b * 2;\n"
        "    return x + y;\n"
        "}\n";

    uint32_t fileId = sm.addSource("calc.sph", code);
    DiagnosticEmitter emitter(sm);
    Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);
    CHECK(!emitter.hasErrors());

    SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    IRGenerator irGen;
    auto irModule = irGen.generate(*program, "calc_module", &sm);
    CHECK(irModule != nullptr);

    // Verify source files are recorded in module
    CHECK_EQ(irModule->getSourceFiles().size(), size_t(1));
    CHECK_EQ(irModule->getSourceFile(fileId), "calc.sph");

    auto* fn = irModule->getFunction("calculate");
    CHECK(fn != nullptr);
    if (!fn) return;

    // Verify function location
    CHECK(fn->getLocation().isValid());
    CHECK_EQ(fn->getLocation().line, 2u);

    // Verify instructions in entry block carry valid source line numbers
    bool hasLine3 = false;
    bool hasLine4 = false;
    bool hasLine5 = false;

    for (const auto& bb : fn->getBlocks()) {
        for (const auto& inst : bb->getInstructions()) {
            if (inst->getLocation().isValid()) {
                if (inst->getLocation().line == 3) hasLine3 = true;
                if (inst->getLocation().line == 4) hasLine4 = true;
                if (inst->getLocation().line == 5) hasLine5 = true;
            }
        }
    }

    CHECK(hasLine3);
    CHECK(hasLine4);
    CHECK(hasLine5);
}

TEST_CASE("Debugger: X86_64CodeGen emits DWARF .file, .loc, and CFI directives") {
    SourceManager sm;
    std::string code =
        "<?php\n"
        "fn simple(x: i32): i32 {\n"
        "    return x + 42;\n"
        "}\n";

    uint32_t fileId = sm.addSource("simple.sph", code);
    DiagnosticEmitter emitter(sm);
    Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    IRGenerator irGen;
    auto irModule = irGen.generate(*program, "simple_module", &sm);
    CHECK(irModule != nullptr);

    // 1. Without debug info
    {
        std::ostringstream asmOut;
        X86_64CodeGen codegen(asmOut);
        codegen.setDebugInfo(false);
        codegen.generate(*irModule);
        std::string s = asmOut.str();
        CHECK(s.find(".file") == std::string::npos);
        CHECK(s.find(".loc") == std::string::npos);
        CHECK(s.find(".cfi_startproc") == std::string::npos);
    }

    // 2. With debug info
    {
        std::ostringstream asmOut;
        X86_64CodeGen codegen(asmOut);
        codegen.setDebugInfo(true);
        codegen.generate(*irModule);
        std::string s = asmOut.str();

        CHECK(s.find(".file 1 \"simple.sph\"") != std::string::npos);
        CHECK(s.find(".cfi_startproc") != std::string::npos);
        CHECK(s.find(".cfi_def_cfa_offset 16") != std::string::npos);
        CHECK(s.find(".cfi_offset rbp, -16") != std::string::npos);
        CHECK(s.find(".cfi_def_cfa_register rbp") != std::string::npos);
        CHECK(s.find(".cfi_endproc") != std::string::npos);
        CHECK(s.find(".loc 1 2") != std::string::npos);
        CHECK(s.find(".loc 1 3") != std::string::npos);
    }
}

TEST_CASE("Debugger: LLVMEmitter emits debug metadata and location annotations") {
    SourceManager sm;
    std::string code =
        "<?php\n"
        "fn test_llvm_dbg(v: i32): i32 {\n"
        "    return v;\n"
        "}\n";

    uint32_t fileId = sm.addSource("dbg.sph", code);
    DiagnosticEmitter emitter(sm);
    Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    IRGenerator irGen;
    auto irModule = irGen.generate(*program, "dbg_module", &sm);
    CHECK(irModule != nullptr);

    std::ostringstream llvmOut;
    LLVMEmitter emitterLLVM(llvmOut);
    emitterLLVM.setDebugInfo(true);
    emitterLLVM.emit(*irModule);
    std::string s = llvmOut.str();

    CHECK(s.find("!llvm.dbg.cu") != std::string::npos);
    CHECK(s.find("!DICompileUnit") != std::string::npos);
    CHECK(s.find("!DIFile") != std::string::npos);
    CHECK(s.find("; !dbg") != std::string::npos);
}

TEST_CASE("End-to-End Acceptance Test: Compiles debug_example.sph with -g producing DWARF debug sections and GDB line resolution (Milestone 11E)") {
    SourceManager sm;
    std::string examplePath = "examples/debug_example.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    DiagnosticEmitter emitter(sm);
    Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);
    CHECK(!emitter.hasErrors());

    SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));
    CHECK(!emitter.hasErrors());

    IRGenerator irGen;
    auto module = irGen.generate(*program, "debug_test", &sm);
    CHECK(module != nullptr);

    IRVerifier verifier;
    std::vector<std::string> vErrors;
    CHECK(verifier.verify(*module, vErrors));

    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib)) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    std::filesystem::path tempAsm = "test_debugger_e2e.s";
    std::filesystem::path tempExe = "test_debugger_e2e.exe";

    std::ofstream asmOut(tempAsm);
    X86_64CodeGen codegen(asmOut);
    codegen.setDebugInfo(true);
    codegen.generate(*module);
    asmOut.close();

    CHECK(std::filesystem::exists(tempAsm));

    // Link with debug info (-g) enabled
    Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg, "", true);
    std::error_code ec;
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    // 1. Run executable and verify computation (compute_sum(10) -> 55)
    std::string outputLog = "test_debugger_out.txt";
    std::string runCmd = ".\\" + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::string firstLine;
    if (std::getline(outFile, firstLine)) {
        if (!firstLine.empty() && firstLine.back() == '\r') firstLine.pop_back();
        CHECK_EQ(firstLine, "55");
    }
    outFile.close();
    std::filesystem::remove(outputLog, ec);

    // 2. Inspect binary sections with objdump -h to verify DWARF debug sections are present
    std::string objdumpLog = "test_debugger_objdump.txt";
    std::string objdumpCmd = "objdump -h " + tempExe.string() + " > " + objdumpLog;
    int objRes = std::system(objdumpCmd.c_str());
    CHECK_EQ(objRes, 0);

    std::ifstream objdumpFile(objdumpLog);
    std::string objdumpContent((std::istreambuf_iterator<char>(objdumpFile)), std::istreambuf_iterator<char>());
    objdumpFile.close();
    std::filesystem::remove(objdumpLog, ec);

    CHECK(objdumpContent.find(".debug_line") != std::string::npos);
    CHECK(objdumpContent.find(".debug_info") != std::string::npos);
    CHECK(objdumpContent.find(".debug_frame") != std::string::npos);

    // 3. Inspect line mapping with gdb if gdb is present in environment
    std::string gdbLog = "test_debugger_gdb.txt";
    std::string gdbCmd = "gdb --batch -ex \"file " + tempExe.string() + "\" -ex \"info line main\" > " + gdbLog + " 2>&1";
    int gdbRes = std::system(gdbCmd.c_str());
    if (gdbRes == 0) {
        std::ifstream gdbFile(gdbLog);
        std::string gdbContent((std::istreambuf_iterator<char>(gdbFile)), std::istreambuf_iterator<char>());
        gdbFile.close();
        CHECK(gdbContent.find("Line 13") != std::string::npos);
    }
    std::filesystem::remove(gdbLog, ec);

    std::filesystem::remove(tempExe, ec);
}

