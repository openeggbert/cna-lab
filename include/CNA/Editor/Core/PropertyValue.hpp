// SPDX-License-Identifier: MS-PL
#pragma once

/**
 * @file CNA/Editor/Core/PropertyValue.hpp
 * @brief The dynamically-typed value every reflected component property is read and written as.
 *
 * C++ has no usable built-in reflection, so the editor carries its own metadata (see
 * ComponentDescriptor). The inspector, the undo stack and the serialiser all speak this one
 * type, which is what lets a single generic SetPropertyCommand undo a change to *any* property of
 * *any* component -- including component types supplied by a plugin that the editor was never
 * compiled against.
 */

#include <string>
#include <variant>
#include <vector>

#include "CNA/Editor/Core/EditorMath.hpp"
#include "CNA/Editor/Core/Uuid.hpp"

namespace CNA::Editor
{
    class JsonValue;

    /** @brief Names the alternative a PropertyValue currently holds. */
    enum class PropertyType
    {
        /** @brief No value. Returned when a property lookup misses. */
        None,
        Boolean,
        Integer,
        Float,
        String,
        /** @brief A named choice; the value is the option's string name, not its ordinal. */
        Enum,
        Color,
        Vector2,
        Vector3,
        Vector4,
        Quaternion,
        Rectangle,
        /** @brief A reference to an asset by its stable AssetDatabase id. */
        AssetReference,
        /** @brief A reference to another entity in the same scene document. */
        EntityReference
    };

    /** @brief Returns the stable textual name of @p type as written into scene files. */
    const char* toString(PropertyType type);

    /** @brief Parses the textual name produced by toString(); returns PropertyType::None on failure. */
    PropertyType parsePropertyType(std::string_view text);

    /**
     * @brief A tagged union covering every property type the inspector can edit.
     *
     * Enum and AssetReference/EntityReference are deliberately *distinct* alternatives rather than
     * plain strings and UUIDs. The inspector needs the distinction to pick a widget (a combo box
     * versus a text field, an asset picker versus an entity picker), and the asset database needs
     * it to walk a scene's outbound references without consulting the descriptor for every field.
     */
    class PropertyValue
    {
    public:
        /** @brief Wraps a string so the variant can tell an enum name from an ordinary string. */
        struct EnumValue
        {
            std::string name;
            friend bool operator==(const EnumValue& lhs, const EnumValue& rhs) { return lhs.name == rhs.name; }
        };

        /** @brief Wraps a Uuid so the variant can tell an asset reference from an entity reference. */
        struct AssetReference
        {
            Uuid id;
            friend bool operator==(const AssetReference& lhs, const AssetReference& rhs) { return lhs.id == rhs.id; }
        };

        /** @brief Wraps a Uuid so the variant can tell an entity reference from an asset reference. */
        struct EntityReference
        {
            Uuid id;
            friend bool operator==(const EntityReference& lhs, const EntityReference& rhs) { return lhs.id == rhs.id; }
        };

        using Storage = std::variant<std::monostate,
                                     bool,
                                     std::int64_t,
                                     float,
                                     std::string,
                                     EnumValue,
                                     EditorColor,
                                     EditorVector2,
                                     EditorVector3,
                                     EditorVector4,
                                     EditorQuaternion,
                                     EditorRectangle,
                                     AssetReference,
                                     EntityReference>;

        PropertyValue() = default;
        PropertyValue(bool value) : storage_(value) {}
        PropertyValue(int value) : storage_(static_cast<std::int64_t>(value)) {}
        PropertyValue(std::int64_t value) : storage_(value) {}
        PropertyValue(float value) : storage_(value) {}
        PropertyValue(double value) : storage_(static_cast<float>(value)) {}
        PropertyValue(std::string value) : storage_(std::move(value)) {}
        PropertyValue(const char* value) : storage_(std::string{value ? value : ""}) {}
        PropertyValue(EnumValue value) : storage_(std::move(value)) {}
        PropertyValue(EditorColor value) : storage_(value) {}
        PropertyValue(EditorVector2 value) : storage_(value) {}
        PropertyValue(EditorVector3 value) : storage_(value) {}
        PropertyValue(EditorVector4 value) : storage_(value) {}
        PropertyValue(EditorQuaternion value) : storage_(value) {}
        PropertyValue(EditorRectangle value) : storage_(value) {}
        PropertyValue(AssetReference value) : storage_(value) {}
        PropertyValue(EntityReference value) : storage_(value) {}

        /** @brief Returns which alternative is held. */
        [[nodiscard]] PropertyType getType() const;

        /** @brief Returns true when no value is held. */
        [[nodiscard]] bool isEmpty() const { return std::holds_alternative<std::monostate>(storage_); }

        /**
         * @brief Returns the held value when it is a @p T, otherwise @p fallback.
         *
         * Never throws, unlike std::get: an inspector reading a property whose type changed under
         * it (a plugin reload, a hand-edited scene file) should degrade to the default rather than
         * take the process down.
         */
        template <typename T>
        [[nodiscard]] T get(T fallback = T{}) const
        {
            const T* held = std::get_if<T>(&storage_);
            return held != nullptr ? *held : fallback;
        }

        /** @brief Returns the underlying variant, for callers that want to std::visit it. */
        [[nodiscard]] const Storage& getStorage() const { return storage_; }

        friend bool operator==(const PropertyValue& lhs, const PropertyValue& rhs)
        {
            return lhs.storage_ == rhs.storage_;
        }
        friend bool operator!=(const PropertyValue& lhs, const PropertyValue& rhs) { return !(lhs == rhs); }

        /**
         * @brief Serialises to the JSON shape documented in docs/FORMATS.md.
         *
         * Scalars become JSON scalars; vectors and colours become fixed-length arrays; references
         * become their UUID string. The type is *not* embedded -- the ComponentDescriptor supplies
         * it on load, which keeps scene files small and readable.
         */
        [[nodiscard]] JsonValue toJson() const;

        /**
         * @brief Reads a value of the type @p type demands out of @p json.
         *
         * @param json The serialised form produced by toJson().
         * @param type The type the ComponentDescriptor declares for this property.
         * @return The parsed value, or an empty PropertyValue when @p json does not fit @p type.
         */
        static PropertyValue fromJson(const JsonValue& json, PropertyType type);

        /** @brief Returns the natural zero value for @p type; used when a scene file omits a field. */
        static PropertyValue defaultOf(PropertyType type);

        /** @brief Returns a short human-readable rendering, for the console and for command labels. */
        [[nodiscard]] std::string toDisplayString() const;

    private:
        Storage storage_;
    };
}
