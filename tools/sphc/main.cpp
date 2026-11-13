#include "compiler/ast/ASTPrinter.hpp"
#include "compiler/codegen/Linker.hpp"
#include "compiler/codegen/llvm/LLVMEmitter.hpp"
#include "compiler/codegen/x86_64/X86_64CodeGen.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/docgen/DocExtractor.hpp"
#include "compiler/docgen/DocGenerator.hpp"
#include "compiler/doctor/Doctor.hpp"
#include "compiler/formatter/Formatter.hpp"
#include "compiler/lsp/LspServer.hpp"
#include "compiler/package/PackageManager.hpp"
#include "compiler/ir/IRGenerator.hpp"
#include "compiler/ir/IRPrinter.hpp"
#include "compiler/ir/IRVerifier.hpp"
#include "compiler/lexer/Lexer.hpp"
#include "compiler/manifest/Manifest.hpp"
#include "compiler/optimizer/PassManager.hpp"
#include "compiler/parser/Parser.hpp"
#include "compiler/semantic/SemanticAnalyzer.hpp"
#include "compiler/source/SourceManager.hpp"
#include "compiler/target/Target.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <sys/wait.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace {

static std::filesystem::path getExecutableDir() {
#if defined(_WIN32)
    wchar_t buffer[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, buffer, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        return std::filesystem::path(buffer).parent_path();
    }
#elif defined(__APPLE__)
    char buffer[1024];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) == 0) {
        std::error_code ec;
        return std::filesystem::canonical(buffer, ec).parent_path();
    }
#else
    char buffer[1024];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len > 0) {
        buffer[len] = '\0';
        std::error_code ec;
        return std::filesystem::canonical(buffer, ec).parent_path();
    }
#endif
    return std::filesystem::current_path();
}

void printVersion() {
    std::cout << "sphc 0.1.0 (SantientPHP)\n"
              << "Baseline: SPH 0.1\n"
              << "Default Host Target: " << sph::Target::getHostTarget().getCanonicalTriple() << "\n";
}

void printTargets() {
    sph::Target host = sph::Target::getHostTarget();
    std::cout << "SantientPHP Supported Targets:\n\n";
    std::cout << "  Triple                 Canonical Triple             Description\n";
    std::cout << "  ----------------------------------------------------------------------------------\n";
    for (const auto& t : sph::Target::getSupportedTargets()) {
        bool isHost = (t == host);
        std::cout << "  " << std::left << std::setw(23) << t.getShortName()
                  << std::left << std::setw(29) << t.getCanonicalTriple()
                  << t.getDescription();
        if (isHost) {
            std::cout << " [HOST]";
        }
        std::cout << "\n";
    }
    std::cout << "\nDefault host target: " << host.getCanonicalTriple() << " (" << host.getShortName() << ")\n";
}

void printHelp() {
    std::cout << "Usage: sphc <command> [options] [file.sph]\n\n"
              << "Commands:\n"
              << "  build      [file.sph]   Compile source file to target binary\n"
              << "  run        [file.sph]   Compile and run executable (host only)\n"
              << "  check      [file.sph]   Check syntax and semantics\n"
              << "  test       [file.sph]   Run unit tests or test suite\n"
              << "  clean                   Clean build artifacts and cache\n"
              << "  inspect    <file.sph>   Inspect compiler stages (--tokens, --ast, --ir, --asm, --llvm)\n"
              << "  tokens     <file.sph>   Tokenize file and print token stream\n"
              << "  ast        <file.sph>   Parse file and display AST\n"
              << "  ir         [file.sph]   Compile to verified IR and display\n"
              << "  emit-asm   [file.sph]   Emit target machine assembly\n"
              << "  emit-llvm  [file.sph]   Emit standard LLVM IR (.ll)\n"
              << "  fmt        [file.sph]   Format source code (--check, --stdout)\n"
              << "  doc        [file.sph]   Generate API documentation (--format=html|md, -o <path>)\n"
              << "  lsp                     Run Language Server Protocol engine (JSON-RPC over stdio)\n"
              << "  doctor                  Run environment and toolchain health diagnostics\n"
              << "  new        <name>       Create a new SantientPHP project structure\n"
              << "  init                    Initialize sph.toml in current directory\n"
              << "  pkg        <subcommand> Manage packages and dependencies (add, list)\n"
              << "  targets                 List supported target triples and architectures\n\n"
              << "Options:\n"
              << "  -o <path>               Output binary or file path\n"
              << "  -t, --target <triple>   Set target compilation triple (e.g. x86_64-linux, aarch64-linux)\n"
              << "  --list-targets          List supported target triples and architectures\n"
              << "  -g, -g0..3              Emit debug information (DWARF / line tables)\n"
              << "  -O, -O0..3, -Os         Set optimization level (default: -O0 or manifest)\n"
              << "  --check                 Check formatting without modifying files (for 'fmt')\n"
              << "  --stdout                Print formatted output to stdout without modifying file\n"
              << "  --format=<fmt>          Output format for 'doc' (html, md)\n"
              << "  --title=<title>         Custom title for documentation\n"
              << "  --mode=<mode>           Set compatibility mode (strict, transitional, dynamic)\n"
              << "  --compat-php            Enable PHP compatibility mode\n"
              << "  -v, --version           Display version information\n"
              << "  -h, --help              Display this help message\n";
}

