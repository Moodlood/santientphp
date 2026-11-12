#pragma once

#include "compiler/docgen/DocModel.hpp"
#include <filesystem>
#include <string>
#include <string_view>

namespace sph {

class DocGenerator {
public:
    DocGenerator() = default;

    // Generate Markdown representation
    [[nodiscard]] std::string generateMarkdown(const DocProject& project) const;
    [[nodiscard]] std::string generateMarkdown(const DocModule& module) const;

    // Generate standalone responsive HTML representation
    [[nodiscard]] std::string generateHtml(const DocProject& project) const;
    [[nodiscard]] std::string generateHtml(const DocModule& module) const;

    // Write to file (format: "html" or "md")
    bool generateToFile(const DocProject& project, const std::filesystem::path& outPath,
                        std::string_view format = "html") const;

private:
    void renderFunctionMd(std::string& out, const DocFunction& fn, int headerLevel = 3) const;
    void renderStructMd(std::string& out, const DocStruct& st) const;
    void renderClassMd(std::string& out, const DocClass& cl) const;
    void renderInterfaceMd(std::string& out, const DocInterface& in) const;
    void renderEnumMd(std::string& out, const DocEnum& en) const;

    [[nodiscard]] std::string escapeHtml(std::string_view text) const;
    void renderFunctionHtml(std::string& out, const DocFunction& fn, const std::string& parentScope = "") const;
};

} // namespace sph

