#pragma once

#include <string>

namespace IronGang
{
    // plan_24-mission-framework-and-scripting.md IG-24-005: the closed set of types a mission
    // variable, mission fact, or expression result can have. Deliberately four scalar types --
    // missions describe state ("did the player pay?", "how many crates are loaded?"), not
    // structured data, and every one of these round-trips through the plain-text save file.
    enum class MissionValueType
    {
        Bool,
        Int,
        Float,
        String,
    };

    // "bool"/"int"/"float"/"string" -- the exact spellings a mission file's "type" field uses.
    [[nodiscard]] const char* MissionValueTypeName(MissionValueType type) noexcept;
    [[nodiscard]] bool ParseMissionValueType(const std::string& name, MissionValueType& out);

    // One typed mission value. Constructed through the named factories so the type is always
    // explicit at the call site (MissionValue::Int(0) rather than an implicit conversion picking
    // a type for the caller); accessors are only meaningful for the matching GetType().
    class MissionValue final
    {
    public:
        MissionValue() = default;

        [[nodiscard]] static MissionValue Bool(bool value);
        [[nodiscard]] static MissionValue Int(int value);
        [[nodiscard]] static MissionValue Float(float value);
        [[nodiscard]] static MissionValue String(std::string value);

        [[nodiscard]] MissionValueType GetType() const noexcept { return type_; }
        [[nodiscard]] bool AsBool() const noexcept { return bool_; }
        [[nodiscard]] int AsInt() const noexcept { return int_; }
        // Int values convert to float here so numeric expressions can mix the two without the
        // caller branching on the type; String/Bool return 0.
        [[nodiscard]] float AsFloat() const noexcept;
        [[nodiscard]] const std::string& AsString() const noexcept { return string_; }
        [[nodiscard]] bool IsNumeric() const noexcept
        {
            return type_ == MissionValueType::Int || type_ == MissionValueType::Float;
        }

        // Save-file/diagnostic text. Float uses the shortest representation that parses back to
        // the same value, so ToText()/Parse() round-trip exactly (see SaveGame).
        [[nodiscard]] std::string ToText() const;
        // Parses text produced by ToText() (and hand-written mission-file literals) as @p type.
        // Returns false for text that is not a complete, valid value of that type.
        [[nodiscard]] static bool Parse(MissionValueType type, const std::string& text, MissionValue& out);

    private:
        MissionValueType type_{MissionValueType::Bool};
        bool bool_{false};
        int int_{0};
        float float_{0.0F};
        std::string string_;
    };

    // One mission variable's persisted name/value pair (SaveGame, IG-24-029/039).
    struct MissionVariableSnapshot
    {
        std::string name;
        MissionValue value;
    };
}
