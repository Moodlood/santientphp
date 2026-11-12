#pragma once

#include <iosfwd>
#include <string>
#include <vector>

namespace sph {

struct ToolchainCheck {
    std::string category;
    std::string name;
    std::string details;
    bool isHealthy = false;
    bool isOptional = false;
};

class Doctor {
public:
    Doctor() = default;

    // Runs all system and toolchain checks
    [[nodiscard]] std::vector<ToolchainCheck> runChecks() const;

    // Formats and prints doctor report to out stream
    // Returns 0 if all required toolchains are healthy, 1 if any required check failed
    int report(std::ostream& out) const;

private:
    static bool findTool(const std::string& tool, std::string& versionOut);
    static bool checkSystemLibrary(const std::string& dllName);
};

} // namespace sph

