#pragma once

#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sph::lsp {

enum class JsonType {
    Null,
    Boolean,
    Number,
    String,
    Array,
    Object
};

class JsonValue {
public:
    using Array = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue>;

    JsonValue() : m_type(JsonType::Null) {}
    JsonValue(std::nullptr_t) : m_type(JsonType::Null) {}
    JsonValue(bool val) : m_type(JsonType::Boolean), m_bool(val) {}
    JsonValue(int val) : m_type(JsonType::Number), m_num(static_cast<double>(val)), m_int(val), m_isInteger(true) {}
    JsonValue(uint32_t val) : m_type(JsonType::Number), m_num(static_cast<double>(val)), m_int(val), m_isInteger(true) {}
    JsonValue(int64_t val) : m_type(JsonType::Number), m_num(static_cast<double>(val)), m_int(val), m_isInteger(true) {}
    JsonValue(uint64_t val) : m_type(JsonType::Number), m_num(static_cast<double>(val)), m_int(static_cast<int64_t>(val)), m_isInteger(true) {}
    JsonValue(double val) : m_type(JsonType::Number), m_num(val), m_int(static_cast<int64_t>(val)), m_isInteger(false) {}
    JsonValue(const char* val) : m_type(JsonType::String), m_str(val ? val : "") {}
    JsonValue(std::string val) : m_type(JsonType::String), m_str(std::move(val)) {}
    JsonValue(std::string_view val) : m_type(JsonType::String), m_str(val) {}
    JsonValue(Array val) : m_type(JsonType::Array), m_arr(std::move(val)) {}
    JsonValue(Object val) : m_type(JsonType::Object), m_obj(std::move(val)) {}

    JsonValue(std::initializer_list<std::pair<const std::string, JsonValue>> list)
        : m_type(JsonType::Object) {
        for (const auto& kv : list) {
            m_obj.emplace(kv.first, kv.second);
        }
    }

    [[nodiscard]] JsonType getType() const noexcept { return m_type; }
    [[nodiscard]] bool isNull() const noexcept { return m_type == JsonType::Null; }
    [[nodiscard]] bool isBool() const noexcept { return m_type == JsonType::Boolean; }
    [[nodiscard]] bool isNumber() const noexcept { return m_type == JsonType::Number; }
    [[nodiscard]] bool isString() const noexcept { return m_type == JsonType::String; }
    [[nodiscard]] bool isArray() const noexcept { return m_type == JsonType::Array; }
    [[nodiscard]] bool isObject() const noexcept { return m_type == JsonType::Object; }

    [[nodiscard]] bool asBool(bool def = false) const noexcept {
        return m_type == JsonType::Boolean ? m_bool : def;
    }

    [[nodiscard]] int64_t asInt(int64_t def = 0) const noexcept {
        return m_type == JsonType::Number ? m_int : def;
    }

    [[nodiscard]] double asDouble(double def = 0.0) const noexcept {
        return m_type == JsonType::Number ? m_num : def;
    }

    [[nodiscard]] const std::string& asString(const std::string& def = s_emptyString) const noexcept {
        return m_type == JsonType::String ? m_str : def;
    }

    [[nodiscard]] const Array& asArray() const noexcept {
        return m_type == JsonType::Array ? m_arr : s_emptyArray;
    }

    [[nodiscard]] Array& asArray() {
        if (m_type != JsonType::Array) {
            m_type = JsonType::Array;
            m_arr.clear();
        }
        return m_arr;
    }

    [[nodiscard]] const Object& asObject() const noexcept {
        return m_type == JsonType::Object ? m_obj : s_emptyObject;
    }

    [[nodiscard]] Object& asObject() {
        if (m_type != JsonType::Object) {
            m_type = JsonType::Object;
            m_obj.clear();
        }
        return m_obj;
    }

    [[nodiscard]] bool has(const std::string& key) const noexcept {
        return m_type == JsonType::Object && m_obj.find(key) != m_obj.end();
    }

    static const JsonValue& nullRef() {
        static const JsonValue s_null;
        return s_null;
    }

