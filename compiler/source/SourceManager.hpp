#pragma once

#include "SourceLocation.hpp"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sph {

class SourceFile {
public:
    SourceFile(uint32_t id, std::string filename, std::string content);

    [[nodiscard]] uint32_t getId() const noexcept { return m_id; }
    [[nodiscard]] const std::string& getFilename() const noexcept { return m_filename; }
    [[nodiscard]] std::string_view getContent() const noexcept { return m_content; }

    [[nodiscard]] SourceLocation getLocation(uint32_t byteOffset) const noexcept;
    [[nodiscard]] std::string_view getLineContent(uint32_t line) const noexcept;
    [[nodiscard]] uint32_t getLineCount() const noexcept { return static_cast<uint32_t>(m_lineOffsets.size()); }

private:
    void computeLineOffsets();

    uint32_t m_id;
    std::string m_filename;
    std::string m_content;
    std::vector<uint32_t> m_lineOffsets; // 0-based offset where each line starts (index 0 is line 1)
};

class SourceManager {
public:
    SourceManager() = default;

    // Load file from filesystem
    [[nodiscard]] std::optional<uint32_t> loadFile(const std::filesystem::path& path);

    // Register in-memory source buffer
    [[nodiscard]] uint32_t addSource(std::string name, std::string content);

    [[nodiscard]] std::optional<uint32_t> findFileIdByName(const std::string& name) const noexcept;
    [[nodiscard]] const SourceFile* getFile(uint32_t fileId) const noexcept;
    [[nodiscard]] uint32_t getFileCount() const noexcept { return static_cast<uint32_t>(m_files.size()); }
    [[nodiscard]] SourceLocation getLocation(uint32_t fileId, uint32_t byteOffset) const noexcept;
    [[nodiscard]] std::string_view getLineContent(uint32_t fileId, uint32_t line) const noexcept;

private:
    std::vector<std::unique_ptr<SourceFile>> m_files;
};

} // namespace sph

