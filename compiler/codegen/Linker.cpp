#include "Linker.hpp"
#include <chrono>
#include <cstdlib>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static std::string findExecutableInPath(const std::string& name) {
    char buf[MAX_PATH];
    DWORD len = SearchPathA(NULL, name.c_str(), ".exe", MAX_PATH, buf, NULL);
    if (len > 0 && len < MAX_PATH) {
        return std::string(buf, len);
    }
    return "";
}
#else
#include <sys/wait.h>
#include <unistd.h>
#include <wordexp.h>

static std::string findExecutableInPath(const std::string& name) {
    if (name.find('/') != std::string::npos) {
        return access(name.c_str(), X_OK) == 0 ? name : "";
    }
    const char* pathEnv = std::getenv("PATH");
    if (!pathEnv) return "";
    std::stringstream ss(pathEnv);
    std::string dir;
    while (std::getline(ss, dir, ':')) {
        if (dir.empty()) continue;
        std::string full = dir + "/" + name;
        if (access(full.c_str(), X_OK) == 0) {
            return full;
        }
    }
    return "";
}
#endif

#if defined(_WIN32)

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
        if (!findExecutableInPath(cand).empty()) {
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
    std::vector<std::filesystem::path> libs;
    if (!runtimeLibPath.empty()) {
        libs.push_back(runtimeLibPath);
    }
    bool linkNet = runtimeLibPath.filename() != "libsph_core.a";
    return linkExecutable(asmPath, outputPath, libs, errorMsg, optLevel, debugInfo,
                          target, isGui, guiLibPath, resourceObjPath, extraLibs, libSearchPaths, linkNet);
}

bool Linker::linkExecutable(const std::filesystem::path& asmPath,
                           const std::filesystem::path& outputPath,
                           const std::vector<std::filesystem::path>& runtimeLibPaths,
                           std::string& errorMsg,
                           const std::string& /*optLevel*/,
                           bool debugInfo,
                           const Target& target,
                           bool isGui,
                           const std::filesystem::path& guiLibPath,
                           const std::filesystem::path& resourceObjPath,
                           const std::vector<std::string>& extraLibs,
                           const std::vector<std::filesystem::path>& libSearchPaths,
                           bool linkNetwork) const {
    std::string tool;
    if (target.isHost()) {
        static std::string cachedHostTool;
        if (cachedHostTool.empty()) {
            std::string found = findExecutableInPath("gcc");
            if (!found.empty()) {
                cachedHostTool = found;
            } else {
                cachedHostTool = "gcc";
#if defined(_WIN32)
                if (std::filesystem::exists("C:\\msys64\\ucrt64\\bin\\gcc.exe")) {
                    cachedHostTool = "C:\\msys64\\ucrt64\\bin\\gcc.exe";
                } else if (std::filesystem::exists("C:\\msys64\\mingw64\\bin\\gcc.exe")) {
                    cachedHostTool = "C:\\msys64\\mingw64\\bin\\gcc.exe";
                }
#endif
            }
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
    cmd << "\"" << tool << "\" ";
    if (!target.isHost() && tool == "clang") {
        cmd << "--target=" << target.getLLVMTriple() << " ";
    }
    if (debugInfo) {
        cmd << "-g ";
    } else {
        cmd << "-s -pipe ";
        if (target.getOS() == TargetOS::MacOS) {
            cmd << "-Wl,-dead_strip ";
        } else {
            cmd << "-Wl,--gc-sections ";
        }
    }
#ifdef SPH_SANITIZE_LINK_FLAGS
    if (target.isHost()) {
        cmd << SPH_SANITIZE_LINK_FLAGS << " ";
    }
#endif
    for (const auto& searchPath : libSearchPaths) {
        cmd << "-L\"" << searchPath.string() << "\" ";
    }
    cmd << "\"" << asmPath.string() << "\" ";

    for (const auto& rtLib : runtimeLibPaths) {
        if (!rtLib.empty() && std::filesystem::exists(rtLib)) {
            cmd << "\"" << rtLib.string() << "\" ";
        }
    }
    if (isGui && !guiLibPath.empty() && std::filesystem::exists(guiLibPath)) {
        cmd << "\"" << guiLibPath.string() << "\" ";
    }
    if (!resourceObjPath.empty() && std::filesystem::exists(resourceObjPath)) {
        cmd << "\"" << resourceObjPath.string() << "\" ";
    }
#if defined(_WIN32)
    if (isGui) {
        cmd << "-mwindows -static-libgcc -lcomctl32 -lgdi32 -luser32 -lopengl32 -lole32 -luuid -lwinmm ";
        if (linkNetwork) {
            cmd << "-lws2_32 ";
        }
    } else if (linkNetwork) {
        cmd << "-static-libgcc -lws2_32 ";
    } else {
        cmd << "-static-libgcc ";
    }
#else
    if (isGui) {
        if (target.getOS() == TargetOS::MacOS) {
            cmd << "-lpthread -ldl -lm ";
        } else {
            cmd << "-no-pie -lpthread -ldl -lm -lX11 -lGL ";
        }
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

