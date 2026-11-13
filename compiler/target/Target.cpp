#include "Target.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace sph {

namespace {

std::string toLower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

} // namespace

Target::Target() : Target(getHostTarget()) {}

Target::Target(TargetArch arch, TargetOS os, TargetVendor vendor, TargetEnvironment env,
               std::string rawTriple)
    : m_arch(arch), m_os(os), m_vendor(vendor), m_env(env), m_rawTriple(std::move(rawTriple)) {}

TargetCallingConvention Target::getCallingConvention() const noexcept {
    if (m_arch == TargetArch::AArch64) {
        return TargetCallingConvention::AAPCS64;
    }
    if (m_os == TargetOS::Windows) {
        return TargetCallingConvention::Win64;
    }
    return TargetCallingConvention::SysV64;
}

std::optional<Target> Target::parse(const std::string& tripleStr) {
    std::string s = toLower(tripleStr);
    // Trim whitespace
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();

    if (s.empty()) {
        return std::nullopt;
    }

    // Direct short names
    if (s == "host") {
        return getHostTarget();
    }
    if (s == "x86_64-windows" || s == "x86_64-win" || s == "windows" || s == "win64") {
        return Target(TargetArch::X86_64, TargetOS::Windows, TargetVendor::W64, TargetEnvironment::GNU, "x86_64-windows");
    }
    if (s == "x86_64-linux" || s == "linux-x86_64" || s == "linux64" || s == "linux") {
        return Target(TargetArch::X86_64, TargetOS::Linux, TargetVendor::Unknown, TargetEnvironment::GNU, "x86_64-linux");
    }
    if (s == "aarch64-linux" || s == "arm64-linux" || s == "linux-arm64" || s == "linux-aarch64") {
        return Target(TargetArch::AArch64, TargetOS::Linux, TargetVendor::Unknown, TargetEnvironment::GNU, "aarch64-linux");
    }
    if (s == "aarch64-macos" || s == "arm64-macos" || s == "macos-arm64" || s == "macos-aarch64" || s == "macos" || s == "darwin-arm64") {
        return Target(TargetArch::AArch64, TargetOS::MacOS, TargetVendor::Apple, TargetEnvironment::Darwin, "aarch64-macos");
    }
    if (s == "x86_64-macos" || s == "macos-x86_64" || s == "darwin-x86_64") {
        return Target(TargetArch::X86_64, TargetOS::MacOS, TargetVendor::Apple, TargetEnvironment::Darwin, "x86_64-macos");
    }
    if (s == "aarch64-windows" || s == "arm64-windows" || s == "win-arm64") {
        return Target(TargetArch::AArch64, TargetOS::Windows, TargetVendor::PC, TargetEnvironment::MSVC, "aarch64-windows");
    }

    // Parse multi-component triples (e.g. x86_64-w64-windows-gnu, x86_64-unknown-linux-gnu, arm64-apple-macosx)
    auto parts = split(s, '-');
    if (parts.size() < 2 || parts.size() > 4) {
        return std::nullopt;
    }

    TargetArch arch = TargetArch::Unknown;
    if (parts[0] == "x86_64" || parts[0] == "amd64" || parts[0] == "x64") {
        arch = TargetArch::X86_64;
    } else if (parts[0] == "aarch64" || parts[0] == "arm64") {
        arch = TargetArch::AArch64;
    } else {
        return std::nullopt;
    }

    TargetOS os = TargetOS::Unknown;
    TargetVendor vendor = TargetVendor::Unknown;
    TargetEnvironment env = TargetEnvironment::None;

    if (parts.size() == 2) {
        // arch-os
        if (parts[1] == "windows" || parts[1] == "win32" || parts[1] == "mingw32") {
            os = TargetOS::Windows;
            vendor = TargetVendor::W64;
            env = TargetEnvironment::GNU;
        } else if (parts[1] == "linux" || parts[1] == "gnu") {
            os = TargetOS::Linux;
            env = TargetEnvironment::GNU;
        } else if (parts[1] == "darwin" || parts[1] == "macos" || parts[1] == "macosx") {
            os = TargetOS::MacOS;
            vendor = TargetVendor::Apple;
            env = TargetEnvironment::Darwin;
        } else {
            return std::nullopt;
        }
    } else if (parts.size() == 3) {
        // arch-vendor-os or arch-os-env
        if (parts[1] == "apple" || parts[2] == "darwin" || parts[2] == "macosx") {
            os = TargetOS::MacOS;
            vendor = TargetVendor::Apple;
            env = TargetEnvironment::Darwin;
        } else if (parts[2] == "windows" || parts[2] == "mingw32") {
            os = TargetOS::Windows;
            vendor = (parts[1] == "w64" ? TargetVendor::W64 : TargetVendor::PC);
            env = TargetEnvironment::GNU;
        } else if (parts[1] == "linux" || parts[2] == "linux") {
            os = TargetOS::Linux;
            env = TargetEnvironment::GNU;
        } else if (parts[1] == "unknown" && parts[2] == "gnu") {
            os = TargetOS::Linux;
            env = TargetEnvironment::GNU;
        } else {
            return std::nullopt;
        }
    } else if (parts.size() == 4) {
        // arch-vendor-os-env (e.g., x86_64-pc-windows-gnu, aarch64-unknown-linux-gnu)
        if (parts[1] == "apple") vendor = TargetVendor::Apple;
        else if (parts[1] == "w64") vendor = TargetVendor::W64;
        else if (parts[1] == "pc") vendor = TargetVendor::PC;

        if (parts[2] == "windows" || parts[2] == "win32" || parts[2] == "mingw32") os = TargetOS::Windows;
        else if (parts[2] == "linux") os = TargetOS::Linux;
        else if (parts[2] == "darwin" || parts[2] == "macosx") os = TargetOS::MacOS;
        else return std::nullopt;

        if (parts[3] == "gnu") env = TargetEnvironment::GNU;
        else if (parts[3] == "msvc") env = TargetEnvironment::MSVC;
        else if (parts[3] == "darwin") env = TargetEnvironment::Darwin;
    }

    if (arch == TargetArch::Unknown || os == TargetOS::Unknown) {
        return std::nullopt;
    }

    return Target(arch, os, vendor, env, tripleStr);
}

