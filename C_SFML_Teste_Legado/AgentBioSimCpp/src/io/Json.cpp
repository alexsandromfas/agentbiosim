#include "io/Json.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace agentbiosim::io
{
namespace
{
void appendEscaped(std::string& out, const std::string& s)
{
    out.push_back('"');
    for (const char c : s)
    {
        switch (c)
        {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20)
            {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(c)));
                out += buf;
            }
            else
            {
                out.push_back(c);
            }
        }
    }
    out.push_back('"');
}

void appendIndent(std::string& out, const int indent)
{
    out.push_back('\n');
    for (int i = 0; i < indent; ++i) out += "  ";
}
} // namespace

Json& Json::set(const std::string& key, Json value)
{
    type_ = Type::Object;
    for (auto& kv : object_)
    {
        if (kv.first == key)
        {
            kv.second = std::move(value);
            return *this;
        }
    }
    object_.emplace_back(key, std::move(value));
    return *this;
}

Json& Json::push(Json value)
{
    type_ = Type::Array;
    array_.push_back(std::move(value));
    return *this;
}

bool Json::contains(const std::string& key) const { return find(key) != nullptr; }

const Json* Json::find(const std::string& key) const
{
    for (const auto& kv : object_)
    {
        if (kv.first == key) return &kv.second;
    }
    return nullptr;
}

std::int64_t Json::asInt() const noexcept
{
    if (type_ == Type::Int) return int_;
    if (type_ == Type::Double) return static_cast<std::int64_t>(double_);
    if (type_ == Type::Bool) return bool_ ? 1 : 0;
    return 0;
}

double Json::asDouble() const noexcept
{
    if (type_ == Type::Double) return double_;
    if (type_ == Type::Int) return static_cast<double>(int_);
    return 0.0;
}

bool Json::getBool(const std::string& key, const bool fallback) const
{
    const Json* j = find(key);
    return j != nullptr ? j->asBool() : fallback;
}
std::int64_t Json::getInt(const std::string& key, const std::int64_t fallback) const
{
    const Json* j = find(key);
    return j != nullptr ? j->asInt() : fallback;
}
double Json::getDouble(const std::string& key, const double fallback) const
{
    const Json* j = find(key);
    return j != nullptr ? j->asDouble() : fallback;
}
std::string Json::getString(const std::string& key, const std::string& fallback) const
{
    const Json* j = find(key);
    return (j != nullptr && j->type_ == Type::String) ? j->string_ : fallback;
}

void Json::dumpInto(std::string& out, const bool pretty, const int indent) const
{
    switch (type_)
    {
    case Type::Null: out += "null"; break;
    case Type::Bool: out += bool_ ? "true" : "false"; break;
    case Type::Int:
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(int_));
        out += buf;
        break;
    }
    case Type::Double:
    {
        if (std::isfinite(double_))
        {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.17g", double_);
            out += buf;
        }
        else
        {
            out += "0";  // JSON has no NaN/Inf; persist as 0 (never expected here).
        }
        break;
    }
    case Type::String: appendEscaped(out, string_); break;
    case Type::Array:
    {
        if (array_.empty()) { out += "[]"; break; }
        out.push_back('[');
        for (std::size_t i = 0; i < array_.size(); ++i)
        {
            if (pretty) appendIndent(out, indent + 1);
            array_[i].dumpInto(out, pretty, indent + 1);
            if (i + 1 < array_.size()) out.push_back(',');
        }
        if (pretty) appendIndent(out, indent);
        out.push_back(']');
        break;
    }
    case Type::Object:
    {
        if (object_.empty()) { out += "{}"; break; }
        out.push_back('{');
        for (std::size_t i = 0; i < object_.size(); ++i)
        {
            if (pretty) appendIndent(out, indent + 1);
            appendEscaped(out, object_[i].first);
            out += pretty ? ": " : ":";
            object_[i].second.dumpInto(out, pretty, indent + 1);
            if (i + 1 < object_.size()) out.push_back(',');
        }
        if (pretty) appendIndent(out, indent);
        out.push_back('}');
        break;
    }
    }
}

std::string Json::dump(const bool pretty) const
{
    std::string out;
    dumpInto(out, pretty, 0);
    return out;
}

// ----------------------------- parser ---------------------------------------
namespace
{
struct Parser
{
    const std::string& text;
    std::size_t pos = 0;
    std::string error;

    explicit Parser(const std::string& t) : text(t) {}

