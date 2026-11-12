#include "Manifest.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace sph {

namespace {

std::string trim(std::string_view s) {
    size_t start = 0;
    while (start < s.size() && (std::isspace(static_cast<unsigned char>(s[start])) != 0)) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && (std::isspace(static_cast<unsigned char>(s[end - 1])) != 0)) {
        --end;
    }
    return std::string(s.substr(start, end - start));
}

std::string stripQuotes(const std::string& s) {
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') ||
                          (s.front() == '\'' && s.back() == '\''))) {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

bool parseBool(const std::string& s) {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return (lower == "true" || lower == "1" || lower == "yes");
}

} // namespace

std::optional<Manifest> Manifest::fromString(const std::string& tomlContent) {
    Manifest manifest;
    std::istringstream stream(tomlContent);
    std::string line;
    std::string currentSection;

    while (std::getline(stream, line)) {
        // Strip comments starting with '#'
        auto commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }

        std::string trimmed = trim(line);
        if (trimmed.empty()) {
            continue;
        }

        // Section header: [section]
        if (trimmed.front() == '[' && trimmed.back() == ']') {
            currentSection = trim(trimmed.substr(1, trimmed.size() - 2));
            continue;
        }

        // Key-value pair: key = value
        auto eqPos = trimmed.find('=');
        if (eqPos == std::string::npos) {
            continue;
        }

        std::string key = trim(trimmed.substr(0, eqPos));
        std::string rawVal = trim(trimmed.substr(eqPos + 1));
        std::string val = stripQuotes(rawVal);

        if (currentSection == "project") {
            if (key == "name") manifest.projectName = val;
            else if (key == "version") manifest.version = val;
            else if (key == "entry") manifest.entry = val;
        } else if (currentSection == "compiler") {
            if (key == "language_version") manifest.languageVersion = val;
        } else if (currentSection == "target") {
            if (key == "triple") manifest.targetTriple = val;
        } else if (currentSection == "build") {
            if (key == "mode") manifest.buildMode = val;
            else if (key == "optimization") manifest.optimization = val;
        } else if (currentSection == "memory") {
            if (key == "default") manifest.memory = val;
        } else if (currentSection == "compatibility") {
            if (key == "php") {
                manifest.compatPhp = parseBool(val);
                if (manifest.compatPhp && manifest.compatMode == CompatibilityMode::Strict) {
                    manifest.compatMode = CompatibilityMode::Dynamic;
                }
            } else if (key == "mode") {
                std::string modeLower = val;
                std::transform(modeLower.begin(), modeLower.end(), modeLower.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (modeLower == "dynamic") {
                    manifest.compatMode = CompatibilityMode::Dynamic;
                    manifest.compatPhp = true;
                } else if (modeLower == "transitional") {
                    manifest.compatMode = CompatibilityMode::Transitional;
                    manifest.compatPhp = true;
                } else {
                    manifest.compatMode = CompatibilityMode::Strict;
                }
            }
        } else if (currentSection == "dependencies") {
            Dependency dep;
            dep.name = key;
            if (rawVal.front() == '{' && rawVal.back() == '}') {
                std::string inner = trim(rawVal.substr(1, rawVal.size() - 2));
                std::istringstream innerStream(inner);
                std::string pair;
                while (std::getline(innerStream, pair, ',')) {
                    auto pEq = pair.find('=');
                    if (pEq != std::string::npos) {
                        std::string pKey = trim(pair.substr(0, pEq));
                        std::string pVal = stripQuotes(trim(pair.substr(pEq + 1)));
                        if (pKey == "path") dep.path = pVal;
                        else if (pKey == "git") dep.gitUrl = pVal;
                        else if (pKey == "version") dep.version = pVal;
                    }
                }
            } else {
                dep.version = val;
            }
            manifest.dependencies.push_back(std::move(dep));
        }
    }

    return manifest;
}

std::optional<Manifest> Manifest::fromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    return fromString(content);
}

std::optional<Manifest> Manifest::findInPath(const std::string& startDir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path current = fs::absolute(startDir, ec);
    if (ec) {
        current = fs::current_path();
    }

    for (int i = 0; i < 10; ++i) {
        fs::path candidate = current / "sph.toml";
        if (fs::exists(candidate, ec) && fs::is_regular_file(candidate, ec)) {
            return fromFile(candidate.string());
        }
        if (!current.has_parent_path() || current == current.parent_path()) {
            break;
        }
        current = current.parent_path();
    }

    return std::nullopt;
}

} // namespace sph

