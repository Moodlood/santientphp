#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace sph {

struct DocTag {
    std::string tag;         // e.g. "param", "return", "throws", "author", "since"
    std::string target;      // e.g. parameter name for @param, or exception type for @throws
    std::string description; // text description
};

struct DocComment {
    std::string raw;
    std::string brief;
    std::string detailed;
    std::vector<DocTag> tags;

    [[nodiscard]] bool empty() const noexcept {
        return brief.empty() && detailed.empty() && tags.empty();
    }

    [[nodiscard]] const DocTag* findTag(std::string_view tagName) const noexcept {
        for (const auto& t : tags) {
            if (t.tag == tagName) return &t;
        }
        return nullptr;
    }

    [[nodiscard]] std::vector<DocTag> getTags(std::string_view tagName) const {
        std::vector<DocTag> matches;
        for (const auto& t : tags) {
            if (t.tag == tagName) matches.push_back(t);
        }
        return matches;
    }
};

class DocCommentParser {
public:
    static DocComment parse(std::string_view rawText);

private:
    static std::string trim(std::string_view s);
    static std::string cleanDocLine(std::string_view line);
};

} // namespace sph

