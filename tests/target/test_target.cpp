#include "common/test_framework.hpp"
#include "compiler/codegen/Linker.hpp"
#include "compiler/codegen/llvm/LLVMEmitter.hpp"
#include "compiler/codegen/x86_64/X86_64CodeGen.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/ir/IRGenerator.hpp"
#include "compiler/parser/Parser.hpp"
#include "compiler/semantic/SemanticAnalyzer.hpp"
#include "compiler/source/SourceManager.hpp"
#include "compiler/target/Target.hpp"
#include <sstream>

TEST_CASE("Target: Parsing and canonical normalization of short target names") {
    // Host target alias
    auto hostT = sph::Target::parse("host");
    CHECK(hostT.has_value());
    CHECK_EQ(hostT->getCanonicalTriple(), sph::Target::getHostTarget().getCanonicalTriple());

    // Windows x86-64
    auto win = sph::Target::parse("x86_64-windows");
    CHECK(win.has_value());
    CHECK(win->isWindows());
    CHECK(win->isX86_64());
    CHECK(!win->isAArch64());
    CHECK(!win->isUnix());
    CHECK_EQ(win->getShortName(), "x86_64-windows");
    CHECK_EQ(win->getCanonicalTriple(), "x86_64-w64-windows-gnu");
    CHECK_EQ(win->getExecutableExtension(), ".exe");
    CHECK_EQ(win->getSharedLibraryExtension(), ".dll");
    CHECK_EQ(win->getCallingConvention(), sph::TargetCallingConvention::Win64);

    // Linux x86-64
    auto linux64 = sph::Target::parse("x86_64-linux");
    CHECK(linux64.has_value());
    CHECK(linux64->isLinux());
    CHECK(linux64->isX86_64());
    CHECK(linux64->isUnix());
    CHECK_EQ(linux64->getShortName(), "x86_64-linux");
    CHECK_EQ(linux64->getCanonicalTriple(), "x86_64-unknown-linux-gnu");
    CHECK_EQ(linux64->getExecutableExtension(), "");
    CHECK_EQ(linux64->getSharedLibraryExtension(), ".so");
    CHECK_EQ(linux64->getCallingConvention(), sph::TargetCallingConvention::SysV64);

    // Linux AArch64
    auto linuxArm = sph::Target::parse("aarch64-linux");
    CHECK(linuxArm.has_value());
    CHECK(linuxArm->isLinux());
    CHECK(linuxArm->isAArch64());
    CHECK(!linuxArm->isX86_64());
    CHECK(linuxArm->isUnix());
    CHECK_EQ(linuxArm->getShortName(), "aarch64-linux");
    CHECK_EQ(linuxArm->getCanonicalTriple(), "aarch64-unknown-linux-gnu");
    CHECK_EQ(linuxArm->getCallingConvention(), sph::TargetCallingConvention::AAPCS64);

    // MacOS AArch64 (Apple Silicon)
    auto macArm = sph::Target::parse("aarch64-macos");
    CHECK(macArm.has_value());
    CHECK(macArm->isMacOS());
    CHECK(macArm->isAArch64());
    CHECK(macArm->isUnix());
    CHECK_EQ(macArm->getShortName(), "aarch64-macos");
    CHECK_EQ(macArm->getCanonicalTriple(), "arm64-apple-macosx");
    CHECK_EQ(macArm->getSharedLibraryExtension(), ".dylib");
    CHECK_EQ(macArm->getCallingConvention(), sph::TargetCallingConvention::AAPCS64);

    // MacOS x86-64
    auto macIntel = sph::Target::parse("x86_64-macos");
    CHECK(macIntel.has_value());
    CHECK(macIntel->isMacOS());
    CHECK(macIntel->isX86_64());
    CHECK_EQ(macIntel->getShortName(), "x86_64-macos");
    CHECK_EQ(macIntel->getCanonicalTriple(), "x86_64-apple-darwin");
    CHECK_EQ(macIntel->getCallingConvention(), sph::TargetCallingConvention::SysV64);

    // Windows AArch64
    auto winArm = sph::Target::parse("aarch64-windows");
    CHECK(winArm.has_value());
    CHECK(winArm->isWindows());
    CHECK(winArm->isAArch64());
    CHECK_EQ(winArm->getShortName(), "aarch64-windows");
    CHECK_EQ(winArm->getCanonicalTriple(), "aarch64-pc-windows-msvc");
    CHECK_EQ(winArm->getExecutableExtension(), ".exe");
}

