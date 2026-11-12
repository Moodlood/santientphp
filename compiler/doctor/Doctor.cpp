#include "Doctor.hpp"
#include <array>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace sph {

static std::string execCommand(const std::string& cmd) {
    std::string result;
    std::array<char, 256> buffer{};
#if defined(_WIN32)
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) {
        return "";
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result += buffer.data();
    }
#if defined(_WIN32)
    _pclose(pipe);
#else
    pclose(pipe);
#endif

    // Trim trailing newline
    while (!result.empty() && (result.back() == '\r' || result.back() == '\n')) {
        result.pop_back();
    }
    return result;
}

bool Doctor::findTool(const std::string& tool, std::string& versionOut) {
#if defined(_WIN32)
    std::string checkCmd = "where.exe " + tool + " 2>nul";
#else
    std::string checkCmd = "which " + tool + " 2>/dev/null";
#endif
    std::string path = execCommand(checkCmd);
    if (path.empty()) {
        return false;
    }

    // Get first line of tool --version
    std::string verCmd = tool + " --version 2>&1";
    std::string verRaw = execCommand(verCmd);
    if (!verRaw.empty()) {
        size_t nl = verRaw.find('\n');
        versionOut = (nl != std::string::npos) ? verRaw.substr(0, nl) : verRaw;
        if (!versionOut.empty() && versionOut.back() == '\r') {
            versionOut.pop_back();
        }
    } else {
        versionOut = path;
    }
    return true;
}