    void skipWs()
    {
        while (pos < text.size())
        {
            const char c = text[pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++pos;
            else break;
        }
    }

    bool fail(const std::string& msg)
    {
        if (error.empty()) error = msg + " at offset " + std::to_string(pos);
        return false;
    }

    bool parseValue(Json& out)
    {
        skipWs();
        if (pos >= text.size()) return fail("unexpected end");
        const char c = text[pos];
        switch (c)
        {
        case '{': return parseObject(out);
        case '[': return parseArray(out);
        case '"':
        {
            std::string s;
            if (!parseString(s)) return false;
            out = Json(std::move(s));
            return true;
        }
        case 't': case 'f': return parseBool(out);
        case 'n': return parseNull(out);
        default: return parseNumber(out);
        }
    }

    bool parseObject(Json& out)
    {
        out = Json::makeObject();
        ++pos;  // {
        skipWs();
        if (pos < text.size() && text[pos] == '}') { ++pos; return true; }
        while (true)
        {
            skipWs();
            if (pos >= text.size() || text[pos] != '"') return fail("expected key");
            std::string key;
            if (!parseString(key)) return false;
            skipWs();
            if (pos >= text.size() || text[pos] != ':') return fail("expected ':'");
            ++pos;
            Json value;
            if (!parseValue(value)) return false;
            out.set(key, std::move(value));
            skipWs();
            if (pos >= text.size()) return fail("unterminated object");
            if (text[pos] == ',') { ++pos; continue; }
            if (text[pos] == '}') { ++pos; return true; }
            return fail("expected ',' or '}'");
        }
    }

    bool parseArray(Json& out)
    {
        out = Json::makeArray();
        ++pos;  // [
        skipWs();
        if (pos < text.size() && text[pos] == ']') { ++pos; return true; }
        while (true)
        {
            Json value;
            if (!parseValue(value)) return false;
            out.push(std::move(value));
            skipWs();
            if (pos >= text.size()) return fail("unterminated array");
            if (text[pos] == ',') { ++pos; continue; }
            if (text[pos] == ']') { ++pos; return true; }
            return fail("expected ',' or ']'");
        }
    }

    bool parseString(std::string& out)
    {
        ++pos;  // opening quote
        out.clear();
        while (pos < text.size())
        {
            const char c = text[pos++];
            if (c == '"') return true;
            if (c == '\\')
            {
                if (pos >= text.size()) return fail("bad escape");
                const char e = text[pos++];
                switch (e)
                {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'u':
                {
                    if (pos + 4 > text.size()) return fail("bad \\u");
                    const std::string hex = text.substr(pos, 4);
                    pos += 4;
                    const unsigned code = static_cast<unsigned>(std::strtoul(hex.c_str(), nullptr, 16));
                    // Minimal: emit ASCII directly, otherwise '?'. The persistence
                    // layer only writes ASCII, so this is sufficient.
                    out.push_back(code < 0x80 ? static_cast<char>(code) : '?');
                    break;
                }
                default: return fail("unknown escape");
                }
            }
            else
            {
                out.push_back(c);
            }
        }
        return fail("unterminated string");
    }

    bool parseBool(Json& out)
    {
        if (text.compare(pos, 4, "true") == 0) { pos += 4; out = Json(true); return true; }
        if (text.compare(pos, 5, "false") == 0) { pos += 5; out = Json(false); return true; }
        return fail("invalid literal");
    }

    bool parseNull(Json& out)
    {
        if (text.compare(pos, 4, "null") == 0) { pos += 4; out = Json(); return true; }
        return fail("invalid literal");
    }

    bool parseNumber(Json& out)
    {
        const std::size_t start = pos;
        bool isDouble = false;
        if (pos < text.size() && (text[pos] == '-' || text[pos] == '+')) ++pos;
        while (pos < text.size())
        {
            const char c = text[pos];
            if (c >= '0' && c <= '9') { ++pos; }
            else if (c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') { isDouble = true; ++pos; }
            else break;
        }
        if (pos == start) return fail("invalid number");
        const std::string num = text.substr(start, pos - start);
        if (isDouble)
        {
            out = Json(std::strtod(num.c_str(), nullptr));
        }
        else
        {
            out = Json(static_cast<std::int64_t>(std::strtoll(num.c_str(), nullptr, 10)));
        }
        return true;
    }
};
} // namespace

bool Json::parse(const std::string& text, Json& out, std::string& error)
{
    Parser parser(text);
    if (!parser.parseValue(out))
    {
        error = parser.error;
        return false;
    }
    parser.skipWs();
    // Trailing content is tolerated (allows a final newline); not an error.
    return true;
}
} // namespace agentbiosim::io
