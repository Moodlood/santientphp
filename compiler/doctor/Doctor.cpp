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
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

namespace sph {

static std::filesystem::path getProcessExecutableDir() {
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

    // 3b. LLVM Tools (Optional for LLVM backend verification)
    std::string llcVer;
    bool hasLlc = findTool("llc", llcVer);
    if (hasLlc) {
        checks.push_back(ToolchainCheck{
            "Toolchain", "LLVM Tools (llc)", llcVer, true, true
        });
    } else {
        checks.push_back(ToolchainCheck{
            "Toolchain", "LLVM Tools (llc)", "Not found in PATH (optional for LLVM IR verification)", false, true
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

    bool sysLibsOk = hasKernel32 && hasWinHttp;
    checks.push_back(ToolchainCheck{
        "Libraries", "Windows Native Subsystems", sysLibDetails, sysLibsOk, false
    });
#endif

    // 7. SantientPHP Runtime Sources
    std::filesystem::path exeDir = getProcessExecutableDir();
    std::vector<std::filesystem::path> searchRoots = {
        exeDir,
        exeDir.parent_path(),
        exeDir.parent_path().parent_path(),
        "C:/Program Files/SantientPHP",
        "C:/SantientPHP"
    };
    if (const char* sphHome = std::getenv("SPH_HOME")) {
        searchRoots.insert(searchRoots.begin(), sphHome);
    }
    if (const char* sphHome2 = std::getenv("SANTIENTPHP_HOME")) {
        searchRoots.insert(searchRoots.begin(), sphHome2);
    }

    bool hasRuntimeC = false;
    bool hasRuntimeH = false;
    bool hasRuntimeLib = false;

    std::error_code ec;
    for (const auto& root : searchRoots) {
        if (!hasRuntimeC && (std::filesystem::exists(root / "runtime" / "core" / "runtime.c", ec) ||
                             std::filesystem::exists(root / "runtime" / "core" / "core.c", ec))) hasRuntimeC = true;
        if (!hasRuntimeH && (std::filesystem::exists(root / "runtime" / "core" / "runtime.h", ec) ||
                             std::filesystem::exists(root / "runtime" / "core" / "core.h", ec))) hasRuntimeH = true;
        if (!hasRuntimeLib && (std::filesystem::exists(root / "lib" / "libsph_runtime.a", ec) ||
                               std::filesystem::exists(root / "lib" / "libsph_core.a", ec))) hasRuntimeLib = true;
        if (!hasRuntimeLib && (std::filesystem::exists(root / "build" / "lib" / "libsph_runtime.a", ec) ||
                               std::filesystem::exists(root / "build" / "lib" / "libsph_core.a", ec))) hasRuntimeLib = true;
        if (!hasRuntimeLib && (std::filesystem::exists(root / "libsph_runtime.a", ec) ||
                               std::filesystem::exists(root / "libsph_core.a", ec))) hasRuntimeLib = true;
    }
    if (!hasRuntimeC) hasRuntimeC = std::filesystem::exists("runtime/core/runtime.c", ec) || std::filesystem::exists("runtime/core/core.c", ec) || std::filesystem::exists("../runtime/core/runtime.c", ec);
    if (!hasRuntimeH) hasRuntimeH = std::filesystem::exists("runtime/core/runtime.h", ec) || std::filesystem::exists("runtime/core/core.h", ec) || std::filesystem::exists("../runtime/core/runtime.h", ec);
    if (!hasRuntimeLib) hasRuntimeLib = std::filesystem::exists("build/lib/libsph_runtime.a", ec) || std::filesystem::exists("build/lib/libsph_core.a", ec) || std::filesystem::exists("lib/libsph_runtime.a", ec) || std::filesystem::exists("../build/lib/libsph_runtime.a", ec);

    if (hasRuntimeLib || (hasRuntimeC && hasRuntimeH)) {
        std::string rtInfo;
        if (hasRuntimeLib) {
            rtInfo = "Precompiled runtime libraries present (libsph_runtime.a / libsph_core.a)";
            if (hasRuntimeC && hasRuntimeH) {
                rtInfo += " (modular runtime sources available)";
            }
        } else {
            rtInfo = "Modular runtime sources present (core, async, compat)";
        }
        checks.push_back(ToolchainCheck{
            "Runtime", "SantientPHP Core Runtime", rtInfo, true, false
        });
    } else {
        checks.push_back(ToolchainCheck{
            "Runtime", "SantientPHP Core Runtime", "Missing libsph_runtime.a or runtime sources", false, false
        });
    }

    // 7b. SantientPHP Standard Library
    bool hasStdlib = false;
    std::filesystem::path foundStdlibPath;
    for (const auto& root : searchRoots) {
        if (std::filesystem::exists(root / "stdlib" / "core.sph", ec)) {
            hasStdlib = true;
            foundStdlibPath = root / "stdlib";
            break;
        }
    }
    if (!hasStdlib) {
        if (std::filesystem::exists("stdlib/core.sph", ec)) {
            hasStdlib = true;
            foundStdlibPath = "stdlib";
        } else if (std::filesystem::exists("../stdlib/core.sph", ec)) {
            hasStdlib = true;
            foundStdlibPath = "../stdlib";
        }
    }

    if (hasStdlib) {
        checks.push_back(ToolchainCheck{
            "Stdlib", "SantientPHP Standard Library (stdlib/)", "Found at " + foundStdlibPath.lexically_normal().string(), true, false
        });
    } else {
        checks.push_back(ToolchainCheck{
            "Stdlib", "SantientPHP Standard Library (stdlib/)", "Missing standard library (core.sph not found)", false, false
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

