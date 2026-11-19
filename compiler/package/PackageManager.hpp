#pragma once

#include "compiler/manifest/Manifest.hpp"
#include "compiler/package/Lockfile.hpp"
#include "compiler/package/SemVer.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace sph {

enum class ProjectTemplate {
    CLI,
    GUI,
    Web,
    Lib
};

class PackageManager {
public:
    PackageManager() = default;

    // Create a new project directory with specified template layout
    static bool createProject(const std::string& projectName,
                             const std::filesystem::path& parentDir,
                             ProjectTemplate tmpl,
                             std::string& errorMsg);

    // Create a new project directory with standard layout (default CLI)
    static bool createProject(const std::string& projectName,
                             const std::filesystem::path& parentDir,
                             std::string& errorMsg) {
        return createProject(projectName, parentDir, ProjectTemplate::CLI, errorMsg);
    }

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

    // Generate or update sph.lock for a project
    static bool generateLockfile(const std::filesystem::path& projectDir,
                                 std::string& errorMsg);

    // Verify existing sph.lock against disk files
    static bool verifyLockfile(const std::filesystem::path& projectDir,
                               std::string& errorMsg);
};

} // namespace sph