int cmdFmt(const std::string& path, bool checkOnly, bool toStdout) {
    sph::Formatter formatter;
    if (path.empty()) {
        std::cerr << "error: missing source file argument for 'fmt'\n";
        return 1;
    }
    if (!std::filesystem::exists(path)) {
        std::cerr << "error: file not found '" << path << "'\n";
        return 1;
    }

    if (toStdout) {
        std::ifstream file(path);
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        auto res = formatter.format(content);
        if (!res.success) {
            std::cerr << "error formatting: " << res.errorMessage << "\n";
            return 1;
        }
        std::cout << res.formattedCode;
        return 0;
    }

    if (checkOnly) {
        if (!formatter.isFileFormatted(path)) {
            std::cout << "needs formatting: " << path << "\n";
            return 1;
        }
        return 0;
    }

    auto res = formatter.formatFile(path, true);
    if (!res.success) {
        std::cerr << "error: failed to format '" << path << "': " << res.errorMessage << "\n";
        return 1;
    }

    if (res.changed) {
        std::cout << "formatted: " << path << "\n";
    }
    return 0;
}

std::filesystem::path findRuntimeLibrary() {
    std::vector<std::filesystem::path> candidates;

    if (const char* sphHome = std::getenv("SPH_HOME")) {
        candidates.push_back(std::filesystem::path(sphHome) / "lib" / "libsph_runtime.a");
        candidates.push_back(std::filesystem::path(sphHome) / "libsph_runtime.a");
    }
    if (const char* sphHome2 = std::getenv("SANTIENTPHP_HOME")) {
        candidates.push_back(std::filesystem::path(sphHome2) / "lib" / "libsph_runtime.a");
        candidates.push_back(std::filesystem::path(sphHome2) / "libsph_runtime.a");
    }

    std::filesystem::path exeDir = getExecutableDir();
    candidates.push_back(exeDir / "libsph_runtime.a");
    candidates.push_back(exeDir / "lib" / "libsph_runtime.a");
    candidates.push_back(exeDir.parent_path() / "lib" / "libsph_runtime.a");
    candidates.push_back(exeDir.parent_path() / "libsph_runtime.a");
    candidates.push_back(exeDir.parent_path().parent_path() / "build" / "lib" / "libsph_runtime.a");
    candidates.push_back(exeDir.parent_path().parent_path() / "lib" / "libsph_runtime.a");

    candidates.push_back("C:/Program Files/SantientPHP/lib/libsph_runtime.a");
    candidates.push_back("C:/SantientPHP/lib/libsph_runtime.a");

    candidates.push_back("build/lib/libsph_runtime.a");
    candidates.push_back("lib/libsph_runtime.a");
    candidates.push_back("../lib/libsph_runtime.a");
    candidates.push_back("../../lib/libsph_runtime.a");
    candidates.push_back("../build/lib/libsph_runtime.a");

    for (const auto& c : candidates) {
        std::error_code ec;
        if (!c.empty() && std::filesystem::exists(c, ec)) {
            return std::filesystem::canonical(c, ec);
        }
    }
    return "build/lib/libsph_runtime.a";
}