bool Target::isValidTriple(const std::string& tripleStr) {
    return parse(tripleStr).has_value();
}

Target Target::getHostTarget() {
#if defined(_WIN32)
    #if defined(_M_ARM64)
    return Target(TargetArch::AArch64, TargetOS::Windows, TargetVendor::PC, TargetEnvironment::MSVC, "aarch64-windows");
    #else
    return Target(TargetArch::X86_64, TargetOS::Windows, TargetVendor::W64, TargetEnvironment::GNU, "x86_64-windows");
    #endif
#elif defined(__APPLE__)
    #if defined(__aarch64__) || defined(__arm64__)
    return Target(TargetArch::AArch64, TargetOS::MacOS, TargetVendor::Apple, TargetEnvironment::Darwin, "aarch64-macos");
    #else
    return Target(TargetArch::X86_64, TargetOS::MacOS, TargetVendor::Apple, TargetEnvironment::Darwin, "x86_64-macos");
    #endif
#elif defined(__linux__)
    #if defined(__aarch64__)
    return Target(TargetArch::AArch64, TargetOS::Linux, TargetVendor::Unknown, TargetEnvironment::GNU, "aarch64-linux");
    #else
    return Target(TargetArch::X86_64, TargetOS::Linux, TargetVendor::Unknown, TargetEnvironment::GNU, "x86_64-linux");
    #endif
#else
    return Target(TargetArch::X86_64, TargetOS::Windows, TargetVendor::W64, TargetEnvironment::GNU, "x86_64-windows");
#endif
}

std::vector<Target> Target::getSupportedTargets() {
    return {
        Target(TargetArch::X86_64, TargetOS::Windows, TargetVendor::W64, TargetEnvironment::GNU, "x86_64-windows"),
        Target(TargetArch::X86_64, TargetOS::Linux, TargetVendor::Unknown, TargetEnvironment::GNU, "x86_64-linux"),
        Target(TargetArch::AArch64, TargetOS::Linux, TargetVendor::Unknown, TargetEnvironment::GNU, "aarch64-linux"),
        Target(TargetArch::AArch64, TargetOS::MacOS, TargetVendor::Apple, TargetEnvironment::Darwin, "aarch64-macos"),
        Target(TargetArch::X86_64, TargetOS::MacOS, TargetVendor::Apple, TargetEnvironment::Darwin, "x86_64-macos"),
        Target(TargetArch::AArch64, TargetOS::Windows, TargetVendor::PC, TargetEnvironment::MSVC, "aarch64-windows")
    };
}

std::vector<std::string> Target::getSupportedTripleStrings() {
    return {
        "x86_64-windows",
        "x86_64-linux",
        "aarch64-linux",
        "aarch64-macos",
        "x86_64-macos",
        "aarch64-windows"
    };
}

std::string Target::getCanonicalTriple() const {
    if (m_arch == TargetArch::X86_64) {
        if (m_os == TargetOS::Windows) return "x86_64-w64-windows-gnu";
        if (m_os == TargetOS::Linux) return "x86_64-unknown-linux-gnu";
        if (m_os == TargetOS::MacOS) return "x86_64-apple-darwin";
    } else if (m_arch == TargetArch::AArch64) {
        if (m_os == TargetOS::Linux) return "aarch64-unknown-linux-gnu";
        if (m_os == TargetOS::MacOS) return "arm64-apple-macosx";
        if (m_os == TargetOS::Windows) return "aarch64-pc-windows-msvc";
    }
    return m_rawTriple;
}

std::string Target::getShortName() const {
    std::string archStr = (m_arch == TargetArch::AArch64) ? "aarch64" : "x86_64";
    std::string osStr;
    switch (m_os) {
        case TargetOS::Windows: osStr = "windows"; break;
        case TargetOS::Linux: osStr = "linux"; break;
        case TargetOS::MacOS: osStr = "macos"; break;
        default: osStr = "unknown"; break;
    }
    return archStr + "-" + osStr;
}

