#pragma once

#include <cstdint>
#include <ostream>
#include <string>

namespace sph {

struct SourceLocation {
    uint32_t fileId = 0;
    uint32_t byteOffset = 0;
    uint32_t line = 0;      // 1-based line number (0 = invalid)
    uint32_t column = 0;    // 1-based column number (0 = invalid)

    [[nodiscard]] constexpr bool isValid() const noexcept {
        return line > 0 && column > 0;
    }

    constexpr bool operator==(const SourceLocation& other) const noexcept {
        return fileId == other.fileId && byteOffset == other.byteOffset &&
               line == other.line && column == other.column;
    }

    constexpr bool operator!=(const SourceLocation& other) const noexcept {
        return !(*this == other);
    }

    constexpr bool operator<(const SourceLocation& other) const noexcept {
        if (fileId != other.fileId) return fileId < other.fileId;
        return byteOffset < other.byteOffset;
    }
};

struct SourceSpan {
    SourceLocation start;
    SourceLocation end;

    SourceSpan() = default;
    constexpr SourceSpan(SourceLocation s, SourceLocation e) : start(s), end(e) {}
    constexpr explicit SourceSpan(SourceLocation loc) : start(loc), end(loc) {}

    [[nodiscard]] constexpr bool isValid() const noexcept {
        return start.isValid() && end.isValid();
    }

    [[nodiscard]] constexpr SourceSpan merge(const SourceSpan& other) const noexcept {
        if (!isValid()) return other;
        if (!other.isValid()) return *this;
        SourceLocation newStart = (start < other.start) ? start : other.start;
        SourceLocation newEnd = (other.end < end) ? end : other.end;
        return SourceSpan(newStart, newEnd);
    }

    constexpr bool operator==(const SourceSpan& other) const noexcept {
        return start == other.start && end == other.end;
    }

    constexpr bool operator!=(const SourceSpan& other) const noexcept {
        return !(*this == other);
    }
};

inline std::ostream& operator<<(std::ostream& os, const SourceLocation& loc) {
    return os << loc.line << ":" << loc.column;
}

inline std::ostream& operator<<(std::ostream& os, const SourceSpan& span) {
    return os << "[" << span.start << " - " << span.end << "]";
}

} // namespace sph