std::unique_ptr<sph::IRModule> compileToIR(const std::string& path, sph::SourceManager& sourceManager,
                                           sph::DiagnosticEmitter& emitter, bool forceDynamic = false,
                                           sph::OptimizationLevel optLevel = sph::OptimizationLevel::O0) {
    auto fileIdOpt = sourceManager.loadFile(path);
    if (!fileIdOpt) {
        std::cerr << "error: could not open source file '" << path << "'\n";
        return nullptr;
    }

    uint32_t fileId = *fileIdOpt;
    sph::Parser parser(fileId, sourceManager, &emitter);

    auto program = parser.parseProgram();
    if (emitter.hasErrors() || !program) {
        std::cerr << emitter.getErrorCount() << " syntax error(s) found.\n";
        return nullptr;
    }

    sph::SemanticAnalyzer analyzer(emitter);
    std::filesystem::path srcP(path);
    auto manifestOpt = sph::Manifest::findInPath(srcP.has_parent_path() ? srcP.parent_path().string() : ".");
    if (forceDynamic || (manifestOpt && manifestOpt->isDynamicMode())) {
        analyzer.setDynamicMode(true);
    }

    if (!analyzer.analyze(*program) || emitter.hasErrors()) {
        std::cerr << emitter.getErrorCount() << " semantic error(s) found.\n";
        return nullptr;
    }

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, path, &sourceManager);

    sph::IRVerifier verifier;
    std::vector<std::string> verifierErrors;
    if (!verifier.verify(*irModule, verifierErrors)) {
        std::cerr << "IR verification failed (" << verifierErrors.size() << " errors):\n";
        for (const auto& err : verifierErrors) {
            std::cerr << "  " << err << "\n";
        }
        return nullptr;
    }

    if (optLevel != sph::OptimizationLevel::O0) {
        auto pm = sph::PassManager::createForLevel(optLevel);
        pm.run(*irModule);

        verifierErrors.clear();
        if (!verifier.verify(*irModule, verifierErrors)) {
            std::cerr << "IR verification failed after optimization (" << verifierErrors.size() << " errors):\n";
            for (const auto& err : verifierErrors) {
                std::cerr << "  " << err << "\n";
            }
            return nullptr;
        }
    }

    return irModule;
}

int cmdTokens(const std::string& path) {
    sph::SourceManager sourceManager;
    auto fileIdOpt = sourceManager.loadFile(path);
    if (!fileIdOpt) {
        std::cerr << "error: could not open source file '" << path << "'\n";
        return 1;
    }

    uint32_t fileId = *fileIdOpt;
    sph::DiagnosticEmitter emitter(sourceManager, &std::cerr);
    sph::Lexer lexer(fileId, sourceManager, &emitter);

    std::vector<sph::Token> tokens = lexer.tokenizeAll();

    std::cout << "Tokens for " << path << ":\n";
    for (const auto& tok : tokens) {
        std::cout << "  [" << tok.span.start.line << ":" << tok.span.start.column << "] "
                  << sph::tokenKindToString(tok.kind);
        if (!tok.text.empty()) {
            std::cout << " (" << tok.text << ")";
        }
        std::cout << "\n";
    }

    return emitter.hasErrors() ? 1 : 0;
}

int cmdAst(const std::string& path) {
    sph::SourceManager sourceManager;
    auto fileIdOpt = sourceManager.loadFile(path);
    if (!fileIdOpt) {
        std::cerr << "error: could not open source file '" << path << "'\n";
        return 1;
    }

    uint32_t fileId = *fileIdOpt;
    sph::DiagnosticEmitter emitter(sourceManager, &std::cerr);
    sph::Parser parser(fileId, sourceManager, &emitter);

    auto program = parser.parseProgram();
    if (emitter.hasErrors() || !program) {
        std::cerr << emitter.getErrorCount() << " error(s) found during parsing.\n";
        return 1;
    }

    sph::ASTPrinter printer(std::cout);
    printer.print(*program);
    return 0;
}

int cmdCheck(const std::string& path, bool forceDynamic = false, sph::OptimizationLevel optLevel = sph::OptimizationLevel::O0) {
    sph::SourceManager sourceManager;
    sph::DiagnosticEmitter emitter(sourceManager, &std::cerr);
    auto irModule = compileToIR(path, sourceManager, emitter, forceDynamic, optLevel);
    if (!irModule) {
        return 1;
    }

    std::cout << "Syntax and semantic check passed: " << path << "\n";
    return 0;
}

int cmdIr(const std::string& path, bool forceDynamic = false, sph::OptimizationLevel optLevel = sph::OptimizationLevel::O0) {
    sph::SourceManager sourceManager;
    sph::DiagnosticEmitter emitter(sourceManager, &std::cerr);
    auto irModule = compileToIR(path, sourceManager, emitter, forceDynamic, optLevel);
    if (!irModule) {
        return 1;
    }

    sph::IRPrinter printer(std::cout);
    printer.print(*irModule);
    return 0;
}

