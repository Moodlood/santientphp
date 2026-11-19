#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace sph {

struct LockedPackage {
    std::string name;
    std::string version;
    std::string source;     // e.g. "path:../lib", "git:https://...", "registry:..."
    std::string checksum;   // SHA-256 hex digest
    std::vector<std::string> dependencies;
};

class Lockfile {
public:
    uint32_t version = 1;
    std::vector<LockedPackage> packages;

    Lockfile() = default;

    static std::optional<Lockfile> fromString(const std::string& content);
    static std::optional<Lockfile> fromFile(const std::filesystem::path& path);

    bool toFile(const std::filesystem::path& path) const;
    std::string toString() const;

    const LockedPackage* findPackage(const std::string& name) const;
    void addOrUpdatePackage(LockedPackage pkg);

    // Compute standard SHA-256 hex digest of a byte sequence
    static std::string computeSha256(const std::string& data);
    // Compute SHA-256 hex digest of a file on disk
    static std::string computeFileSha256(const std::filesystem::path& filePath);
    // Compute SHA-256 checksum representing a directory's contents
    static std::string computeDirSha256(const std::filesystem::path& dirPath);
};

} // namespace sph

