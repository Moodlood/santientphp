#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace sph {

struct FormatOptions {
    int indentSpaces = 4;
    bool useTabs = false;
    bool insertFinalNewline = true;
    bool trimTrailingWhitespace = true;
    int maxEmptyLines = 1;
};

struct FormatResult {
    bool success = true;
    bool changed = false;
    std::string formattedCode;
    std::string errorMessage;
    int linesFormatted = 0;
};

class Formatter {
public:
    explicit Formatter(FormatOptions options = {}) : m_options(options) {}

    [[nodiscard]] const FormatOptions& getOptions() const noexcept { return m_options; }
    void setOptions(FormatOptions options) noexcept { m_options = options; }

    // Format a source string
    [[nodiscard]] FormatResult format(std::string_view source) const;

    // Check if source string is already formatted
    [[nodiscard]] bool isFormatted(std::string_view source) const;

    // Format a file on disk (in-place if inPlace is true)
    [[nodiscard]] FormatResult formatFile(const std::filesystem::path& path, bool inPlace = true) const;

    // Check if a file on disk is properly formatted
    [[nodiscard]] bool isFileFormatted(const std::filesystem::path& path) const;

private:
    FormatOptions m_options;

    [[nodiscard]] std::string formatTokensAndWhitespace(std::string_view source) const;
    [[nodiscard]] std::string makeIndent(int level) const;
};

} // namespace sph

