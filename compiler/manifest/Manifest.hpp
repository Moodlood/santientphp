#pragma once

#include <optional>
#include <string>
#include <vector>

namespace sph {

enum class CompatibilityMode {
    Strict,
    Transitional,
    Dynamic
};

struct Dependency {
    std::string name;
    std::string path;
    std::string gitUrl;
    std::string version;
};

struct Manifest {
    // [project]
    std::string projectName = "unnamed";
    std::string version = "0.1.0";
    std::string entry = "main.sph";

    // [dependencies]
    std::vector<Dependency> dependencies;

    // [compiler]
    std::string languageVersion = "0.1";

    // [target]
    std::string targetTriple = "x86_64-windows";

    // [build]
    std::string buildMode = "debug";
    std::string optimization = "0";

    // [memory]
    std::string memory = "automatic";

    // [compatibility]
    bool compatPhp = false;
    CompatibilityMode compatMode = CompatibilityMode::Strict;

    [[nodiscard]] bool isDynamicMode() const noexcept {
        return compatPhp || compatMode == CompatibilityMode::Dynamic || compatMode == CompatibilityMode::Transitional;
    }

    static std::optional<Manifest> fromString(const std::string& tomlContent);
    static std::optional<Manifest> fromFile(const std::string& path);
    static std::optional<Manifest> findInPath(const std::string& startDir = ".");
};

} // namespace sph