int cmdEmitAsm(const std::string& path, const std::string& outPath, bool forceDynamic = false,
               sph::OptimizationLevel optLevel = sph::OptimizationLevel::O0, bool debugInfo = false,
               const sph::Target& target = sph::Target::getHostTarget()) {
    sph::SourceManager sourceManager;
    sph::DiagnosticEmitter emitter(sourceManager, &std::cerr);
    auto irModule = compileToIR(path, sourceManager, emitter, forceDynamic, optLevel);
    if (!irModule) {
        return 1;
    }

    if (!outPath.empty()) {
        std::ofstream outFile(outPath);
        if (!outFile.is_open()) {
            std::cerr << "error: could not open output file '" << outPath << "'\n";
            return 1;
        }
        sph::X86_64CodeGen codegen(outFile);
        codegen.setTarget(target);
        codegen.setDebugInfo(debugInfo);
        codegen.generate(*irModule);
    } else {
        sph::X86_64CodeGen codegen(std::cout);
        codegen.setTarget(target);
        codegen.setDebugInfo(debugInfo);
        codegen.generate(*irModule);
    }

    return 0;
}

int cmdEmitLlvm(const std::string& path, const std::string& outPath, bool forceDynamic = false,
                sph::OptimizationLevel optLevel = sph::OptimizationLevel::O0, bool debugInfo = false,
                const sph::Target& target = sph::Target::getHostTarget()) {
    sph::SourceManager sourceManager;
    sph::DiagnosticEmitter emitter(sourceManager, &std::cerr);
    auto irModule = compileToIR(path, sourceManager, emitter, forceDynamic, optLevel);
    if (!irModule) {
        return 1;
    }

    if (!outPath.empty()) {
        std::ofstream outFile(outPath);
        if (!outFile.is_open()) {
            std::cerr << "error: could not open output file '" << outPath << "'\n";
            return 1;
        }
        sph::LLVMEmitter llvmEmitter(outFile);
        llvmEmitter.setTarget(target);
        llvmEmitter.setDebugInfo(debugInfo);
        llvmEmitter.emit(*irModule);
    } else {
        sph::LLVMEmitter llvmEmitter(std::cout);
        llvmEmitter.setTarget(target);
        llvmEmitter.setDebugInfo(debugInfo);
        llvmEmitter.emit(*irModule);
    }

    return 0;
}

int cmdInspect(const std::string& path, const std::string& stage, bool forceDynamic = false,
               sph::OptimizationLevel optLevel = sph::OptimizationLevel::O0, bool debugInfo = false,
               const sph::Target& target = sph::Target::getHostTarget()) {
    if (stage == "--tokens" || stage == "-tokens" || stage == "tokens") {
        return cmdTokens(path);
    }
    if (stage == "--ast" || stage == "-ast" || stage == "ast") {
        return cmdAst(path);
    }
    if (stage == "--ir" || stage == "-ir" || stage == "ir") {
        return cmdIr(path, forceDynamic, optLevel);
    }
    if (stage == "--asm" || stage == "-asm" || stage == "asm" || stage == "--emit-asm") {
        return cmdEmitAsm(path, "", forceDynamic, optLevel, debugInfo, target);
    }
    if (stage == "--llvm" || stage == "-llvm" || stage == "llvm" || stage == "--emit-llvm") {
        return cmdEmitLlvm(path, "", forceDynamic, optLevel, debugInfo, target);
    }

    std::cout << "=== AST: " << path << " ===\n";
    int r = cmdAst(path);
    if (r != 0) return r;
    std::cout << "\n=== SPHIR: " << path << " ===\n";
    return cmdIr(path, forceDynamic, optLevel);
}

int cmdBuild(const std::string& path, std::string outExe, bool forceDynamic = false,
             sph::OptimizationLevel optLevel = sph::OptimizationLevel::O0, bool debugInfo = false,
             const sph::Target& target = sph::Target::getHostTarget()) {
    std::filesystem::path srcPath(path);
    if (outExe.empty()) {
        outExe = srcPath.stem().string() + target.getExecutableExtension();
    }

    std::filesystem::path asmPath = srcPath.stem().string() + ".s";

    int asmRes = cmdEmitAsm(path, asmPath.string(), forceDynamic, optLevel, debugInfo, target);
    if (asmRes != 0) {
        return asmRes;
    }

    std::filesystem::path runtimeLib = findRuntimeLibrary();
    sph::Linker linker;
    std::string errorMsg;
    std::string optFlag = (optLevel != sph::OptimizationLevel::O0) ? sph::optimizationLevelToString(optLevel) : "";
    bool linkOk = linker.linkExecutable(asmPath, outExe, runtimeLib, errorMsg, optFlag, debugInfo, target);

    if (!linkOk) {
        if (target.isCrossCompileFromHost()) {
            std::cout << "sphc: note: " << errorMsg << "\n";
            std::cout << "sphc: target assembly generated: " << asmPath.string() << "\n";
            return 0;
        }
        std::filesystem::remove(asmPath);
        std::cerr << "error: " << errorMsg << "\n";
        return 1;
    }

    // Clean up temporary .s file on successful host link
    std::filesystem::remove(asmPath);

    std::cout << "Successfully compiled: " << outExe << " (target: " << target.getShortName() << ")\n";
    return 0;
}