    const JsonValue& operator[](const std::string& key) const {
        if (m_type == JsonType::Object) {
            auto it = m_obj.find(key);
            if (it != m_obj.end()) {
                return it->second;
            }
        }
        return nullRef();
    }

    JsonValue& operator[](const std::string& key) {
        if (m_type != JsonType::Object) {
            m_type = JsonType::Object;
            m_obj.clear();
        }
        return m_obj[key];
    }

    const JsonValue& operator[](size_t index) const {
        if (m_type == JsonType::Array && index < m_arr.size()) {
            return m_arr[index];
        }
        return nullRef();
    }

    JsonValue& operator[](size_t index) {
        if (m_type != JsonType::Array) {
            m_type = JsonType::Array;
            m_arr.clear();
        }
        if (index >= m_arr.size()) {
            m_arr.resize(index + 1);
        }
        return m_arr[index];
    }

    [[nodiscard]] std::string serialize() const {
        std::ostringstream ss;
        serializeInternal(ss);
        return ss.str();
    }

    static JsonValue parse(std::string_view json) {
        size_t idx = 0;
        skipWhitespace(json, idx);
        return parseValue(json, idx);
    }

private:
    JsonType m_type = JsonType::Null;
    bool m_bool = false;
    double m_num = 0.0;
    int64_t m_int = 0;
    bool m_isInteger = false;
    std::string m_str;
    Array m_arr;
    Object m_obj;

    inline static const std::string s_emptyString = "";
    inline static const Array s_emptyArray = {};
    inline static const Object s_emptyObject = {};

    static void skipWhitespace(std::string_view s, size_t& idx) {
        while (idx < s.size() && (s[idx] == ' ' || s[idx] == '\t' || s[idx] == '\r' || s[idx] == '\n')) {
            ++idx;
        }
    }

