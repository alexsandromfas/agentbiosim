#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// Phase 28: a small, dependency-free JSON value + parser + writer. The project
// vendors no JSON library, so this is a self-contained implementation covering
// exactly what persistence needs: objects (insertion-ordered), arrays, strings
// (with escapes), integers, doubles, booleans and null.
//
// Doubles are written with %.17g, which round-trips IEEE-754 binary64 exactly,
// so a save -> load -> save cycle reproduces values bit-for-bit. Integers are a
// distinct type so ids/counters stay exact and compact.
namespace agentbiosim::io
{
class Json
{
public:
    enum class Type
    {
        Null,
        Bool,
        Int,
        Double,
        String,
        Array,
        Object
    };

    Json() = default;
    static Json makeObject() { Json j; j.type_ = Type::Object; return j; }
    static Json makeArray() { Json j; j.type_ = Type::Array; return j; }

    Json(bool value) : type_(Type::Bool), bool_(value) {}
    Json(int value) : type_(Type::Int), int_(value) {}
    Json(std::int64_t value) : type_(Type::Int), int_(value) {}
    // Note: on this target std::size_t == std::uint64_t, so a separate size_t
    // overload would be a redefinition; size_t callers bind to this one.
    Json(std::uint64_t value) : type_(Type::Int), int_(static_cast<std::int64_t>(value)) {}
    Json(std::uint32_t value) : type_(Type::Int), int_(static_cast<std::int64_t>(value)) {}
    Json(double value) : type_(Type::Double), double_(value) {}
    Json(const char* value) : type_(Type::String), string_(value) {}
    Json(std::string value) : type_(Type::String), string_(std::move(value)) {}

    [[nodiscard]] Type type() const noexcept { return type_; }
    [[nodiscard]] bool isObject() const noexcept { return type_ == Type::Object; }
    [[nodiscard]] bool isArray() const noexcept { return type_ == Type::Array; }
    [[nodiscard]] bool isNull() const noexcept { return type_ == Type::Null; }

    // ---- builders ----
    // Object: set/overwrite a member (preserves first-insertion order).
    Json& set(const std::string& key, Json value);
    // Array: append.
    Json& push(Json value);

    // ---- object access ----
    [[nodiscard]] bool contains(const std::string& key) const;
    [[nodiscard]] const Json* find(const std::string& key) const;
    // Typed getters with defaults (forgiving: int<->double coerce).
    [[nodiscard]] bool getBool(const std::string& key, bool fallback = false) const;
    [[nodiscard]] std::int64_t getInt(const std::string& key, std::int64_t fallback = 0) const;
    [[nodiscard]] double getDouble(const std::string& key, double fallback = 0.0) const;
    [[nodiscard]] std::string getString(const std::string& key, const std::string& fallback = {}) const;

    // ---- scalar access ----
    [[nodiscard]] bool asBool() const noexcept { return type_ == Type::Bool ? bool_ : false; }
    [[nodiscard]] std::int64_t asInt() const noexcept;
    [[nodiscard]] double asDouble() const noexcept;
    [[nodiscard]] const std::string& asString() const noexcept { return string_; }

    // ---- array access ----
    [[nodiscard]] std::size_t size() const noexcept { return array_.size(); }
    [[nodiscard]] const Json& at(std::size_t index) const { return array_[index]; }
    [[nodiscard]] const std::vector<Json>& items() const noexcept { return array_; }

    // ---- members access ----
    [[nodiscard]] const std::vector<std::pair<std::string, Json>>& members() const noexcept { return object_; }

    // ---- serialize / parse ----
    [[nodiscard]] std::string dump(bool pretty = true) const;
    // Returns true on success; on failure `error` carries a message.
    [[nodiscard]] static bool parse(const std::string& text, Json& out, std::string& error);

private:
    void dumpInto(std::string& out, bool pretty, int indent) const;

    Type type_ = Type::Null;
    bool bool_ = false;
    std::int64_t int_ = 0;
    double double_ = 0.0;
    std::string string_;
    std::vector<Json> array_;
    std::vector<std::pair<std::string, Json>> object_;
};
} // namespace agentbiosim::io