int cmdRun(const std::string& path, bool forceDynamic = false,
           sph::OptimizationLevel optLevel = sph::OptimizationLevel::O0, bool debugInfo = false,
           const sph::Target& target = sph::Target::getHostTarget()) {
    if (target.isCrossCompileFromHost()) {
        std::cerr << "error: 'run' command cannot execute binaries cross-compiled for target '"
                  << target.getCanonicalTriple() << "' on host system '"
                  << sph::Target::getHostTarget().getCanonicalTriple() << "'\n";
        std::cerr << "note: use 'sphc build --target=" << target.getShortName() << " " << path << "' to produce target binary\n";
        return 1;
    }

    std::filesystem::path srcPath(path);
    std::string ext = target.isWindows() ? "_run.exe" : "_run";
    std::string tempExe = (std::filesystem::temp_directory_path() / (srcPath.stem().string() + ext)).string();

    int buildRes = cmdBuild(path, tempExe, forceDynamic, optLevel, debugInfo, target);
    if (buildRes != 0) {
        return buildRes;
    }

    std::string runCmd = "\"" + tempExe + "\"";
    int exitCode = std::system(runCmd.c_str());
#if !defined(_WIN32)
    if (WIFEXITED(exitCode)) {
        exitCode = WEXITSTATUS(exitCode);
    }
#endif
    std::filesystem::remove(tempExe);
    return exitCode;
}

int cmdTest(const std::string& path, sph::OptimizationLevel optLevel = sph::OptimizationLevel::O0, bool debugInfo = false,
            const sph::Target& target = sph::Target::getHostTarget()) {
    if (!path.empty()) {
        std::cout << "Running test: " << path << "\n";
        return cmdRun(path, false, optLevel, debugInfo, target);
    }
    std::filesystem::path exeDir = getExecutableDir();
    std::vector<std::filesystem::path> testBins = {
        exeDir / "sph_tests.exe",
        exeDir / "sph_tests",
        exeDir.parent_path() / "build" / "bin" / "sph_tests.exe",
        "build/bin/sph_tests.exe",
        "bin/sph_tests.exe"
    };
    for (const auto& tb : testBins) {
        if (std::filesystem::exists(tb)) {
            std::cout << "Executing SantientPHP test suite (" << tb.string() << ")...\n";
            std::string cmd = "\"" + tb.string() + "\"";
            return std::system(cmd.c_str());
        }
    }
    std::cout << "No test suite executable found. Specify a test file: sphc test <test_file.sph>\n";
    return 1;
}

int cmdClean() {
    std::cout << "Cleaning SantientPHP build artifacts...\n";
    std::error_code ec;
    size_t removedCount = 0;
    for (const auto& entry : std::filesystem::directory_iterator(".")) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        if (ext == ".asm" || ext == ".ll" || ext == ".o" || ext == ".obj") {
            std::filesystem::remove(entry.path(), ec);
            if (!ec) {
                std::cout << "  removed: " << entry.path().filename().string() << "\n";
                ++removedCount;
            }
        }
    }
    if (std::filesystem::exists(".sph_cache")) {
        std::filesystem::remove_all(".sph_cache", ec);
        std::cout << "  removed: .sph_cache/\n";
        ++removedCount;
    }
    std::cout << "Clean complete (" << removedCount << " items removed).\n";
    return 0;
}

