#pragma once

#include "compiler/manifest/Manifest.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace sph {

class PackageManager {
public:
    PackageManager() = default;

    // Create a new project directory with standard layout
    static bool createProject(const std::string& projectName,
                             const std::filesystem::path& parentDir,
                             std::string& errorMsg);

    // Initialize an existing directory with sph.toml
    static bool initProject(const std::filesystem::path& targetDir,
                            std::string& errorMsg);

    // Add a dependency to sph.toml
    static bool addDependency(const std::filesystem::path& manifestPath,
                             const std::string& depName,
                             const std::string& location,
                             bool isGit,
                             std::string& errorMsg);

    // List dependencies from a manifest file or directory
    static std::vector<Dependency> listDependencies(const std::filesystem::path& path,
                                                    std::string& errorMsg);
};

} // namespace sph

