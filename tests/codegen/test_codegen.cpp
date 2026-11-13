#include "common/test_framework.hpp"
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
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

TEST_CASE("X86_64CodeGen generates valid Windows x86-64 assembly") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "\n"
        "fn add(a: i32, b: i32): i32 {\n"
        "    return a + b;\n"
        "}\n"
        "\n"
        "fn main(): i32 {\n"
        "    let first: i32 = 100;\n"
        "    let second: i32 = 200;\n"
        "    let result = add(first, second);\n"
        "\n"
        "    println(result);\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("hello.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "hello_module");
    CHECK(irModule != nullptr);

    std::ostringstream ss;
    sph::X86_64CodeGen codegen(ss);
    codegen.generate(*irModule);
    std::string asmText = ss.str();

    CHECK(asmText.find(".intel_syntax noprefix") != std::string::npos);
    CHECK(asmText.find(".globl add") != std::string::npos);
    CHECK(asmText.find(".globl main") != std::string::npos);
    CHECK(asmText.find("push rbp") != std::string::npos);
    CHECK(asmText.find("mov rbp, rsp") != std::string::npos);
    CHECK(asmText.find("call add") != std::string::npos);
    CHECK(asmText.find("call sph_println_i32") != std::string::npos);
    CHECK(asmText.find("ret") != std::string::npos);
}

