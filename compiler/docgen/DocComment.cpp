#include "compiler/docgen/DocComment.hpp"
#include <cctype>
#include <sstream>

namespace sph {

std::string DocCommentParser::trim(std::string_view s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        start++;
    }
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        end--;
    }
    return std::string(s.substr(start, end - start));
}

std::string DocCommentParser::cleanDocLine(std::string_view line) {
    std::string trimmed = trim(line);
    if (trimmed.empty()) return "";

    // Strip leading * from block comments (e.g. " * @param a ...")
    if (trimmed[0] == '*') {
        size_t start = 1;
        while (start < trimmed.size() && (trimmed[start] == ' ' || trimmed[start] == '\t')) {
            start++;
        }
        return trim(trimmed.substr(start));
    }

    // Strip leading /// from line comments
    if (trimmed.rfind("///", 0) == 0) {
        size_t start = 3;
        while (start < trimmed.size() && (trimmed[start] == ' ' || trimmed[start] == '\t')) {
            start++;
        }
        return trim(trimmed.substr(start));
    }

    return trimmed;
}

DocComment DocCommentParser::parse(std::string_view rawText) {
    DocComment comment;
    comment.raw = std::string(rawText);

    std::string textToParse = std::string(rawText);
    // Strip opening /** or /* and closing */
    if (textToParse.rfind("/**", 0) == 0) {
        textToParse = textToParse.substr(3);
    } else if (textToParse.rfind("/*", 0) == 0) {
        textToParse = textToParse.substr(2);
    }

    if (textToParse.size() >= 2 && textToParse.substr(textToParse.size() - 2) == "*/") {
        textToParse = textToParse.substr(0, textToParse.size() - 2);
    }

    std::istringstream stream(textToParse);
    std::string line;
    std::vector<std::string> descLines;
    bool inTags = false;

    while (std::getline(stream, line)) {
        std::string cleaned = cleanDocLine(line);
        if (cleaned.empty()) {
            if (!descLines.empty() && !inTags) {
                descLines.push_back(""); // preserve paragraph break
            }
            continue;
        }

        if (cleaned[0] == '@') {
            inTags = true;
            // Parse tag
            size_t spacePos = cleaned.find_first_of(" \t");
            std::string tagName = cleaned.substr(1, spacePos == std::string::npos ? std::string::npos : spacePos - 1);
            std::string remainder = (spacePos == std::string::npos) ? "" : trim(cleaned.substr(spacePos + 1));

            DocTag tag;
            tag.tag = tagName;

            if (tagName == "param") {
                // @param <name> <description...>
                size_t paramSpace = remainder.find_first_of(" \t");
                if (paramSpace != std::string::npos) {
                    tag.target = trim(remainder.substr(0, paramSpace));
                    tag.description = trim(remainder.substr(paramSpace + 1));
                } else {
                    tag.target = remainder;
                }
            } else if (tagName == "throws") {
                // @throws <type> <description...>
                size_t throwsSpace = remainder.find_first_of(" \t");
                if (throwsSpace != std::string::npos) {
                    tag.target = trim(remainder.substr(0, throwsSpace));
                    tag.description = trim(remainder.substr(throwsSpace + 1));
                } else {
                    tag.target = remainder;
                }
            } else if (tagName == "returns") {
                tag.tag = "return";
                tag.description = remainder;
            } else if (tagName == "brief") {
                comment.brief = remainder;
                continue;
            } else {
                tag.description = remainder;
            }

            comment.tags.push_back(std::move(tag));
        } else {
            if (inTags && !comment.tags.empty()) {
                // Continuation of previous tag's description
                if (!comment.tags.back().description.empty()) {
                    comment.tags.back().description += " ";
                }
                comment.tags.back().description += cleaned;
            } else {
                descLines.push_back(cleaned);
            }
        }
    }

    // Process descLines into brief and detailed
    if (!descLines.empty()) {
        if (comment.brief.empty()) {
            comment.brief = descLines[0];
            // Remaining lines become detailed
            std::string details;
            for (size_t i = 1; i < descLines.size(); ++i) {
                if (descLines[i].empty()) {
                    if (!details.empty()) details += "\n\n";
                } else {
                    if (!details.empty() && details.back() != '\n') details += " ";
                    details += descLines[i];
                }
            }
            comment.detailed = trim(details);
        } else {
            std::string details;
            for (const auto& l : descLines) {
                if (l.empty()) {
                    if (!details.empty()) details += "\n\n";
                } else {
                    if (!details.empty() && details.back() != '\n') details += " ";
                    details += l;
                }
            }
            comment.detailed = trim(details);
        }
    }

    return comment;
}

} // namespace sph