int cmdDoc(const std::string& inputPath, const std::string& outputPath, std::string_view format,
           const std::string& docTitle) {
    sph::SourceManager sourceManager;
    sph::DocExtractor extractor(sourceManager);
    sph::DocProject project;

    if (!docTitle.empty()) {
        project.title = docTitle;
    } else {
        auto manifest = sph::Manifest::findInPath(".");
        if (manifest && !manifest->projectName.empty()) {
            project.title = manifest->projectName + " API Reference";
            project.version = manifest->version;
        } else {
            project.title = "SantientPHP API Reference";
        }
    }

    std::vector<std::filesystem::path> filesToDoc;

    if (!inputPath.empty() && std::filesystem::is_directory(inputPath)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(inputPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".sph") {
                filesToDoc.push_back(entry.path());
            }
        }
    } else if (!inputPath.empty() && std::filesystem::exists(inputPath)) {
        filesToDoc.push_back(inputPath);
    } else {
        // Look in current dir / src
        if (std::filesystem::exists("src") && std::filesystem::is_directory("src")) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator("src")) {
                if (entry.is_regular_file() && entry.path().extension() == ".sph") {
                    filesToDoc.push_back(entry.path());
                }
            }
        } else {
            for (const auto& entry : std::filesystem::directory_iterator(".")) {
                if (entry.is_regular_file() && entry.path().extension() == ".sph") {
                    filesToDoc.push_back(entry.path());
                }
            }
        }
    }

    if (filesToDoc.empty()) {
        std::cerr << "error: no .sph source files found to document\n";
        return 1;
    }

    for (const auto& p : filesToDoc) {
        auto fileIdOpt = sourceManager.loadFile(p);
        if (!fileIdOpt) {
            std::cerr << "warning: could not load " << p.string() << "\n";
            continue;
        }

        sph::DiagnosticEmitter emitter(sourceManager);
        sph::Parser parser(*fileIdOpt, sourceManager, &emitter);
        auto program = parser.parseProgram();
        if (!program || emitter.hasErrors()) {
            std::cerr << "warning: errors parsing " << p.string() << ", documentation may be partial\n";
        }

        if (program) {
            auto mod = extractor.extractModule(*fileIdOpt, *program);
            project.modules.push_back(std::move(mod));
        }
    }

    std::string fmt = format.empty() ? "html" : std::string(format);
    std::filesystem::path out = outputPath;
    if (out.empty()) {
        if (filesToDoc.size() == 1 && !std::filesystem::is_directory(inputPath) && !inputPath.empty()) {
            std::string ext = (fmt == "md" || fmt == "markdown") ? ".md" : ".html";
            out = std::filesystem::path("docs") / (std::filesystem::path(inputPath).stem().string() + ext);
        } else {
            std::string ext = (fmt == "md" || fmt == "markdown") ? "index.md" : "index.html";
            out = std::filesystem::path("docs") / ext;
        }
    }

    sph::DocGenerator generator;
    if (!generator.generateToFile(project, out, fmt)) {
        std::cerr << "error: failed to write documentation to '" << out.string() << "'\n";
        return 1;
    }

    std::cout << "sphc: generated documentation (" << project.modules.size() << " module(s)) in " << out.string() << "\n";
    return 0;
}

int cmdNew(const std::string& projectName) {
    if (projectName.empty()) {
        std::cerr << "error: missing project name for 'new'\n";
        return 1;
    }
    std::string err;
    if (!sph::PackageManager::createProject(projectName, ".", err)) {
        std::cerr << "error: " << err << "\n";
        return 1;
    }
    std::cout << "sphc: created project '" << projectName << "'\n";
    return 0;
}

int cmdInit() {
    std::string err;
    if (!sph::PackageManager::initProject(".", err)) {
        std::cerr << "error: " << err << "\n";
        return 1;
    }
    std::cout << "sphc: initialized project in current directory\n";
    return 0;
}