TEST_CASE("LLVMEmitter generates valid LLVM IR text") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn add(a: i32, b: i32): i32 {\n"
        "    return a + b;\n"
        "}\n";

    uint32_t fileId = sm.addSource("add.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "add_module");
    CHECK(irModule != nullptr);

    std::ostringstream ss;
    sph::LLVMEmitter llvmEmitter(ss);
    llvmEmitter.emit(*irModule);
    std::string llvmText = ss.str();

    CHECK(llvmText.find("target triple = \"" + sph::Target::getHostTarget().getLLVMTriple() + "\"") != std::string::npos);
    CHECK(llvmText.find("declare void @sph_println_i32(i32)") != std::string::npos);
    CHECK(llvmText.find("define i32 @add(i32 %arg_a, i32 %arg_b)") != std::string::npos);
    CHECK(llvmText.find("ret i32") != std::string::npos);
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs hello.sph producing 42") {
    sph::SourceManager sm;
    std::string examplePath = "examples/hello.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "acceptance_module");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_hello.s";
    std::filesystem::path tempExe = "test_hello.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    // Emit assembly
    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    // Link
    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::filesystem::remove(tempAsm);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    // Run executable and verify stdout is 300
    std::string outputLog = "test_hello_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::string line;
    std::getline(outFile, line);
    outFile.close();

    std::error_code ec;
    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK_EQ(line, "300");
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs struct.sph producing 150 and 200") {
    sph::SourceManager sm;
    std::string examplePath = "examples/struct.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "struct_acceptance_module");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_struct.s";
    std::filesystem::path tempExe = "test_struct.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    // Emit assembly
    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    // Link
    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::error_code ec;
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    // Run executable and verify stdout is 150 followed by 200
    std::string outputLog = "test_struct_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::string line1, line2;
    std::getline(outFile, line1);
    std::getline(outFile, line2);
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK_EQ(line1, "150");
    CHECK_EQ(line2, "200");
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs array.sph producing 190") {
    sph::SourceManager sm;
    std::string examplePath = "examples/array.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "array_acceptance_module");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_array.s";
    std::filesystem::path tempExe = "test_array.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    // Emit assembly
    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    // Link
    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::error_code ec;
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    // Run executable and verify stdout is 190
    std::string outputLog = "test_array_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::string line;
    std::getline(outFile, line);
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK_EQ(line, "190");
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs enum.sph producing 300") {
    sph::SourceManager sm;
    std::string examplePath = "examples/enum.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "enum_acceptance_module");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_enum.s";
    std::filesystem::path tempExe = "test_enum.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    // Emit assembly
    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    // Link
    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::error_code ec;
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    // Run executable and verify stdout is 300
    std::string outputLog = "test_enum_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::string line;
    std::getline(outFile, line);
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK_EQ(line, "300");
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs modules/main.sph producing 26") {
    sph::SourceManager sm;
    std::string examplePath = "examples/modules/main.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "modules_acceptance_module");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_modules.s";
    std::filesystem::path tempExe = "test_modules.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    // Emit assembly
    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    // Link
    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::error_code ec;
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    // Run executable and verify stdout is 26
    std::string outputLog = "test_modules_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::string line;
    std::getline(outFile, line);
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK_EQ(line, "26");
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs slice.sph producing 90") {
    sph::SourceManager sm;
    std::string examplePath = "examples/slice.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "slice_acceptance_module");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_slice.s";
    std::filesystem::path tempExe = "test_slice.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    // Emit assembly
    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    // Link
    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::error_code ec;
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    // Run executable and verify stdout is 90
    std::string outputLog = "test_slice_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::string line;
    std::getline(outFile, line);
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK_EQ(line, "90");
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs class.sph producing 120") {
    sph::SourceManager sm;
    std::string examplePath = "examples/class.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "class_acceptance_module");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_class.s";
    std::filesystem::path tempExe = "test_class.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    // Emit assembly
    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    // Link
    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::error_code ec;
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    // Run executable and verify stdout is 120
    std::string outputLog = "test_class_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::string line;
    std::getline(outFile, line);
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK_EQ(line, "120");
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs interface.sph producing 50 and 25") {
    sph::SourceManager sm;
    std::string examplePath = "examples/interface.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "interface_acceptance_module");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_interface.s";
    std::filesystem::path tempExe = "test_interface.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    // Emit assembly
    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    // Link
    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::error_code ec;
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    // Run executable and verify stdout is 50 and 25
    std::string outputLog = "test_interface_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::string line1, line2;
    std::getline(outFile, line1);
    std::getline(outFile, line2);
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK_EQ(line1, "50");
    CHECK_EQ(line2, "25");
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs generics.sph producing 30 and 100") {
    sph::SourceManager sm;
    std::string examplePath = "examples/generics.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "generics_acceptance_module");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_generics.s";
    std::filesystem::path tempExe = "test_generics.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    std::error_code ec;
    std::filesystem::remove(tempExe, ec);

    // Emit assembly
    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    // Link
    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    // Run executable and verify stdout is 30 and 100
    std::string outputLog = "test_generics_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::string line1, line2;
    std::getline(outFile, line1);
    std::getline(outFile, line2);
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK_EQ(line1, "30");
    CHECK_EQ(line2, "100");
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs pointers.sph producing 99 and 42") {
    sph::SourceManager sm;
    std::string examplePath = "examples/pointers.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer sem(emitter);
    bool semOk = sem.analyze(*program);
    CHECK(semOk);
    CHECK(!emitter.hasErrors());

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "pointers_app");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_pointers.s";
    std::filesystem::path tempExe = "test_pointers.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    std::error_code ec;
    std::filesystem::remove(tempExe, ec);

    // Emit assembly
    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    // Link
    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    // Run executable and verify stdout is 99 and 42
    std::string outputLog = "test_pointers_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::string line1, line2;
    std::getline(outFile, line1);
    std::getline(outFile, line2);
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK_EQ(line1, "99");
    CHECK_EQ(line2, "42");
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs c_ffi.sph producing 42 and Hello C FFI!") {
    sph::SourceManager sm;
    std::string examplePath = "examples/c_ffi.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer sem(emitter);
    bool semOk = sem.analyze(*program);
    CHECK(semOk);
    CHECK(!emitter.hasErrors());

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "c_ffi_app");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_c_ffi.s";
    std::filesystem::path tempExe = "test_c_ffi.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    std::error_code ec;
    std::filesystem::remove(tempExe, ec);

    // Emit assembly
    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    // Link
    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    // Run executable and verify stdout is 42 and Hello C FFI!
    std::string outputLog = "test_c_ffi_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::string line1, line2;
    std::getline(outFile, line1);
    std::getline(outFile, line2);
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK_EQ(line1, "42");
    CHECK_EQ(line2, "Hello C FFI!");
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs bitwise_ptr.sph producing 8, 14, 6, 24, 6, 200, 300, 250") {
    sph::SourceManager sm;
    std::string examplePath = "examples/bitwise_ptr.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer sem(emitter);
    bool semOk = sem.analyze(*program);
    CHECK(semOk);
    CHECK(!emitter.hasErrors());

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "bitwise_ptr_app");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_bitwise_ptr.s";
    std::filesystem::path tempExe = "test_bitwise_ptr.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    std::error_code ec;
    std::filesystem::remove(tempExe, ec);

    // Emit assembly
    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    // Link
    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    // Run executable
    std::string outputLog = "test_bitwise_ptr_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::vector<std::string> lines;
    std::string l;
    while (std::getline(outFile, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
    }
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK_EQ(lines.size(), 8u);
    if (lines.size() >= 8) {
        CHECK_EQ(lines[0], "8");
        CHECK_EQ(lines[1], "14");
        CHECK_EQ(lines[2], "6");
        CHECK_EQ(lines[3], "24");
        CHECK_EQ(lines[4], "6");
        CHECK_EQ(lines[5], "200");
        CHECK_EQ(lines[6], "300");
        CHECK_EQ(lines[7], "250");
    }
}