TEST_CASE("Target: Parsing canonical triples and rejecting malformed triples") {
    // Canonical triples
    auto t1 = sph::Target::parse("x86_64-w64-windows-gnu");
    CHECK(t1.has_value());
    CHECK(t1->isWindows());
    CHECK(t1->isX86_64());

    auto t2 = sph::Target::parse("x86_64-unknown-linux-gnu");
    CHECK(t2.has_value());
    CHECK(t2->isLinux());
    CHECK(t2->isX86_64());

    auto t3 = sph::Target::parse("arm64-apple-macosx");
    CHECK(t3.has_value());
    CHECK(t3->isMacOS());
    CHECK(t3->isAArch64());

    auto t4 = sph::Target::parse("aarch64-unknown-linux-gnu");
    CHECK(t4.has_value());
    CHECK(t4->isLinux());
    CHECK(t4->isAArch64());

    // Host detection
    auto host = sph::Target::getHostTarget();
    CHECK(host.isHost());
    CHECK(!host.isCrossCompileFromHost());
#if defined(_WIN32)
    CHECK(host.isWindows());
    CHECK(host.isX86_64());
#elif defined(__linux__)
    CHECK(host.isLinux());
    CHECK(host.isX86_64());
#endif

    // Supported targets list
    auto supported = sph::Target::getSupportedTargets();
    CHECK_EQ(supported.size(), 6u);

    // Invalid triples
    CHECK(!sph::Target::isValidTriple(""));
    CHECK(!sph::Target::isValidTriple("invalid"));
    CHECK(!sph::Target::isValidTriple("mips-unknown-linux-gnu"));
    CHECK(!sph::Target::isValidTriple("x86_64-unknown-solaris-gnu"));
    CHECK(!sph::Target::isValidTriple("foo-bar-baz-qux-extra"));
}

TEST_CASE("Target: ABI properties, section directives, and function attributes") {
    auto win = *sph::Target::parse("x86_64-windows");
    CHECK_EQ(win.getRoDataSectionDirective(), ".section .rdata,\"dr\"");
    CHECK_EQ(win.getFunctionTypeDirective("foo"), ".def foo; .scl 2; .type 32; .endef");

    auto linuxT = *sph::Target::parse("x86_64-linux");
    CHECK_EQ(linuxT.getRoDataSectionDirective(), ".section .rodata");
    CHECK_EQ(linuxT.getFunctionTypeDirective("foo"), ".type foo, @function");

    auto macT = *sph::Target::parse("aarch64-macos");
    CHECK_EQ(macT.getRoDataSectionDirective(), ".section __TEXT,__cstring,cstring_literals");
    CHECK_EQ(macT.getFunctionTypeDirective("foo"), "");
}

TEST_CASE("Target: Assembly generator produces System V ABI and ELF directives for x86_64-linux") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn calculate(a: i32, b: i32, c: i32): i32 {\n"
        "    return a + b + c;\n"
        "}\n"
        "fn main(): i32 {\n"
        "    let res = calculate(10, 20, 30);\n"
        "    println(res);\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("calc.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "calc.sph", &sm);
    CHECK(irModule != nullptr);

    // Emit for Linux x86-64
    std::ostringstream ssLinux;
    sph::X86_64CodeGen codegenLinux(ssLinux);
    codegenLinux.setTarget(*sph::Target::parse("x86_64-linux"));
    codegenLinux.generate(*irModule);
    std::string asmLinux = ssLinux.str();

    // Verify ELF directives
    CHECK(asmLinux.find(".type calculate, @function") != std::string::npos);
    CHECK(asmLinux.find(".type main, @function") != std::string::npos);
    CHECK(asmLinux.find(".def calculate") == std::string::npos);

    // Verify System V AMD64 argument registers in incoming param setup
    // Arg 0 -> rdi, Arg 1 -> rsi, Arg 2 -> rdx
    CHECK(asmLinux.find("rdi") != std::string::npos);
    CHECK(asmLinux.find("rsi") != std::string::npos);
    CHECK(asmLinux.find("rdx") != std::string::npos);

    // Verify System V calling setup for calculate(10, 20, 30)
    CHECK(asmLinux.find("call calculate") != std::string::npos);

    // Emit for Windows x86-64
    std::ostringstream ssWin;
    sph::X86_64CodeGen codegenWin(ssWin);
    codegenWin.setTarget(*sph::Target::parse("x86_64-windows"));
    codegenWin.generate(*irModule);
    std::string asmWin = ssWin.str();

    // Verify Windows COFF directives
    CHECK(asmWin.find(".def calculate; .scl 2; .type 32; .endef") != std::string::npos);
    CHECK(asmWin.find(".type calculate, @function") == std::string::npos);

    // Verify Win64 argument registers
    CHECK(asmWin.find("rcx") != std::string::npos);
    CHECK(asmWin.find("rdx") != std::string::npos);
    CHECK(asmWin.find("r8") != std::string::npos);
}

