#pragma once

#include <optional>
#include <string>
#include <vector>

namespace sph {

enum class TargetArch {
    X86_64,
    AArch64,
    Unknown
};

enum class TargetOS {
    Windows,
    Linux,
    MacOS,
    Unknown
};

enum class TargetVendor {
    PC,
    W64,
    Apple,
    Unknown
};

enum class TargetEnvironment {
    GNU,
    MSVC,
    Darwin,
    None
};

enum class TargetCallingConvention {
    Win64,    // RCX, RDX, R8, R9, 32-byte shadow space, XMM0-3
    SysV64,   // RDI, RSI, RDX, RCX, R8, R9, no shadow space, XMM0-7
    AAPCS64   // X0-X7, D0-D7
};

class Target {
public:
    Target();
    Target(TargetArch arch, TargetOS os, TargetVendor vendor, TargetEnvironment env,
           std::string rawTriple);

    // Parsing and Factory Methods
    static std::optional<Target> parse(const std::string& tripleStr);
    static bool isValidTriple(const std::string& tripleStr);
    static Target getHostTarget();
    static std::vector<Target> getSupportedTargets();
    static std::vector<std::string> getSupportedTripleStrings();

    // Query Methods
    [[nodiscard]] TargetArch getArch() const noexcept { return m_arch; }
    [[nodiscard]] TargetOS getOS() const noexcept { return m_os; }
    [[nodiscard]] TargetVendor getVendor() const noexcept { return m_vendor; }
    [[nodiscard]] TargetEnvironment getEnvironment() const noexcept { return m_env; }
    [[nodiscard]] TargetCallingConvention getCallingConvention() const noexcept;

    [[nodiscard]] const std::string& getRawTriple() const noexcept { return m_rawTriple; }
    [[nodiscard]] std::string getCanonicalTriple() const;
    [[nodiscard]] std::string getShortName() const;
    [[nodiscard]] std::string getDescription() const;

    [[nodiscard]] bool isWindows() const noexcept { return m_os == TargetOS::Windows; }
    [[nodiscard]] bool isLinux() const noexcept { return m_os == TargetOS::Linux; }
    [[nodiscard]] bool isMacOS() const noexcept { return m_os == TargetOS::MacOS; }
    [[nodiscard]] bool isUnix() const noexcept { return m_os == TargetOS::Linux || m_os == TargetOS::MacOS; }

    [[nodiscard]] bool isX86_64() const noexcept { return m_arch == TargetArch::X86_64; }
    [[nodiscard]] bool isAArch64() const noexcept { return m_arch == TargetArch::AArch64; }

    [[nodiscard]] bool isHost() const noexcept;
    [[nodiscard]] bool isCrossCompileFromHost() const noexcept;

    // Platform ABI & LLVM specifics
    [[nodiscard]] std::string getLLVMTriple() const;
    [[nodiscard]] std::string getDataLayout() const;
    [[nodiscard]] std::string getExecutableExtension() const;
    [[nodiscard]] std::string getObjectExtension() const;
    [[nodiscard]] std::string getSharedLibraryExtension() const;
    [[nodiscard]] std::string getRoDataSectionDirective() const;
    [[nodiscard]] std::string getFunctionTypeDirective(const std::string& fnName) const;
    [[nodiscard]] std::string getDefaultLinker() const;
    [[nodiscard]] std::string getCrossCompilerPrefix() const;

    // Equality
    bool operator==(const Target& other) const noexcept;
    bool operator!=(const Target& other) const noexcept;

private:
    TargetArch m_arch = TargetArch::X86_64;
    TargetOS m_os = TargetOS::Windows;
    TargetVendor m_vendor = TargetVendor::W64;
    TargetEnvironment m_env = TargetEnvironment::GNU;
    std::string m_rawTriple = "x86_64-windows";
};

} // namespace sph

