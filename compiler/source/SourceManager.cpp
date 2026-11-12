#include "SourceManager.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace sph {

SourceFile::SourceFile(uint32_t id, std::string filename, std::string content)
    : m_id(id), m_filename(std::move(filename)), m_content(std::move(content)) {
    computeLineOffsets();
}

void SourceFile::computeLineOffsets() {
    m_lineOffsets.clear();
    m_lineOffsets.push_back(0); // Line 1 starts at byte 0

    for (size_t i = 0; i < m_content.size(); ++i) {
        if (m_content[i] == '\n') {
            m_lineOffsets.push_back(static_cast<uint32_t>(i + 1));
        }
    }
}

SourceLocation SourceFile::getLocation(uint32_t byteOffset) const noexcept {
    if (m_content.empty() || m_lineOffsets.empty()) {
        return SourceLocation{m_id, byteOffset, 1, 1};
    }

    if (byteOffset > m_content.size()) {
        byteOffset = static_cast<uint32_t>(m_content.size());
    }

    auto it = std::upper_bound(m_lineOffsets.begin(), m_lineOffsets.end(), byteOffset);
    uint32_t line = static_cast<uint32_t>(std::distance(m_lineOffsets.begin(), it));
    if (line == 0) {
        line = 1;
    }

    uint32_t lineStart = m_lineOffsets[line - 1];
    uint32_t column = (byteOffset - lineStart) + 1;

    return SourceLocation{m_id, byteOffset, line, column};
}

std::string_view SourceFile::getLineContent(uint32_t line) const noexcept {
    if (line == 0 || line > m_lineOffsets.size()) {
        return {};
    }

    uint32_t startOffset = m_lineOffsets[line - 1];
    uint32_t endOffset = (line < m_lineOffsets.size())
                             ? m_lineOffsets[line]
                             : static_cast<uint32_t>(m_content.size());

    if (startOffset > m_content.size()) {
        return {};
    }
    if (endOffset > m_content.size()) {
        endOffset = static_cast<uint32_t>(m_content.size());
    }

    std::string_view lineView(m_content.data() + startOffset, endOffset - startOffset);

    // Strip trailing \r and \n for clean output
    while (!lineView.empty() && (lineView.back() == '\n' || lineView.back() == '\r')) {
        lineView.remove_suffix(1);
    }

    return lineView;
}

std::optional<uint32_t> SourceManager::loadFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return std::nullopt;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    uint32_t fileId = static_cast<uint32_t>(m_files.size() + 1);
    m_files.push_back(std::make_unique<SourceFile>(fileId, path.string(), std::move(content)));
    return fileId;
}

uint32_t SourceManager::addSource(std::string name, std::string content) {
    uint32_t fileId = static_cast<uint32_t>(m_files.size() + 1);
    m_files.push_back(std::make_unique<SourceFile>(fileId, std::move(name), std::move(content)));
    return fileId;
}

std::optional<uint32_t> SourceManager::findFileIdByName(const std::string& name) const noexcept {
    for (size_t i = 0; i < m_files.size(); ++i) {
        if (m_files[i]->getFilename() == name) {
            return static_cast<uint32_t>(i + 1);
        }
    }
    return std::nullopt;
}

const SourceFile* SourceManager::getFile(uint32_t fileId) const noexcept {
    if (fileId == 0 || fileId > m_files.size()) {
        return nullptr;
    }
    return m_files[fileId - 1].get();
}

SourceLocation SourceManager::getLocation(uint32_t fileId, uint32_t byteOffset) const noexcept {
    const SourceFile* file = getFile(fileId);
    if (!file) {
        return SourceLocation{fileId, byteOffset, 0, 0};
    }
    return file->getLocation(byteOffset);
}

std::string_view SourceManager::getLineContent(uint32_t fileId, uint32_t line) const noexcept {
    const SourceFile* file = getFile(fileId);
    if (!file) {
        return {};
    }
    return file->getLineContent(line);
}

} // namespace sph

