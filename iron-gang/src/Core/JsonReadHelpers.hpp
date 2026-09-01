#pragma once

// Small readers shared by the data loaders. Extracted when SidewalkGraph needed the same four
// helpers RoadGraph had already grown: a second copy is how two subtly different implementations
// of "reject unknown fields" get written, and unknown-field rejection is only worth anything if
// every loader does it the same way.
//
// Private to the library's own sources, like JsonDataFileInternal.hpp, because these take
// sharp-runtime JSON types and nothing public may.

#include "System/Text/Json/JsonElement.hpp"
#include "System/Text/Json/JsonProperty.hpp"

#include "IronGang/Core/WorldTypes.hpp"

#include <algorithm>
#include <initializer_list>
#include <string>

namespace IronGang::JsonRead
{
    using System::Text::Json::JsonElement;
    using System::Text::Json::JsonValueKind;

    [[nodiscard]] inline bool Vector3Field(const JsonElement& element, Vector3& out)
    {
        if (element.getValueKindProperty() != JsonValueKind::Array)
        {
            return false;
        }
        float values[3] = {0.0F, 0.0F, 0.0F};
        std::size_t count = 0;
        for (const JsonElement& entry : element.EnumerateArray())
        {
            if (count >= 3 || entry.getValueKindProperty() != JsonValueKind::Number)
            {
                return false;
            }
            values[count] = static_cast<float>(entry.GetDouble());
            ++count;
        }
        if (count != 3)
        {
            return false;
        }
        out = Vector3(values[0], values[1], values[2]);
        return true;
    }

    // Rejects any field not in @p allowed, naming it. A typo in a data file is otherwise a setting
    // that silently does nothing.
    [[nodiscard]] inline bool OnlyFields(const JsonElement& element,
                                         std::initializer_list<const char*> allowed,
                                         const char* what,
                                         const std::string& path,
                                         std::string& error)
    {
        for (const auto& property : element.EnumerateObject())
        {
            const std::string name = property.getNameProperty();
            if (std::find_if(allowed.begin(), allowed.end(), [&name](const char* candidate) {
                    return name == candidate;
                }) == allowed.end())
            {
                error = std::string("unknown field \"") + name + "\" in " + what + ": " + path;
                return false;
            }
        }
        return true;
    }

    // An empty string counts as absent: an id nobody can reference is not an id.
    [[nodiscard]] inline bool StringField(const JsonElement& owner, const char* field, std::string& out)
    {
        JsonElement element;
        if (!owner.TryGetProperty(field, element) ||
            element.getValueKindProperty() != JsonValueKind::String)
        {
            return false;
        }
        out = element.GetString();
        return !out.empty();
    }

    [[nodiscard]] inline bool NumberField(const JsonElement& owner, const char* field, double& out)
    {
        JsonElement element;
        if (!owner.TryGetProperty(field, element) ||
            element.getValueKindProperty() != JsonValueKind::Number)
        {
            return false;
        }
        out = element.GetDouble();
        return true;
    }

    // Absent leaves @p out untouched; present but not a boolean is an error.
    [[nodiscard]] inline bool OptionalBoolField(const JsonElement& owner, const char* field, bool& out,
                                                bool& malformed)
    {
        malformed = false;
        JsonElement element;
        if (!owner.TryGetProperty(field, element))
        {
            return false;
        }
        const JsonValueKind kind = element.getValueKindProperty();
        if (kind != JsonValueKind::True && kind != JsonValueKind::False)
        {
            malformed = true;
            return false;
        }
        out = kind == JsonValueKind::True;
        return true;
    }
}