TEST_CASE("Target: LLVM emitter produces valid target datalayout and triple for all architectures") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn square(x: i32): i32 {\n"
        "    return x * x;\n"
        "}\n";

    uint32_t fileId = sm.addSource("square.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "square.sph", &sm);
    CHECK(irModule != nullptr);

    // Linux x86-64
    {
        std::ostringstream ss;
        sph::LLVMEmitter emitterLL(ss);
        emitterLL.setTarget(*sph::Target::parse("x86_64-linux"));
        emitterLL.emit(*irModule);
        std::string text = ss.str();
        CHECK(text.find("target triple = \"x86_64-unknown-linux-gnu\"") != std::string::npos);
        CHECK(text.find("target datalayout = \"e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128\"") != std::string::npos);
    }

    // Linux AArch64
    {
        std::ostringstream ss;
        sph::LLVMEmitter emitterLL(ss);
        emitterLL.setTarget(*sph::Target::parse("aarch64-linux"));
        emitterLL.emit(*irModule);
        std::string text = ss.str();
        CHECK(text.find("target triple = \"aarch64-unknown-linux-gnu\"") != std::string::npos);
        CHECK(text.find("target datalayout = \"e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128\"") != std::string::npos);
    }

    // MacOS AArch64 (Apple Silicon)
    {
        std::ostringstream ss;
        sph::LLVMEmitter emitterLL(ss);
        emitterLL.setTarget(*sph::Target::parse("aarch64-macos"));
        emitterLL.emit(*irModule);
        std::string text = ss.str();
        CHECK(text.find("target triple = \"arm64-apple-macosx\"") != std::string::npos);
        CHECK(text.find("target datalayout = \"e-m:o-i64:64-i128:128-n32:64-S128\"") != std::string::npos);
    }

    // Windows x86-64
    {
        std::ostringstream ss;
        sph::LLVMEmitter emitterLL(ss);
        emitterLL.setTarget(*sph::Target::parse("x86_64-windows"));
        emitterLL.emit(*irModule);
        std::string text = ss.str();
        CHECK(text.find("target triple = \"x86_64-w64-windows-gnu\"") != std::string::npos);
        CHECK(text.find("target datalayout = \"e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128\"") != std::string::npos);
    }
}

TEST_CASE("End-to-End Acceptance Test: Cross-compilation pipeline generates target assembly and toolchain abstraction (Milestone 11F)") {
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
    auto irModule = irGen.generate(*program, examplePath, &sm);
    CHECK(irModule != nullptr);

    // Cross-compile to x86_64-linux: generate assembly
    auto linuxTarget = *sph::Target::parse("x86_64-linux");
    std::ostringstream asmStream;
    sph::X86_64CodeGen codegen(asmStream);
    codegen.setTarget(linuxTarget);
    codegen.generate(*irModule);
    std::string asmOutput = asmStream.str();

    CHECK(asmOutput.find(".type add, @function") != std::string::npos);
    CHECK(asmOutput.find(".type main, @function") != std::string::npos);
    CHECK(asmOutput.find("rdi") != std::string::npos);
    CHECK(asmOutput.find("rsi") != std::string::npos);

    // Linker toolchain abstraction: cross-compiling on host where cross-compiler is absent provides clean diagnostic
    sph::Linker linker;
    std::string errorMsg;
    bool linkRes = linker.linkExecutable("dummy.s", "dummy_out", "dummy.a", errorMsg, "", false, linuxTarget);
    if (!linker.findCrossToolchain(linuxTarget)) {
        CHECK(!linkRes);
        CHECK(errorMsg.find("cross-compilation to target 'x86_64-unknown-linux-gnu' requires a cross-compiler") != std::string::npos);
    }
}
