#pragma once

#include "compiler/target/Target.hpp"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace sph {

class Linker {
public:
    Linker() = default;

    [[nodiscard]] bool linkExecutable(const std::filesystem::path& asmPath,
                                     const std::filesystem::path& outputPath,
                                     const std::filesystem::path& runtimeLibPath,
                                     std::string& errorMsg,
                                     const std::string& optLevel = "",
                                     bool debugInfo = false,
                                     const Target& target = Target::getHostTarget(),
                                     bool isGui = false,
                                     const std::filesystem::path& guiLibPath = "",
                                     const std::filesystem::path& resourceObjPath = "",
                                     const std::vector<std::string>& extraLibs = {},
                                     const std::vector<std::filesystem::path>& libSearchPaths = {}) const;

    [[nodiscard]] bool linkExecutable(const std::filesystem::path& asmPath,
                                     const std::filesystem::path& outputPath,
                                     const std::vector<std::filesystem::path>& runtimeLibPaths,
                                     std::string& errorMsg,
                                     const std::string& optLevel = "",
                                     bool debugInfo = false,
                                     const Target& target = Target::getHostTarget(),
                                     bool isGui = false,
                                     const std::filesystem::path& guiLibPath = "",
                                     const std::filesystem::path& resourceObjPath = "",
                                     const std::vector<std::string>& extraLibs = {},
                                     const std::vector<std::filesystem::path>& libSearchPaths = {},
                                     bool linkNetwork = true) const;

    [[nodiscard]] std::optional<std::string> findCrossToolchain(const Target& target) const;
};

} // namespace sph

