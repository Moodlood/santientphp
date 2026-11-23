#include "Linker.hpp"
#include <chrono>
#include <cstdlib>
#include <sstream>
#include <thread>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static int runProcessDirect(const std::string& commandLine) {
    std::wstring wcmd(commandLine.begin(), commandLine.end());
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    std::vector<wchar_t> cmdBuf(wcmd.begin(), wcmd.end());
    cmdBuf.push_back(L'\0');

    if (!CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        return -1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(exitCode);
}
#else
#include <sys/wait.h>
#include <unistd.h>
#include <wordexp.h>

static int runProcessDirect(const std::string& commandLine) {
    wordexp_t p;
    if (wordexp(commandLine.c_str(), &p, WRDE_NOCMD) != 0) {
        return -1;
    }
    pid_t pid = fork();
    if (pid == 0) {
        execvp(p.we_wordv[0], p.we_wordv);
        _exit(127);
    }
    wordfree(&p);
    if (pid < 0) return -1;
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
#endif

namespace sph {

std::optional<std::string> Linker::findCrossToolchain(const Target& target) const {
    if (target.isHost()) {
        return "gcc";
    }

    std::vector<std::string> candidates = {
        target.getDefaultLinker(),
        target.getCrossCompilerPrefix() + "gcc",
        "clang"
    };

    for (const auto& cand : candidates) {
        if (cand.empty()) continue;
#if defined(_WIN32)
        std::string checkCmd = "where.exe " + cand + " >nul 2>&1";
#else
        std::string checkCmd = "which " + cand + " >/dev/null 2>&1";
#endif
        if (std::system(checkCmd.c_str()) == 0) {
            return cand;
        }
    }

    return std::nullopt;
}

bool Linker::linkExecutable(const std::filesystem::path& asmPath,
                           const std::filesystem::path& outputPath,
                           const std::filesystem::path& runtimeLibPath,
                           std::string& errorMsg,
                           const std::string& optLevel,
                           bool debugInfo,
                           const Target& target,
                           bool isGui,
                           const std::filesystem::path& guiLibPath,
                           const std::filesystem::path& resourceObjPath,
                           const std::vector<std::string>& extraLibs,
                           const std::vector<std::filesystem::path>& libSearchPaths) const {
    std::string tool;
    if (target.isHost()) {
        static std::string cachedHostTool;
        if (cachedHostTool.empty()) {
            cachedHostTool = "gcc";
#if defined(_WIN32)
            if (std::system("where.exe gcc >nul 2>&1") != 0) {
                if (std::filesystem::exists("C:\\msys64\\ucrt64\\bin\\gcc.exe")) {
                    cachedHostTool = "C:\\msys64\\ucrt64\\bin\\gcc.exe";
                } else if (std::filesystem::exists("C:\\msys64\\mingw64\\bin\\gcc.exe")) {
                    cachedHostTool = "C:\\msys64\\mingw64\\bin\\gcc.exe";
                }
            }
#endif
        }
        tool = cachedHostTool;
    } else {
        auto crossToolOpt = findCrossToolchain(target);
        if (!crossToolOpt) {
            errorMsg = "cross-compilation to target '" + target.getCanonicalTriple() +
                       "' requires a cross-compiler (such as " + target.getDefaultLinker() +
                       " or clang) which was not found in PATH. Target assembly was generated at: " +
                       asmPath.string();
            return false;
        }
        tool = *crossToolOpt;
    }

    std::ostringstream cmd;
    cmd << tool << " ";
    if (!target.isHost() && tool == "clang") {
        cmd << "--target=" << target.getLLVMTriple() << " ";
    }
    if (debugInfo) {
        cmd << "-g ";
    } else {
        cmd << "-s -pipe ";
    }
    if (!optLevel.empty()) {
        if (optLevel.front() == 'O') {
            cmd << "-" << optLevel << " ";
        } else {
            cmd << "-O" << optLevel << " ";
        }
    }
    for (const auto& searchPath : libSearchPaths) {
        cmd << "-L\"" << searchPath.string() << "\" ";
    }
    cmd << "\"" << asmPath.string() << "\" ";

    if (std::filesystem::exists(runtimeLibPath)) {
        cmd << "\"" << runtimeLibPath.string() << "\" ";
    }
    if (isGui && !guiLibPath.empty() && std::filesystem::exists(guiLibPath)) {
        cmd << "\"" << guiLibPath.string() << "\" ";
    }
    if (!resourceObjPath.empty() && std::filesystem::exists(resourceObjPath)) {
        cmd << "\"" << resourceObjPath.string() << "\" ";
    }
#if defined(_WIN32)
    if (isGui) {
        cmd << "-mwindows -static-libgcc -lws2_32 -lwinmm -lcomctl32 -lgdi32 -luser32 -lopengl32 -lole32 -luuid -lwinmm ";
    } else {
        cmd << "-static-libgcc -lws2_32 -lwinmm ";
    }
#else
    if (isGui) {
        cmd << "-no-pie -lpthread -ldl -lm -lGL ";
    } else {
        cmd << "-no-pie -lpthread -ldl -lm ";
    }
#endif

    for (const auto& lib : extraLibs) {
        cmd << "-l" << lib << " ";
    }

    cmd << "-o \"" << outputPath.string() << "\"";

    std::error_code ec;
    for (int r = 0; r < 5; ++r) {
        std::filesystem::remove(outputPath, ec);
        if (!std::filesystem::exists(outputPath)) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }

    int result = runProcessDirect(cmd.str());
    if (result != 0) {
        // Retry once after a brief delay in case antivirus or indexing locked the executable
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::filesystem::remove(outputPath, ec);
        result = runProcessDirect(cmd.str());
    }

    if (result != 0) {
        errorMsg = "linker command failed with exit code " + std::to_string(result) + ": " + cmd.str();
        return false;
    }

    if (!std::filesystem::exists(outputPath)) {
        errorMsg = "linker reported success but output executable not found: " + outputPath.string();
        return false;
    }

    return true;
}

} // namespace sph

