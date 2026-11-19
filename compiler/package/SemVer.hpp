#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sph {

struct SemVer {
    uint32_t major = 0;
    uint32_t minor = 0;
    uint32_t patch = 0;
    std::string prerelease;
    std::string build;

    SemVer() = default;
    constexpr SemVer(uint32_t maj, uint32_t min, uint32_t pat)
        : major(maj), minor(min), patch(pat) {}
    SemVer(uint32_t maj, uint32_t min, uint32_t pat, std::string pre, std::string bld = "")
        : major(maj), minor(min), patch(pat), prerelease(std::move(pre)), build(std::move(bld)) {}

    static std::optional<SemVer> parse(std::string_view str);
    std::string toString() const;

    bool operator==(const SemVer& other) const;
    bool operator!=(const SemVer& other) const { return !(*this == other); }
    bool operator<(const SemVer& other) const;
    bool operator<=(const SemVer& other) const { return *this < other || *this == other; }
    bool operator>(const SemVer& other) const { return other < *this; }
    bool operator>=(const SemVer& other) const { return !(*this < other); }
};

enum class ReqOp {
    Exact,       // = or no prefix
    Caret,       // ^
    Tilde,       // ~
    Greater,     // >
    GreaterEq,   // >=
    Less,        // <
    LessEq,      // <=
    Wildcard     // *
};

struct ReqClause {
    ReqOp op = ReqOp::Exact;
    SemVer version;
};

class VersionReq {
public:
    VersionReq() = default;

    static std::optional<VersionReq> parse(std::string_view str);
    static VersionReq any();

    bool matches(const SemVer& ver) const;
    std::string toString() const;

private:
    std::vector<ReqClause> m_clauses;
    std::string m_raw;
    bool m_isAny = false;
};

} // namespace sph