    static std::string escapeString(const std::string& in) {
        std::string out;
        out.reserve(in.size() + 16);
        for (char c : in) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                        out += buf;
                    } else {
                        out += c;
                    }
                    break;
            }
        }
        return out;
    }

    void serializeInternal(std::ostringstream& ss) const {
        switch (m_type) {
            case JsonType::Null:
                ss << "null";
                break;
            case JsonType::Boolean:
                ss << (m_bool ? "true" : "false");
                break;
            case JsonType::Number:
                if (m_isInteger) {
                    ss << m_int;
                } else {
                    ss << m_num;
                }
                break;
            case JsonType::String:
                ss << '"' << escapeString(m_str) << '"';
                break;
            case JsonType::Array: {
                ss << '[';
                for (size_t i = 0; i < m_arr.size(); ++i) {
                    if (i > 0) ss << ',';
                    m_arr[i].serializeInternal(ss);
                }
                ss << ']';
                break;
            }
            case JsonType::Object: {
                ss << '{';
                bool first = true;
                for (const auto& [k, v] : m_obj) {
                    if (!first) ss << ',';
                    first = false;
                    ss << '"' << escapeString(k) << "\":";
                    v.serializeInternal(ss);
                }
                ss << '}';
                break;
            }
        }
    }

    static JsonValue parseValue(std::string_view s, size_t& idx) {
        skipWhitespace(s, idx);
        if (idx >= s.size()) return JsonValue();

        char c = s[idx];
        if (c == 'n') return parseNull(s, idx);
        if (c == 't' || c == 'f') return parseBool(s, idx);
        if (c == '"') return parseString(s, idx);
        if (c == '[') return parseArray(s, idx);
        if (c == '{') return parseObject(s, idx);
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber(s, idx);

        return JsonValue();
    }

    static JsonValue parseNull(std::string_view s, size_t& idx) {
        if (s.substr(idx, 4) == "null") {
            idx += 4;
            return JsonValue();
        }
        return JsonValue();
    }

    static JsonValue parseBool(std::string_view s, size_t& idx) {
        if (s.substr(idx, 4) == "true") {
            idx += 4;
            return JsonValue(true);
        }
        if (s.substr(idx, 5) == "false") {
            idx += 5;
            return JsonValue(false);
        }
        return JsonValue();
    }

    static JsonValue parseNumber(std::string_view s, size_t& idx) {
        size_t start = idx;
        if (s[idx] == '-') ++idx;
        bool isFloat = false;
        while (idx < s.size() && (std::isdigit(static_cast<unsigned char>(s[idx])) || s[idx] == '.' || s[idx] == 'e' || s[idx] == 'E' || s[idx] == '+' || s[idx] == '-')) {
            if (s[idx] == '.' || s[idx] == 'e' || s[idx] == 'E') {
                isFloat = true;
            }
            ++idx;
        }

        std::string numStr(s.substr(start, idx - start));
        try {
            if (isFloat) {
                return JsonValue(std::stod(numStr));
            } else {
                return JsonValue(static_cast<int64_t>(std::stoll(numStr)));
            }
        } catch (...) {
            return JsonValue(0);
        }
    }

    static JsonValue parseString(std::string_view s, size_t& idx) {
        if (idx >= s.size() || s[idx] != '"') return JsonValue("");
        ++idx; // skip opening quote

        std::string res;
        while (idx < s.size()) {
            char c = s[idx++];
            if (c == '"') {
                return JsonValue(res);
            }
            if (c == '\\' && idx < s.size()) {
                char esc = s[idx++];
                switch (esc) {
                    case '"': res += '"'; break;
                    case '\\': res += '\\'; break;
                    case '/': res += '/'; break;
                    case 'b': res += '\b'; break;
                    case 'f': res += '\f'; break;
                    case 'n': res += '\n'; break;
                    case 'r': res += '\r'; break;
                    case 't': res += '\t'; break;
                    case 'u': {
                        if (idx + 4 <= s.size()) {
                            // Basic unicode escape, pass through or decode ASCII
                            std::string hexStr(s.substr(idx, 4));
                            idx += 4;
                            try {
                                unsigned long code = std::stoul(hexStr, nullptr, 16);
                                if (code < 128) {
                                    res += static_cast<char>(code);
                                } else {
                                    res += '?';
                                }
                            } catch (...) {
                                res += '?';
                            }
                        }
                        break;
                    }
                    default: res += esc; break;
                }
            } else {
                res += c;
            }
        }
        return JsonValue(res);
    }

    static JsonValue parseArray(std::string_view s, size_t& idx) {
        if (idx >= s.size() || s[idx] != '[') return JsonValue(Array{});
        ++idx; // skip '['

        Array arr;
        skipWhitespace(s, idx);
        if (idx < s.size() && s[idx] == ']') {
            ++idx;
            return JsonValue(arr);
        }

        while (idx < s.size()) {
            arr.push_back(parseValue(s, idx));
            skipWhitespace(s, idx);
            if (idx < s.size() && s[idx] == ',') {
                ++idx;
                skipWhitespace(s, idx);
            } else if (idx < s.size() && s[idx] == ']') {
                ++idx;
                break;
            } else {
                break;
            }
        }
        return JsonValue(arr);
    }

    static JsonValue parseObject(std::string_view s, size_t& idx) {
        if (idx >= s.size() || s[idx] != '{') return JsonValue(Object{});
        ++idx; // skip '{'

        Object obj;
        skipWhitespace(s, idx);
        if (idx < s.size() && s[idx] == '}') {
            ++idx;
            return JsonValue(obj);
        }

        while (idx < s.size()) {
            skipWhitespace(s, idx);
            if (idx >= s.size() || s[idx] != '"') break;

            JsonValue keyVal = parseString(s, idx);
            std::string key = keyVal.asString();

            skipWhitespace(s, idx);
            if (idx < s.size() && s[idx] == ':') {
                ++idx;
            } else {
                break;
            }

            JsonValue val = parseValue(s, idx);
            obj[key] = std::move(val);

            skipWhitespace(s, idx);
            if (idx < s.size() && s[idx] == ',') {
                ++idx;
                skipWhitespace(s, idx);
            } else if (idx < s.size() && s[idx] == '}') {
                ++idx;
                break;
            } else {
                break;
            }
        }
        return JsonValue(obj);
    }
};

} // namespace sph::lsp