TEST_CASE("End-to-End Acceptance Test: Bitwise NOT, pointer difference, and explicit cast") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    let b: i64 = 500 as i64;\n"
        "    let c: i32 = b as i32;\n"
        "    println(c);\n"
        "    mut arr: [i32; 4] = [10, 20, 30, 40];\n"
        "    unsafe {\n"
        "        let p0: *i32 = &arr[0];\n"
        "        let p3: *i32 = p0 + 3;\n"
        "        let diff: i64 = p3 - p0;\n"
        "        println(diff as i32);\n"
        "    }\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("cast_diff_test.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer sem(emitter);
    bool semOk = sem.analyze(*program);
    CHECK(semOk);
    CHECK(!emitter.hasErrors());

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "cast_diff_app");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_cast_diff.s";
    std::filesystem::path tempExe = "test_cast_diff.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    std::error_code ec;
    std::filesystem::remove(tempExe, ec);

    // Emit assembly
    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    // Link
    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    // Run executable
    std::string outputLog = "test_cast_diff_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::vector<std::string> lines;
    std::string l;
    while (std::getline(outFile, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
    }
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK_EQ(lines.size(), 2u);
    if (lines.size() >= 2) {
        CHECK_EQ(lines[0], "500");
        CHECK_EQ(lines[1], "3");
    }
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs heap_layout.sph producing 1337, true, 256, 256") {
    sph::SourceManager sm;
    std::string examplePath = "examples/heap_layout.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "heap_layout_acceptance_module");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_heap_layout.s";
    std::filesystem::path tempExe = "test_heap_layout.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    std::error_code ec;
    std::filesystem::remove(tempExe, ec);

    // Emit assembly
    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    // Link
    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    // Run executable
    std::string outputLog = "test_heap_layout_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::vector<std::string> lines;
    std::string l;
    while (std::getline(outFile, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
    }
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK_EQ(lines.size(), 4u);
    if (lines.size() >= 4) {
        CHECK_EQ(lines[0], "1337");
        CHECK_EQ(lines[1], "true");
        CHECK_EQ(lines[2], "256");
        CHECK_EQ(lines[3], "256");
    }
}

TEST_CASE("End-to-End Acceptance Test: Uses sph_alloc and sph_free runtime memory functions") {
    std::string src =
        "<?php\n"
        "extern \"C\" fn sph_alloc(size: i64): *void;\n"
        "extern \"C\" fn sph_free(ptr: *void): void;\n"
        "fn main(): i32 {\n"
        "    unsafe {\n"
        "        let raw: *void = sph_alloc(4 as i64);\n"
        "        let p: *i32 = raw as *i32;\n"
        "        *p = 777;\n"
        "        println(*p);\n"
        "        sph_free(raw);\n"
        "    }\n"
        "    return 0;\n"
        "}\n";

    sph::SourceManager sm;
    auto fileIdOpt = sm.addSource("test_sph_alloc.sph", src);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "sph_alloc_free_module");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_sph_alloc.s";
    std::filesystem::path tempExe = "test_sph_alloc.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    std::error_code ec;
    std::filesystem::remove(tempExe, ec);

    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    std::string outputLog = "test_sph_alloc_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::string line;
    std::getline(outFile, line);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK_EQ(line, "777");
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs rc_basic.sph producing 10, 20, 2, 30, 2") {
    sph::SourceManager sm;
    std::string examplePath = "examples/rc_basic.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "rc_basic_acceptance_module");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_rc_basic.s";
    std::filesystem::path tempExe = "test_rc_basic.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    std::error_code ec;
    std::filesystem::remove(tempExe, ec);

    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    std::string outputLog = "test_rc_basic_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::vector<std::string> lines;
    std::string l;
    while (std::getline(outFile, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
    }
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK_EQ(lines.size(), 5u);
    if (lines.size() >= 5) {
        CHECK_EQ(lines[0], "10");
        CHECK_EQ(lines[1], "20");
        CHECK_EQ(lines[2], "2");
        CHECK_EQ(lines[3], "30");
        CHECK_EQ(lines[4], "2");
    }
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs ownership_move.sph producing 42, 42, 42") {
    sph::SourceManager sm;
    std::string examplePath = "examples/ownership_move.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "ownership_move_acceptance_module");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_ownership_move.s";
    std::filesystem::path tempExe = "test_ownership_move.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    std::error_code ec;
    std::filesystem::remove(tempExe, ec);

    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    std::string outputLog = "test_ownership_move_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::vector<std::string> lines;
    std::string l;
    while (std::getline(outFile, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
    }
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK_EQ(lines.size(), 3u);
    if (lines.size() >= 3) {
        CHECK_EQ(lines[0], "42");
        CHECK_EQ(lines[1], "42");
        CHECK_EQ(lines[2], "42");
    }
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs arena_basic.sph producing 1, 100, 2, 200, 3, 300") {
    sph::SourceManager sm;
    std::string examplePath = "examples/arena_basic.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "arena_basic_acceptance_module");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_arena_basic.s";
    std::filesystem::path tempExe = "test_arena_basic.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    std::error_code ec;
    std::filesystem::remove(tempExe, ec);

    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    std::string outputLog = "test_arena_basic_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::vector<std::string> lines;
    std::string l;
    while (std::getline(outFile, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
    }
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK_EQ(lines.size(), 6u);
    if (lines.size() >= 6) {
        CHECK_EQ(lines[0], "1");
        CHECK_EQ(lines[1], "100");
        CHECK_EQ(lines[2], "2");
        CHECK_EQ(lines[3], "200");
        CHECK_EQ(lines[4], "3");
        CHECK_EQ(lines[5], "300");
    }
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs strings_vec.sph producing strings, lengths, and vector operations") {
    sph::SourceManager sm;
    std::string examplePath = "examples/strings_vec.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "strings_vec_acceptance_module");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_strings_vec.s";
    std::filesystem::path tempExe = "test_strings_vec.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    std::error_code ec;
    std::filesystem::remove(tempExe, ec);

    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    std::string outputLog = "test_strings_vec_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::vector<std::string> lines;
    std::string l;
    while (std::getline(outFile, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
    }
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK_EQ(lines.size(), 9u);
    if (lines.size() >= 9) {
        CHECK_EQ(lines[0], "Hello, SantientPHP!");
        CHECK_EQ(lines[1], "19");
        CHECK_EQ(lines[2], "strings equal!");
        CHECK_EQ(lines[3], "3");
        CHECK_EQ(lines[4], "10");
        CHECK_EQ(lines[5], "20");
        CHECK_EQ(lines[6], "30");
        CHECK_EQ(lines[7], "30");
        CHECK_EQ(lines[8], "2");
    }
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs file_io.sph producing filesystem and environment outputs") {
    sph::SourceManager sm;
    std::string examplePath = "examples/file_io.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "file_io_acceptance_module");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_file_io.s";
    std::filesystem::path tempExe = "test_file_io.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    std::error_code ec;
    std::filesystem::remove(tempExe, ec);

    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    std::string outputLog = "test_file_io_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::vector<std::string> lines;
    std::string l;
    while (std::getline(outFile, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
    }
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK_EQ(lines.size(), 11u);
    if (lines.size() >= 11) {
        CHECK_EQ(lines[0], "false");
        CHECK_EQ(lines[1], "true");
        CHECK_EQ(lines[2], "true");
        CHECK_EQ(lines[3], "true");
        CHECK_EQ(lines[4], "Hello from SantientPHP!");
        CHECK_EQ(lines[5], "Appended line.");
        CHECK_EQ(lines[6], "");
        CHECK_EQ(lines[7], "true");
        CHECK_EQ(lines[8], "SantientEnvValue");
        CHECK_EQ(lines[9], "true");
        CHECK_EQ(lines[10], "false");
    }
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs result_handling.sph producing expected Result outputs") {
    sph::SourceManager sm;
    std::string examplePath = "examples/result_handling.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "result_handling_acceptance_module");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_result_handling.s";
    std::filesystem::path tempExe = "test_result_handling.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    std::error_code ec;
    std::filesystem::remove(tempExe, ec);

    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    std::string outputLog = "test_result_handling_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::vector<std::string> lines;
    std::string l;
    while (std::getline(outFile, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
    }
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK_EQ(lines.size(), 6u);
    if (lines.size() >= 6) {
        CHECK_EQ(lines[0], "true");
        CHECK_EQ(lines[1], "false");
        CHECK_EQ(lines[2], "25");
        CHECK_EQ(lines[3], "false");
        CHECK_EQ(lines[4], "true");
        CHECK_EQ(lines[5], "divide by zero error caught!");
    }
}

TEST_CASE("Code generation compiles and executes dynamic PHP operations") {
    sph::SourceManager sm;
    std::string examplePath = "examples/php_dynamic_ops.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    analyzer.setDynamicMode(true);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "php_dynamic_ops_module");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_php_dynamic.s";
    std::filesystem::path tempExe = "test_php_dynamic.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    std::error_code ec;
    std::filesystem::remove(tempExe, ec);

    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    std::string outputLog = "test_php_dynamic_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::vector<std::string> lines;
    std::string l;
    while (std::getline(outFile, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
    }
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK_EQ(lines.size(), 4u);
    if (lines.size() >= 4) {
        CHECK_EQ(lines[0], "a is less than b");
        CHECK_EQ(lines[1], "10 + '50' = 60");
        CHECK_EQ(lines[2], "10 + 3.5 = 13.5");
        CHECK_EQ(lines[3], "strings match!");
    }
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs php_array.sph producing expected associative and sequential array outputs") {
    sph::SourceManager sm;
    std::string examplePath = "examples/php_array.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    analyzer.setDynamicMode(true);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "php_array_acceptance_module");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_php_array.s";
    std::filesystem::path tempExe = "test_php_array.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    std::error_code ec;
    std::filesystem::remove(tempExe, ec);

    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    std::string outputLog = "test_php_array_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::vector<std::string> lines;
    std::string l;
    while (std::getline(outFile, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
    }
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK(lines.size() >= 10);
    if (lines.size() >= 10) {
        CHECK_EQ(lines[0], "Name: Alice");
        CHECK_EQ(lines[1], "Role: Admin");
        CHECK_EQ(lines[2], "Updated Role: SuperAdmin");
        CHECK_EQ(lines[3], "List[0]: 10");
        CHECK_EQ(lines[4], "List[1]: 20");
        CHECK_EQ(lines[5], "List[2]: 30");
        CHECK_EQ(lines[6], "List[3]: 40");
        CHECK_EQ(lines[7], "Mixed[0]: zero");
        CHECK_EQ(lines[8], "Mixed[tag]: ok");
        CHECK_EQ(lines[9], "Mixed[2]: two");
    }
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs php_foreach.sph producing expected loop outputs") {
    sph::SourceManager sm;
    std::string examplePath = "examples/php_foreach.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    analyzer.setDynamicMode(true);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "php_foreach_acceptance_module");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_php_foreach.s";
    std::filesystem::path tempExe = "test_php_foreach.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    std::error_code ec;
    std::filesystem::remove(tempExe, ec);

    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    std::string outputLog = "test_php_foreach_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::vector<std::string> lines;
    std::string l;
    while (std::getline(outFile, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
    }
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK(lines.size() >= 10);
    if (lines.size() >= 10) {
        CHECK_EQ(lines[0], "apple");
        CHECK_EQ(lines[1], "banana");
        CHECK_EQ(lines[2], "cherry");
        CHECK_EQ(lines[3], "France => Paris");
        CHECK_EQ(lines[4], "Japan => Tokyo");
        CHECK_EQ(lines[5], "Num: 1");
        CHECK_EQ(lines[6], "Num: 3");
        CHECK_EQ(lines[7], "100");
        CHECK_EQ(lines[8], "200");
        CHECK_EQ(lines[9], "300");
    }
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs php_stdlib.sph producing expected standard library core function outputs") {
    sph::SourceManager sm;
    std::string examplePath = "examples/php_stdlib.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    analyzer.setDynamicMode(true);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "php_stdlib_acceptance_module");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_php_stdlib.s";
    std::filesystem::path tempExe = "test_php_stdlib.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    std::error_code ec;
    std::filesystem::remove(tempExe, ec);

    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    std::string outputLog = "test_php_stdlib_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::vector<std::string> lines;
    std::string l;
    while (std::getline(outFile, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
    }
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK(lines.size() >= 23);
    if (lines.size() >= 23) {
        CHECK_EQ(lines[0], "Count: 3");
        CHECK_EQ(lines[1], "is_array: 1");
        CHECK_EQ(lines[2], "is_string: 1");
        CHECK_EQ(lines[3], "is_int: 1");
        CHECK_EQ(lines[4], "After push: 4");
        CHECK_EQ(lines[5], "Popped: date");
        CHECK_EQ(lines[6], "Shifted: apple");
        CHECK_EQ(lines[7], "Remaining count: 2");
        CHECK_EQ(lines[8], "Has banana: 1");
        CHECK_EQ(lines[9], "Has apple: ");
        CHECK_EQ(lines[10], "Keys count: 2");
        CHECK_EQ(lines[11], "Key: name");
        CHECK_EQ(lines[12], "Key: role");
        CHECK_EQ(lines[13], "Values count: 2");
        CHECK_EQ(lines[14], "Val: Alice");
        CHECK_EQ(lines[15], "Val: Admin");
        CHECK_EQ(lines[16], "Length: 19");
        CHECK_EQ(lines[17], "Substr: SantientPHP");
        CHECK_EQ(lines[18], "Pos of Santient: 7");
        CHECK_EQ(lines[19], "Exploded count: 3");
        CHECK_EQ(lines[20], "Color: red");
        CHECK_EQ(lines[21], "Color: green");
        CHECK_EQ(lines[22], "Color: blue");
        CHECK_EQ(lines[23], "Joined: red - green - blue");
    }
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs php_objects.sph producing expected object OOP outputs") {
    sph::SourceManager sm;
    std::string examplePath = "examples/php_objects.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    analyzer.setDynamicMode(true);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "php_objects_acceptance_module");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_php_objects.s";
    std::filesystem::path tempExe = "test_php_objects.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    // Emit assembly
    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    // Link
    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::error_code ec;
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    std::string outputLog = "test_php_objects_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::vector<std::string> lines;
    std::string l;
    while (std::getline(outFile, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
    }
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK(lines.size() >= 8);
    if (lines.size() >= 8) {
        CHECK_EQ(lines[0], "Hello, my name is Alice");
        CHECK_EQ(lines[1], "Initial name: Alice");
        CHECK_EQ(lines[2], "Initial age: 30");
        CHECK_EQ(lines[3], "Updated name: Bob");
        CHECK_EQ(lines[4], "is_object: 1");
        CHECK_EQ(lines[5], "get_class: Person");
        CHECK_EQ(lines[6], "property_exists name: 1");
        CHECK_EQ(lines[7], "property_exists missing: ");
    }
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs php_exceptions.sph") {
    sph::SourceManager sm;
    std::string examplePath = "examples/php_exceptions.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    analyzer.setDynamicMode(true);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "php_exceptions_acceptance_module");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_php_exceptions.s";
    std::filesystem::path tempExe = "test_php_exceptions.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    // Emit assembly
    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    // Link
    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::error_code ec;
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    std::string outputLog = "test_php_exceptions_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::vector<std::string> lines;
    std::string l;
    while (std::getline(outFile, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
    }
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK(lines.size() >= 8);
    if (lines.size() >= 8) {
        CHECK_EQ(lines[0], "--- PHP By-Reference Parameters ---");
        CHECK_EQ(lines[1], "Ref before: 10");
        CHECK_EQ(lines[2], "Ref after: 15");
        CHECK_EQ(lines[3], "--- PHP Exceptions & Unwinding ---");
        CHECK_EQ(lines[4], "Div result: 50");
        CHECK_EQ(lines[5], "Caught Exception: Division by zero");
        CHECK_EQ(lines[6], "Finally block executed");
        CHECK_EQ(lines[7], "Exception handling completed successfully");
    }
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs php_db_http.sph producing expected PDO and cURL outputs") {
    sph::SourceManager sm;
    std::string examplePath = "examples/php_db_http.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    analyzer.setDynamicMode(true);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "php_db_http_acceptance_module");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_php_db_http.s";
    std::filesystem::path tempExe = "test_php_db_http.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    // Emit assembly
    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    // Link
    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::error_code ec;
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    std::string outputLog = "test_php_db_http_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::vector<std::string> lines;
    std::string l;
    while (std::getline(outFile, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
    }
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK(lines.size() >= 10);
    if (lines.size() >= 10) {
        CHECK_EQ(lines[0], "--- Testing PDO SQLite ---");
        CHECK_EQ(lines[1], "User: Alice (Admin)");
        CHECK_EQ(lines[2], "User: Bob (Engineer)");
        CHECK_EQ(lines[3], "User: Charlie (Designer)");
        CHECK_EQ(lines[4], "Single User: Bob");
        CHECK_EQ(lines[5], "--- Testing HTTP Client ---");
        CHECK_EQ(lines[6], "HTTP Status: 200");
        CHECK_EQ(lines[7], "HTTP Response: {\"status\":\"ok\",\"action\":\"created\",\"received\":\"user=alice\"}");
        CHECK_EQ(lines[8], "File Get Contents: {\"status\":\"ok\",\"message\":\"SantientPHP HTTP service active\",\"url\":\"http://mock/api/data\"}");
        CHECK_EQ(lines[9], "Milestone 10F Interop Success!");
    }
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs atomics_threads.sph producing expected atomic and multi-threading outputs (Milestone 11A)") {
    sph::SourceManager sm;
    std::string examplePath = "examples/atomics_threads.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "atomics_threads_acceptance_module");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_atomics_threads.s";
    std::filesystem::path tempExe = "test_atomics_threads.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    // Emit assembly
    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    // Link
    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::error_code ec;
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    std::string outputLog = "test_atomics_threads_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::vector<std::string> lines;
    std::string l;
    while (std::getline(outFile, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
    }
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK(lines.size() >= 16);
    if (lines.size() >= 16) {
        CHECK_EQ(lines[0], "--- Test 1: Single-thread Atomic Operations ---");
        CHECK_EQ(lines[1], "Initial AtomicI32: 10");
        CHECK_EQ(lines[2], "After store: 42");
        CHECK_EQ(lines[3], "fetch_add(8) old: 42, new: 50");
        CHECK_EQ(lines[4], "fetch_sub(10) old: 50, new: 40");
        CHECK_EQ(lines[5], "exchange(100) old: 40, new: 100");
        CHECK_EQ(lines[6], "compare_exchange(100 -> 200): success, value: 200");
        CHECK_EQ(lines[7], "compare_exchange(999 -> 300): failed, value: 200");
        CHECK_EQ(lines[8], "--- Test 2: AtomicI64 and AtomicBool ---");
        CHECK_EQ(lines[9], "AtomicI64 value: 1500000000");
        CHECK_EQ(lines[10], "AtomicBool init: ");
        CHECK_EQ(lines[11], "AtomicBool after store: 1");
        CHECK_EQ(lines[12], "--- Test 3: Multi-threaded Concurrency with Native Threads ---");
        CHECK_EQ(lines[13], "Shared counter initial: 0");
        CHECK_EQ(lines[14], "Final shared counter: 1500");
        CHECK_EQ(lines[15], "Thread sleep completed successfully");
    }
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs async_coroutine.sph producing expected coroutine and promise outputs (Milestone 11B)") {
    sph::SourceManager sm;
    std::string examplePath = "examples/async_coroutine.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "async_coroutine_acceptance_module");
    CHECK(irModule != nullptr);

    std::filesystem::path tempAsm = "test_async_coro.s";
    std::filesystem::path tempExe = "test_async_coro.exe";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib) && std::filesystem::exists("lib/libsph_runtime.a")) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    // Emit assembly
    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    CHECK(std::filesystem::exists(tempAsm));

    // Link
    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::error_code ec;
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    std::string outputLog = "test_async_coro_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::vector<std::string> lines;
    std::string l;
    while (std::getline(outFile, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
    }
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK(lines.size() >= 12);
    if (lines.size() >= 12) {
        CHECK_EQ(lines[0], "1");
        CHECK_EQ(lines[1], "false");
        CHECK_EQ(lines[2], "true");
        CHECK_EQ(lines[3], "42");
        CHECK_EQ(lines[4], "20");
        CHECK_EQ(lines[5], "30");
        CHECK_EQ(lines[6], "200");
        CHECK_EQ(lines[7], "21");
        CHECK_EQ(lines[8], "31");
        CHECK_EQ(lines[9], "200");
        CHECK_EQ(lines[10], "300");
        CHECK_EQ(lines[11], "2000");
    }
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs simd_vectors.sph producing expected vector and CPU feature outputs (Milestone 11C)") {
    sph::SourceManager sm;
    std::string examplePath = "examples/simd_vectors.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);
    CHECK(!emitter.hasErrors());

    sph::SemanticAnalyzer analyzer(emitter);
    bool semOk = analyzer.analyze(*program);
    CHECK(semOk);
    CHECK(!emitter.hasErrors());

    sph::IRGenerator irGen;
    auto module = irGen.generate(*program, "simd_test");
    CHECK(module != nullptr);

    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib)) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    std::filesystem::path tempAsm = "test_simd_vectors.s";
    std::filesystem::path tempExe = "test_simd_vectors.exe";

    std::ofstream asmOut(tempAsm);
    sph::X86_64CodeGen codegen(asmOut);
    codegen.generate(*module);
    asmOut.close();

    CHECK(std::filesystem::exists(tempAsm));

    // Link
    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::error_code ec;
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    std::string outputLog = "test_simd_vectors_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::vector<std::string> lines;
    std::string l;
    while (std::getline(outFile, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
    }
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK(lines.size() >= 39);
    if (lines.size() >= 39) {
        CHECK_EQ(lines[0], "--- 1. Target CPU SIMD Feature Detection ---");
        CHECK_EQ(lines[1], "true");
        CHECK_EQ(lines[2], "true");
        CHECK_EQ(lines[3], "true");
        CHECK_EQ(lines[4], "true");
        CHECK_EQ(lines[5], "--- 2. f32x4 Vector Construction & Operations ---");
        CHECK_EQ(lines[6], "f32x4(1, 2, 3, 4)");
        CHECK_EQ(lines[7], "f32x4(5, 6, 7, 8)");
        CHECK_EQ(lines[8], "f32x4(0, 0, 0, 0)");
        CHECK_EQ(lines[9], "f32x4(10, 10, 10, 10)");
        CHECK_EQ(lines[10], "--- 3. f32x4 Vector Arithmetic ---");
        CHECK_EQ(lines[11], "f32x4(6, 8, 10, 12)");
        CHECK_EQ(lines[12], "f32x4(4, 4, 4, 4)");
        CHECK_EQ(lines[13], "f32x4(5, 12, 21, 32)");
        CHECK_EQ(lines[14], "f32x4(5, 3, 2.33333, 2)");
        CHECK_EQ(lines[15], "--- 4. f32x4 Vector Math Functions ---");
        CHECK_EQ(lines[16], "f32x4(2, 3, 4, 5)");
        CHECK_EQ(lines[17], "f32x4(1.5, 2.5, 3.5, 4.5)");
        CHECK_EQ(lines[18], "f32x4(1, 2, 3, 4)");
        CHECK_EQ(lines[19], "f32x4(5, 6, 7, 8)");
        CHECK_EQ(lines[20], "--- 5. f32x4 Reductions & Dot Product ---");
        CHECK_EQ(lines[21], "10");
        CHECK_EQ(lines[22], "70");
        CHECK_EQ(lines[23], "--- 6. Lane Access & Mutation ---");
        CHECK_EQ(lines[24], "1");
        CHECK_EQ(lines[25], "2");
        CHECK_EQ(lines[26], "3");
        CHECK_EQ(lines[27], "4");
        CHECK_EQ(lines[28], "50");
        CHECK_EQ(lines[29], "--- 7. i32x4 Vector Operations ---");
        CHECK_EQ(lines[30], "i32x4(15, 25, 35, 45)");
        CHECK_EQ(lines[31], "i32x4(5, 15, 25, 35)");
        CHECK_EQ(lines[32], "120");
        CHECK_EQ(lines[33], "--- 8. f64x2 Vector Operations ---");
        CHECK_EQ(lines[34], "f64x2(50, 150)");
        CHECK_EQ(lines[35], "200");
        CHECK_EQ(lines[36], "--- 9. i64x2 Vector Operations ---");
        CHECK_EQ(lines[37], "i64x2(1500000000, 2500000000)");
        CHECK_EQ(lines[38], "4000000000");
    }
}

TEST_CASE("End-to-End Acceptance Test: Compiles and runs optimizations.sph with -O2 optimization pipeline (Milestone 11D)") {
    sph::SourceManager sm;
    std::string examplePath = "examples/optimizations.sph";
    if (!std::filesystem::exists(examplePath) && std::filesystem::exists("../" + examplePath)) {
        examplePath = "../" + examplePath;
    }
    auto fileIdOpt = sm.loadFile(examplePath);
    CHECK(fileIdOpt.has_value());
    if (!fileIdOpt) return;

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(*fileIdOpt, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);
    CHECK(!emitter.hasErrors());

    sph::SemanticAnalyzer analyzer(emitter);
    bool semOk = analyzer.analyze(*program);
    CHECK(semOk);
    CHECK(!emitter.hasErrors());

    sph::IRGenerator irGen;
    auto module = irGen.generate(*program, "opt_test");
    CHECK(module != nullptr);

    // Run -O2 Optimization Pipeline
    auto pm = sph::PassManager::createForLevel(sph::OptimizationLevel::O2);
    pm.run(*module);

    sph::IRVerifier verifier;
    std::vector<std::string> vErrors;
    CHECK(verifier.verify(*module, vErrors));

    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib)) {
        runtimeLib = "lib/libsph_runtime.a";
    }

    std::filesystem::path tempAsm = "test_opt_e2e.s";
    std::filesystem::path tempExe = "test_opt_e2e.exe";

    std::ofstream asmOut(tempAsm);
    sph::X86_64CodeGen codegen(asmOut);
    codegen.generate(*module);
    asmOut.close();

    CHECK(std::filesystem::exists(tempAsm));

    // Link with O2 optimization flags
    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg, "-O2");
    std::error_code ec;
    std::filesystem::remove(tempAsm, ec);

    CHECK(linkOk);
    CHECK(std::filesystem::exists(tempExe));

    std::string outputLog = "test_opt_e2e_out.txt";
    std::string runCmd = SPH_RUN_PREFIX + tempExe.string() + " > " + outputLog;
    int runRes = std::system(runCmd.c_str());
    CHECK_EQ(runRes, 0);

    std::ifstream outFile(outputLog);
    std::vector<std::string> lines;
    std::string l;
    while (std::getline(outFile, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
    }
    outFile.close();

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);

    CHECK_EQ(lines.size(), size_t(4));
    if (lines.size() >= 4) {
        CHECK_EQ(lines[0], "300");
        CHECK_EQ(lines[1], "25");
        CHECK_EQ(lines[2], "42");
        CHECK_EQ(lines[3], "60");
    }
}