int cmdPkg(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: sphc pkg <subcommand> [options]\n\n"
                  << "Subcommands:\n"
                  << "  add <name> [--path=<path> | --git=<url>]   Add a dependency\n"
                  << "  list                                      List dependencies\n";
        return 1;
    }

    std::string_view subcmd = argv[2];
    std::string err;

    if (subcmd == "list") {
        auto deps = sph::PackageManager::listDependencies(".", err);
        if (!err.empty()) {
            std::cerr << "error: " << err << "\n";
            return 1;
        }
        std::cout << "Dependencies (" << deps.size() << "):\n";
        for (const auto& d : deps) {
            std::cout << "  " << d.name;
            if (!d.path.empty()) std::cout << " (path: " << d.path << ")";
            if (!d.gitUrl.empty()) std::cout << " (git: " << d.gitUrl << ")";
            if (!d.version.empty()) std::cout << " [version: " << d.version << "]";
            std::cout << "\n";
        }
        return 0;
    }

    if (subcmd == "add") {
        if (argc < 4) {
            std::cerr << "error: missing dependency name for 'pkg add'\n";
            return 1;
        }
        std::string depName = argv[3];
        std::string loc;
        bool isGit = false;

        for (int i = 4; i < argc; ++i) {
            std::string_view arg = argv[i];
            if (arg.rfind("--path=", 0) == 0) {
                loc = std::string(arg.substr(7));
                isGit = false;
            } else if (arg.rfind("--git=", 0) == 0) {
                loc = std::string(arg.substr(6));
                isGit = true;
            }
        }

        if (loc.empty()) {
            std::cerr << "error: must specify --path=<path> or --git=<url>\n";
            return 1;
        }

        if (!sph::PackageManager::addDependency(".", depName, loc, isGit, err)) {
            std::cerr << "error: " << err << "\n";
            return 1;
        }

        std::cout << "sphc: added dependency '" << depName << "'\n";
        return 0;
    }

    std::cerr << "error: unknown pkg subcommand '" << subcmd << "'\n";
    return 1;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printHelp();
        return 0;
    }

    std::string_view arg1 = argv[1];

    if (arg1 == "-v" || arg1 == "--version") {
        printVersion();
        return 0;
    }

    if (arg1 == "-h" || arg1 == "--help") {
        printHelp();
        return 0;
    }

    if (arg1 == "targets" || arg1 == "--list-targets") {
        printTargets();
        return 0;
    }

    if (arg1 == "doctor") {
        sph::Doctor doctor;
        return doctor.report(std::cout);
    }

    if (arg1 == "lsp") {
        sph::lsp::LspServer server;
        return server.run(std::cin, std::cout);
    }

    if (arg1 == "init") {
        return cmdInit();
    }

    if (arg1 == "new") {
        if (argc < 3) {
            std::cerr << "error: missing project name for 'new'\n";
            return 1;
        }
        return cmdNew(argv[2]);
    }

    if (arg1 == "pkg") {
        return cmdPkg(argc, argv);
    }

    std::string srcFile;
    std::string outPath;
    std::string targetStr;
    bool forceDynamic = false;
    sph::OptimizationLevel optLevel = sph::OptimizationLevel::O0;
    bool optSpecified = false;
    bool debugInfo = false;
    bool checkOnly = false;
    bool toStdout = false;
    std::string docFormat = "html";
    std::string docTitle;

    for (int i = 2; i < argc; ++i) {
        std::string_view a = argv[i];
        if (a == "-o" && i + 1 < argc) {
            outPath = argv[++i];
        } else if ((a == "-t" || a == "--target") && i + 1 < argc) {
            targetStr = argv[++i];
        } else if (a.rfind("--target=", 0) == 0) {
            targetStr = std::string(a.substr(9));
        } else if (a.rfind("--format=", 0) == 0) {
            docFormat = std::string(a.substr(9));
        } else if (a.rfind("--title=", 0) == 0) {
            docTitle = std::string(a.substr(8));
        } else if (a == "--list-targets") {
            printTargets();
            return 0;
        } else if (a == "--check") {
            checkOnly = true;
        } else if (a == "--stdout") {
            toStdout = true;
        } else if (a == "-g" || a == "-g1" || a == "-g2" || a == "-g3") {
            debugInfo = true;
        } else if (a == "-g0") {
            debugInfo = false;
        } else if (a == "-O0") {
            optLevel = sph::OptimizationLevel::O0;
            optSpecified = true;
        } else if (a == "-O1") {
            optLevel = sph::OptimizationLevel::O1;
            optSpecified = true;
        } else if (a == "-O2" || a == "-O") {
            optLevel = sph::OptimizationLevel::O2;
            optSpecified = true;
        } else if (a == "-O3") {
            optLevel = sph::OptimizationLevel::O3;
            optSpecified = true;
        } else if (a == "-Os") {
            optLevel = sph::OptimizationLevel::Os;
            optSpecified = true;
        } else if (a == "--mode=dynamic" || a == "--compat-php") {
            forceDynamic = true;
        } else if (a == "--mode=transitional") {
            forceDynamic = true;
        } else if (a == "--mode=strict") {
            forceDynamic = false;
        } else if (!a.empty() && a[0] != '-' && srcFile.empty()) {
            srcFile = std::string(a);
        }
    }

    if (targetStr.empty()) {
        auto manifest = sph::Manifest::findInPath(".");
        if (manifest && !manifest->targetTriple.empty()) {
            targetStr = manifest->targetTriple;
        }
    }

    sph::Target target = sph::Target::getHostTarget();
    if (!targetStr.empty()) {
        auto parsed = sph::Target::parse(targetStr);
        if (!parsed) {
            std::cerr << "error: unknown or unsupported target triple '" << targetStr << "'\n";
            std::cerr << "note: run 'sphc --list-targets' to view all supported targets\n";
            return 1;
        }
        target = *parsed;
    }

    if (!optSpecified) {
        auto manifest = sph::Manifest::findInPath(".");
        if (manifest && !manifest->optimization.empty()) {
            if (manifest->optimization == "0" || manifest->optimization == "O0") optLevel = sph::OptimizationLevel::O0;
            else if (manifest->optimization == "1" || manifest->optimization == "O1") optLevel = sph::OptimizationLevel::O1;
            else if (manifest->optimization == "2" || manifest->optimization == "O2") optLevel = sph::OptimizationLevel::O2;
            else if (manifest->optimization == "3" || manifest->optimization == "O3") optLevel = sph::OptimizationLevel::O3;
            else if (manifest->optimization == "s" || manifest->optimization == "Os") optLevel = sph::OptimizationLevel::Os;
        }
    }

    if (!debugInfo) {
        auto manifest = sph::Manifest::findInPath(".");
        if (manifest && (manifest->buildMode == "debug" || manifest->optimization == "debug")) {
            debugInfo = true;
        }
    }

    if (srcFile.empty() && (arg1 == "check" || arg1 == "ir" || arg1 == "emit-asm" ||
                            arg1 == "emit-llvm" || arg1 == "build" || arg1 == "run" || arg1 == "fmt" ||
                            arg1 == "inspect")) {
        auto manifest = sph::Manifest::findInPath(".");
        if (manifest && !manifest->entry.empty()) {
            srcFile = manifest->entry;
            if (manifest->isDynamicMode()) {
                forceDynamic = true;
            }
        }
    }

    if (arg1 == "inspect") {
        if (srcFile.empty()) {
            std::cerr << "error: missing source file argument for 'inspect'\n";
            return 1;
        }
        std::string stage;
        for (int i = 2; i < argc; ++i) {
            std::string_view a = argv[i];
            if (a == "--tokens" || a == "--ast" || a == "--ir" || a == "--asm" || a == "--llvm" ||
                a == "-tokens" || a == "-ast" || a == "-ir" || a == "-asm" || a == "-llvm" ||
                a == "tokens" || a == "ast" || a == "ir" || a == "asm" || a == "llvm") {
                stage = std::string(a);
            }
        }
        return cmdInspect(srcFile, stage, forceDynamic, optLevel, debugInfo, target);
    }

    if (arg1 == "tokens") {
        if (srcFile.empty()) {
            std::cerr << "error: missing source file argument for 'tokens'\n";
            return 1;
        }
        return cmdTokens(srcFile);
    }

    if (arg1 == "ast") {
        if (srcFile.empty()) {
            std::cerr << "error: missing source file argument for 'ast'\n";
            return 1;
        }
        return cmdAst(srcFile);
    }

    if (arg1 == "check") {
        if (srcFile.empty()) {
            std::cerr << "error: missing source file argument for 'check'\n";
            return 1;
        }
        return cmdCheck(srcFile, forceDynamic, optLevel);
    }

    if (arg1 == "ir") {
        if (srcFile.empty()) {
            std::cerr << "error: missing source file argument for 'ir'\n";
            return 1;
        }
        return cmdIr(srcFile, forceDynamic, optLevel);
    }

    if (arg1 == "emit-asm") {
        if (srcFile.empty()) {
            std::cerr << "error: missing source file argument for 'emit-asm'\n";
            return 1;
        }
        return cmdEmitAsm(srcFile, outPath, forceDynamic, optLevel, debugInfo, target);
    }

    if (arg1 == "emit-llvm") {
        if (srcFile.empty()) {
            std::cerr << "error: missing source file argument for 'emit-llvm'\n";
            return 1;
        }
        return cmdEmitLlvm(srcFile, outPath, forceDynamic, optLevel, debugInfo, target);
    }

    if (arg1 == "build") {
        if (srcFile.empty()) {
            std::cerr << "error: missing source file argument for 'build'\n";
            return 1;
        }
        return cmdBuild(srcFile, outPath, forceDynamic, optLevel, debugInfo, target);
    }

    if (arg1 == "run") {
        if (srcFile.empty()) {
            std::cerr << "error: missing source file argument for 'run'\n";
            return 1;
        }
        return cmdRun(srcFile, forceDynamic, optLevel, debugInfo, target);
    }

    if (arg1 == "test") {
        return cmdTest(srcFile, optLevel, debugInfo, target);
    }

    if (arg1 == "clean") {
        return cmdClean();
    }

    if (arg1 == "fmt") {
        if (srcFile.empty()) {
            std::cerr << "error: missing source file argument for 'fmt'\n";
            return 1;
        }
        return cmdFmt(srcFile, checkOnly, toStdout);
    }

    if (arg1 == "doc") {
        return cmdDoc(srcFile, outPath, docFormat, docTitle);
    }

    std::cerr << "error: unknown command or option '" << arg1 << "'\n";
    printHelp();
    return 1;
}
