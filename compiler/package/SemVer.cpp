#include "compiler/package/SemVer.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace sph {

namespace {

bool isDigits(std::string_view s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

std::vector<std::string> split(std::string_view s, char delim) {
    std::vector<std::string> result;
    size_t start = 0;
    while (start < s.size()) {
        size_t pos = s.find(delim, start);
        if (pos == std::string_view::npos) {
            result.emplace_back(s.substr(start));
            break;
        }
        result.emplace_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return result;
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return s;
}

} // namespace

std::optional<SemVer> SemVer::parse(std::string_view str) {
    str = trim(str);
    if (str.empty()) return std::nullopt;

    // Optional leading 'v' or 'V'
    if (str.front() == 'v' || str.front() == 'V') {
        str.remove_prefix(1);
    }

    std::string build;
    size_t plusPos = str.find('+');
    if (plusPos != std::string_view::npos) {
        build = std::string(str.substr(plusPos + 1));
        str = str.substr(0, plusPos);
    }

    std::string prerelease;
    size_t dashPos = str.find('-');
    if (dashPos != std::string_view::npos) {
        prerelease = std::string(str.substr(dashPos + 1));
        str = str.substr(0, dashPos);
    }

    auto parts = split(str, '.');
    if (parts.size() < 1 || parts.size() > 3) {
        return std::nullopt;
    }

    for (const auto& p : parts) {
        if (!isDigits(p)) return std::nullopt;
    }

    SemVer v;
    try {
        v.major = static_cast<uint32_t>(std::stoul(parts[0]));
        v.minor = parts.size() > 1 ? static_cast<uint32_t>(std::stoul(parts[1])) : 0;
        v.patch = parts.size() > 2 ? static_cast<uint32_t>(std::stoul(parts[2])) : 0;
    } catch (...) {
        return std::nullopt;
    }

    v.prerelease = std::move(prerelease);
    v.build = std::move(build);
    return v;
}

std::string SemVer::toString() const {
    std::string s = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    if (!prerelease.empty()) {
        s += "-" + prerelease;
    }
    if (!build.empty()) {
        s += "+" + build;
    }
    return s;
}

bool SemVer::operator==(const SemVer& other) const {
    return major == other.major && minor == other.minor && patch == other.patch &&
           prerelease == other.prerelease;
}

bool SemVer::operator<(const SemVer& other) const {
    if (major != other.major) return major < other.major;
    if (minor != other.minor) return minor < other.minor;
    if (patch != other.patch) return patch < other.patch;

    // A version with a pre-release has lower precedence than one without.
    if (!prerelease.empty() && other.prerelease.empty()) return true;
    if (prerelease.empty() && !other.prerelease.empty()) return false;
    if (prerelease.empty() && other.prerelease.empty()) return false;

    // Both have pre-release: compare dot-separated identifiers.
    auto aParts = split(prerelease, '.');
    auto bParts = split(other.prerelease, '.');
    size_t count = std::min(aParts.size(), bParts.size());
    for (size_t i = 0; i < count; ++i) {
        bool aNum = isDigits(aParts[i]);
        bool bNum = isDigits(bParts[i]);
        if (aNum && bNum) {
            uint64_t an = std::stoull(aParts[i]);
            uint64_t bn = std::stoull(bParts[i]);
            if (an != bn) return an < bn;
        } else if (aNum && !bNum) {
            return true; // numeric has lower precedence than alphanumeric
        } else if (!aNum && bNum) {
            return false;
        } else {
            if (aParts[i] != bParts[i]) return aParts[i] < bParts[i];
        }
    }
    return aParts.size() < bParts.size();
}

VersionReq VersionReq::any() {
    VersionReq r;
    r.m_isAny = true;
    r.m_raw = "*";
    return r;
}

std::optional<VersionReq> VersionReq::parse(std::string_view str) {
    str = trim(str);
    if (str.empty() || str == "*" || str == "any" || str == "latest") {
        return any();
    }

    VersionReq req;
    req.m_raw = std::string(str);

    // Split by comma
    auto parts = split(str, ',');
    for (auto& clauseStr : parts) {
        std::string_view c = trim(clauseStr);
        if (c.empty()) continue;

        ReqClause clause;
        if (c.rfind(">=", 0) == 0) {
            clause.op = ReqOp::GreaterEq;
            c.remove_prefix(2);
        } else if (c.rfind("<=", 0) == 0) {
            clause.op = ReqOp::LessEq;
            c.remove_prefix(2);
        } else if (c.rfind(">", 0) == 0) {
            clause.op = ReqOp::Greater;
            c.remove_prefix(1);
        } else if (c.rfind("<", 0) == 0) {
            clause.op = ReqOp::Less;
            c.remove_prefix(1);
        } else if (c.rfind("^", 0) == 0) {
            clause.op = ReqOp::Caret;
            c.remove_prefix(1);
        } else if (c.rfind("~", 0) == 0) {
            clause.op = ReqOp::Tilde;
            c.remove_prefix(1);
        } else if (c.rfind("=", 0) == 0) {
            clause.op = ReqOp::Exact;
            c.remove_prefix(1);
        } else {
            clause.op = ReqOp::Exact;
        }

        c = trim(c);
        if (c == "*") {
            continue; // matches everything
        }

        auto ver = SemVer::parse(c);
        if (!ver) return std::nullopt;
        clause.version = *ver;
        req.m_clauses.push_back(clause);
    }

    return req;
}

bool VersionReq::matches(const SemVer& ver) const {
    if (m_isAny) return true;

    for (const auto& clause : m_clauses) {
        switch (clause.op) {
            case ReqOp::Exact:
                if (!(ver == clause.version)) return false;
                break;
            case ReqOp::Greater:
                if (!(ver > clause.version)) return false;
                break;
            case ReqOp::GreaterEq:
                if (!(ver >= clause.version)) return false;
                break;
            case ReqOp::Less:
                if (!(ver < clause.version)) return false;
                break;
            case ReqOp::LessEq:
                if (!(ver <= clause.version)) return false;
                break;
            case ReqOp::Caret: {
                // ^1.2.3 := >=1.2.3, <2.0.0
                // ^0.2.3 := >=0.2.3, <0.3.0
                // ^0.0.3 := ==0.0.3
                if (ver < clause.version) return false;
                if (clause.version.major > 0) {
                    SemVer upper(clause.version.major + 1, 0, 0);
                    if (ver >= upper) return false;
                } else if (clause.version.minor > 0) {
                    SemVer upper(0, clause.version.minor + 1, 0);
                    if (ver >= upper) return false;
                } else {
                    if (ver != clause.version) return false;
                }
                break;
            }
            case ReqOp::Tilde: {
                // ~1.2.3 := >=1.2.3, <1.3.0
                if (ver < clause.version) return false;
                SemVer upper(clause.version.major, clause.version.minor + 1, 0);
                if (ver >= upper) return false;
                break;
            }
            case ReqOp::Wildcard:
                break;
        }
    }

    return true;
}

std::string VersionReq::toString() const {
    return m_raw.empty() ? "*" : m_raw;
}

} // namespace sph