bool Doctor::checkSystemLibrary(const std::string& dllName) {
#if defined(_WIN32)
    HMODULE hMod = LoadLibraryExA(dllName.c_str(), NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (hMod) {
        FreeLibrary(hMod);
        return true;
    }
    // Fallback: check C:\Windows\System32\<dllName>
    char sysDir[MAX_PATH];
    if (GetSystemDirectoryA(sysDir, MAX_PATH) > 0) {
        std::filesystem::path p = std::filesystem::path(sysDir) / dllName;
        if (std::filesystem::exists(p)) {
            return true;
        }
    }
    return false;
#else
    return true;
#endif
}

std::vector<ToolchainCheck> Doctor::runChecks() const {
    std::vector<ToolchainCheck> checks;

    // 1. Host OS & Architecture
#if defined(_WIN32)
    checks.push_back(ToolchainCheck{
        "Host", "Operating System", "Microsoft Windows (x86_64)", true, false
    });
#elif defined(__linux__)
    checks.push_back(ToolchainCheck{
        "Host", "Operating System", "Linux", true, false
    });
#elif defined(__APPLE__)
    checks.push_back(ToolchainCheck{
        "Host", "Operating System", "macOS", true, false
    });
#else
    checks.push_back(ToolchainCheck{
        "Host", "Operating System", "Unknown OS", true, false
    });
#endif

    // 2. C++ Compiler (GCC / Clang)
    std::string gccVer;
    bool hasGcc = findTool("gcc", gccVer);
    if (hasGcc) {
        checks.push_back(ToolchainCheck{
            "Compiler", "Host C/C++ Compiler (gcc)", gccVer, true, false
        });
    } else {
        std::string clangVer;
        bool hasClang = findTool("clang", clangVer);
        if (hasClang) {
            checks.push_back(ToolchainCheck{
                "Compiler", "Host C/C++ Compiler (clang)", clangVer, true, false
            });
        } else {
            checks.push_back(ToolchainCheck{
                "Compiler", "Host C/C++ Compiler", "Not found in PATH (GCC or Clang required)", false, false
            });
        }
    }

    // 3. Native Linker
    std::string ldVer;
    bool hasLd = findTool("ld", ldVer);
    if (hasLd) {
        checks.push_back(ToolchainCheck{
            "Toolchain", "Linker (ld)", ldVer, true, false
        });
    } else {
        checks.push_back(ToolchainCheck{
            "Toolchain", "Linker (ld)", "Not found in PATH", false, false
        });
    }

    // 4. Native Debugger
    std::string gdbVer;
    bool hasGdb = findTool("gdb", gdbVer);
    if (hasGdb) {
        checks.push_back(ToolchainCheck{
            "Toolchain", "Debugger (gdb)", gdbVer, true, true
        });
    } else {
        checks.push_back(ToolchainCheck{
            "Toolchain", "Debugger (gdb)", "Not found in PATH (optional for debugging)", false, true
        });
    }

    // 5. Version Control (Git)
    std::string gitVer;
    bool hasGit = findTool("git", gitVer);
    if (hasGit) {
        checks.push_back(ToolchainCheck{
            "Tools", "Version Control (git)", gitVer, true, true
        });
    } else {
        checks.push_back(ToolchainCheck{
            "Tools", "Version Control (git)", "Not found in PATH (optional for git packages)", false, true
        });
    }

    // 6. Windows System Libraries
#if defined(_WIN32)
    bool hasKernel32 = checkSystemLibrary("kernel32.dll");
    bool hasWinHttp = checkSystemLibrary("winhttp.dll");
    bool hasWinSqlite = checkSystemLibrary("winsqlite3.dll");

    std::string sysLibDetails;
    if (hasKernel32) sysLibDetails += "kernel32.dll (threading/atomics)";
    if (hasWinHttp) sysLibDetails += ", winhttp.dll (cURL/HTTP)";
    if (hasWinSqlite) sysLibDetails += ", winsqlite3.dll (PDO/SQLite)";

    bool sysLibsOk = hasKernel32 && hasWinHttp && hasWinSqlite;
    checks.push_back(ToolchainCheck{
        "Libraries", "Windows Native Subsystems", sysLibDetails, sysLibsOk, false
    });
#endif

    // 7. SantientPHP Runtime Sources
    bool hasRuntimeC = std::filesystem::exists("runtime/core/runtime.c");
    bool hasRuntimeH = std::filesystem::exists("runtime/core/runtime.h");
    bool hasRuntimeLib = std::filesystem::exists("build/lib/libsph_runtime.a") ||
                         std::filesystem::exists("lib/libsph_runtime.a");

    if (hasRuntimeC && hasRuntimeH) {
        std::string rtInfo = "runtime/core/runtime.c present";
        if (hasRuntimeLib) {
            rtInfo += " (precompiled libsph_runtime.a available)";
        }
        checks.push_back(ToolchainCheck{
            "Runtime", "SantientPHP Core Runtime", rtInfo, true, false
        });
    } else {
        checks.push_back(ToolchainCheck{
            "Runtime", "SantientPHP Core Runtime", "Missing runtime/core/runtime.c", false, false
        });
    }

    // 8. Cross-compilation toolchains (Optional)
    std::string linuxGcc;
    if (findTool("x86_64-linux-gnu-gcc", linuxGcc)) {
        checks.push_back(ToolchainCheck{
            "Cross-Target", "x86_64-linux toolchain", linuxGcc, true, true
        });
    } else {
        checks.push_back(ToolchainCheck{
            "Cross-Target", "x86_64-linux toolchain", "Not installed (optional)", false, true
        });
    }

    std::string armGcc;
    if (findTool("aarch64-linux-gnu-gcc", armGcc)) {
        checks.push_back(ToolchainCheck{
            "Cross-Target", "aarch64-linux toolchain", armGcc, true, true
        });
    } else {
        checks.push_back(ToolchainCheck{
            "Cross-Target", "aarch64-linux toolchain", "Not installed (optional)", false, true
        });
    }

    return checks;
}

int Doctor::report(std::ostream& out) const {
    auto checks = runChecks();

    out << "SantientPHP Doctor [v0.1.0]\n";
    out << "===========================\n";

    bool allRequiredHealthy = true;

    for (const auto& c : checks) {
        if (c.isHealthy) {
            out << "  [OK] " << c.name << ": " << c.details << "\n";
        } else if (c.isOptional) {
            out << "  [-]  " << c.name << ": " << c.details << "\n";
        } else {
            out << "  [!!] " << c.name << ": " << c.details << "\n";
            allRequiredHealthy = false;
        }
    }

    out << "\n";
    if (allRequiredHealthy) {
        out << "Status: All required toolchains and system dependencies are healthy.\n";
        return 0;
    } else {
        out << "Status: One or more required toolchain components are missing.\n";
        return 1;
    }
}

} // namespace sph