std::string Target::getDescription() const {
    std::string desc;
    if (m_arch == TargetArch::X86_64) desc += "x86-64 (AMD64)";
    else if (m_arch == TargetArch::AArch64) desc += "AArch64 (ARM64)";
    else desc += "Unknown Architecture";

    desc += " on ";
    if (m_os == TargetOS::Windows) desc += "Microsoft Windows";
    else if (m_os == TargetOS::Linux) desc += "Linux";
    else if (m_os == TargetOS::MacOS) desc += "Apple macOS";
    else desc += "Unknown OS";

    if (m_env == TargetEnvironment::GNU) desc += " (MinGW/GNU ABI)";
    else if (m_env == TargetEnvironment::MSVC) desc += " (MSVC ABI)";
    else if (m_env == TargetEnvironment::Darwin) desc += " (Darwin ABI)";

    return desc;
}

bool Target::isHost() const noexcept {
    Target host = getHostTarget();
    return m_arch == host.m_arch && m_os == host.m_os;
}

bool Target::isCrossCompileFromHost() const noexcept {
    return !isHost();
}

std::string Target::getLLVMTriple() const {
    if (m_arch == TargetArch::X86_64) {
        if (m_os == TargetOS::Windows) return "x86_64-w64-windows-gnu";
        if (m_os == TargetOS::Linux) return "x86_64-unknown-linux-gnu";
        if (m_os == TargetOS::MacOS) return "x86_64-apple-macosx";
    } else if (m_arch == TargetArch::AArch64) {
        if (m_os == TargetOS::Linux) return "aarch64-unknown-linux-gnu";
        if (m_os == TargetOS::MacOS) return "arm64-apple-macosx";
        if (m_os == TargetOS::Windows) return "aarch64-pc-windows-msvc";
    }
    return "x86_64-w64-windows-gnu";
}

std::string Target::getDataLayout() const {
    if (m_arch == TargetArch::X86_64) {
        if (m_os == TargetOS::Windows) {
            return "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128";
        }
        if (m_os == TargetOS::Linux) {
            return "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128";
        }
        if (m_os == TargetOS::MacOS) {
            return "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128";
        }
    } else if (m_arch == TargetArch::AArch64) {
        if (m_os == TargetOS::Linux) {
            return "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128";
        }
        if (m_os == TargetOS::MacOS) {
            return "e-m:o-i64:64-i128:128-n32:64-S128";
        }
        if (m_os == TargetOS::Windows) {
            return "e-m:w-p:64:64-i32:32-i64:64-i128:128-n32:64-S128";
        }
    }
    return "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128";
}

std::string Target::getExecutableExtension() const {
    if (m_os == TargetOS::Windows) {
        return ".exe";
    }
    return "";
}

std::string Target::getObjectExtension() const {
    if (m_env == TargetEnvironment::MSVC) {
        return ".obj";
    }
    return ".o";
}

std::string Target::getSharedLibraryExtension() const {
    if (m_os == TargetOS::Windows) return ".dll";
    if (m_os == TargetOS::MacOS) return ".dylib";
    return ".so";
}

std::string Target::getRoDataSectionDirective() const {
    if (m_os == TargetOS::Windows) {
        return ".section .rdata,\"dr\"";
    }
    if (m_os == TargetOS::MacOS) {
        return ".section __TEXT,__cstring,cstring_literals";
    }
    return ".section .rodata";
}

std::string Target::getFunctionTypeDirective(const std::string& fnName) const {
    if (m_os == TargetOS::Windows) {
        return ".def " + fnName + "; .scl 2; .type 32; .endef";
    }
    if (m_os == TargetOS::Linux) {
        return ".type " + fnName + ", @function";
    }
    return "";
}

std::string Target::getDefaultLinker() const {
    if (m_os == TargetOS::Windows) {
        return "x86_64-w64-mingw32-gcc";
    }
    if (m_os == TargetOS::Linux) {
        if (m_arch == TargetArch::AArch64) return "aarch64-linux-gnu-gcc";
        return "x86_64-linux-gnu-gcc";
    }
    if (m_os == TargetOS::MacOS) {
        return "clang";
    }
    return "gcc";
}

std::string Target::getCrossCompilerPrefix() const {
    if (m_os == TargetOS::Windows) {
        return "x86_64-w64-mingw32-";
    }
    if (m_os == TargetOS::Linux) {
        if (m_arch == TargetArch::AArch64) return "aarch64-linux-gnu-";
        return "x86_64-linux-gnu-";
    }
    if (m_os == TargetOS::MacOS) {
        if (m_arch == TargetArch::AArch64) return "aarch64-apple-darwin-";
        return "x86_64-apple-darwin-";
    }
    return "";
}

bool Target::operator==(const Target& other) const noexcept {
    return m_arch == other.m_arch && m_os == other.m_os &&
           m_vendor == other.m_vendor && m_env == other.m_env;
}

bool Target::operator!=(const Target& other) const noexcept {
    return !(*this == other);
}

} // namespace sph

