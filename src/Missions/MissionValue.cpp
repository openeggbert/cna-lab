#include "IronGang/Missions/MissionValue.hpp"

#include <array>
#include <charconv>
#include <cstdlib>
#include <system_error>

namespace IronGang
{
    const char* MissionValueTypeName(MissionValueType type) noexcept
    {
        switch (type)
        {
            case MissionValueType::Bool: return "bool";
            case MissionValueType::Int: return "int";
            case MissionValueType::Float: return "float";
            case MissionValueType::String: return "string";
        }
        return "bool";
    }

    bool ParseMissionValueType(const std::string& name, MissionValueType& out)
    {
        if (name == "bool") { out = MissionValueType::Bool; return true; }
        if (name == "int") { out = MissionValueType::Int; return true; }
        if (name == "float") { out = MissionValueType::Float; return true; }
        if (name == "string") { out = MissionValueType::String; return true; }
        return false;
    }

    MissionValue MissionValue::Bool(bool value)
    {
        MissionValue result;
        result.type_ = MissionValueType::Bool;
        result.bool_ = value;
        return result;
    }

    MissionValue MissionValue::Int(int value)
    {
        MissionValue result;
        result.type_ = MissionValueType::Int;
        result.int_ = value;
        return result;
    }

    MissionValue MissionValue::Float(float value)
    {
        MissionValue result;
        result.type_ = MissionValueType::Float;
        result.float_ = value;
        return result;
    }

    MissionValue MissionValue::String(std::string value)
    {
        MissionValue result;
        result.type_ = MissionValueType::String;
        result.string_ = std::move(value);
        return result;
    }

    float MissionValue::AsFloat() const noexcept
    {
        switch (type_)
        {
            case MissionValueType::Int: return static_cast<float>(int_);
            case MissionValueType::Float: return float_;
            case MissionValueType::Bool:
            case MissionValueType::String: return 0.0F;
        }
        return 0.0F;
    }

    std::string MissionValue::ToText() const
    {
        switch (type_)
        {
            case MissionValueType::Bool:
                return bool_ ? "true" : "false";
            case MissionValueType::Int:
                return std::to_string(int_);
            case MissionValueType::Float:
            {
                // Shortest round-trip form: std::to_string would fix 6 decimals and lose value.
                std::array<char, 48> buffer{};
                const std::to_chars_result result =
                    std::to_chars(buffer.data(), buffer.data() + buffer.size(), float_);
                if (result.ec != std::errc())
                {
                    return "0";
                }
                return std::string(buffer.data(), result.ptr);
            }
            case MissionValueType::String:
                return string_;
        }
        return {};
    }

    bool MissionValue::Parse(MissionValueType type, const std::string& text, MissionValue& out)
    {
        switch (type)
        {
            case MissionValueType::Bool:
                if (text == "true") { out = Bool(true); return true; }
                if (text == "false") { out = Bool(false); return true; }
                return false;
            case MissionValueType::Int:
            {
                int value = 0;
                const char* begin = text.data();
                const char* end = text.data() + text.size();
                const std::from_chars_result result = std::from_chars(begin, end, value);
                if (result.ec != std::errc() || result.ptr != end)
                {
                    return false;
                }
                out = Int(value);
                return true;
            }
            case MissionValueType::Float:
            {
                float value = 0.0F;
                const char* begin = text.data();
                const char* end = text.data() + text.size();
                const std::from_chars_result result = std::from_chars(begin, end, value);
                if (result.ec != std::errc() || result.ptr != end)
                {
                    return false;
                }
                out = Float(value);
                return true;
            }
            case MissionValueType::String:
                out = String(text);
                return true;
        }
        return false;
    }
}
