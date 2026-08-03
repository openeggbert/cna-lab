// SPDX-License-Identifier: MS-PL
#pragma once

/**
 * @file CNA/Editor/Core/EditorMath.hpp
 * @brief Plain-old-data vector, quaternion, colour and rectangle types for the document model.
 *
 * These deliberately duplicate the shapes of `Microsoft::Xna::Framework::Vector3`, `Quaternion`,
 * `Color` and `Rectangle` rather than reusing them. The reason is ANALYSIS.md decision D-03:
 * cna-editor-core must link nothing from CNA, so that the document model, the undo stack and the
 * asset database can be built and unit-tested without a graphics backend, a window, or even a
 * CNA checkout. The conversion to and from the real CNA types happens in exactly one place --
 * cna-editor-viewport -- where the dependency is unavoidable anyway.
 *
 * Layout is chosen to match the CNA types member-for-member, so that conversion stays a field
 * copy and never a reinterpretation.
 */

#include <cstdint>

namespace CNA::Editor
{
    /** @brief Two-component float vector. Mirrors Microsoft::Xna::Framework::Vector2. */
    struct EditorVector2
    {
        float x = 0.0f;
        float y = 0.0f;

        friend bool operator==(const EditorVector2& lhs, const EditorVector2& rhs)
        {
            return lhs.x == rhs.x && lhs.y == rhs.y;
        }
        friend bool operator!=(const EditorVector2& lhs, const EditorVector2& rhs) { return !(lhs == rhs); }
    };

    /** @brief Three-component float vector. Mirrors Microsoft::Xna::Framework::Vector3. */
    struct EditorVector3
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        friend bool operator==(const EditorVector3& lhs, const EditorVector3& rhs)
        {
            return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
        }
        friend bool operator!=(const EditorVector3& lhs, const EditorVector3& rhs) { return !(lhs == rhs); }
    };

    /** @brief Four-component float vector. Mirrors Microsoft::Xna::Framework::Vector4. */
    struct EditorVector4
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 0.0f;

        friend bool operator==(const EditorVector4& lhs, const EditorVector4& rhs)
        {
            return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w;
        }
        friend bool operator!=(const EditorVector4& lhs, const EditorVector4& rhs) { return !(lhs == rhs); }
    };

    /** @brief Rotation quaternion, identity by default. Mirrors Microsoft::Xna::Framework::Quaternion. */
    struct EditorQuaternion
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 1.0f;

        friend bool operator==(const EditorQuaternion& lhs, const EditorQuaternion& rhs)
        {
            return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w;
        }
        friend bool operator!=(const EditorQuaternion& lhs, const EditorQuaternion& rhs) { return !(lhs == rhs); }
    };

    /** @brief Non-premultiplied RGBA colour, opaque white by default. Mirrors Microsoft::Xna::Framework::Color. */
    struct EditorColor
    {
        std::uint8_t r = 255;
        std::uint8_t g = 255;
        std::uint8_t b = 255;
        std::uint8_t a = 255;

        friend bool operator==(const EditorColor& lhs, const EditorColor& rhs)
        {
            return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
        }
        friend bool operator!=(const EditorColor& lhs, const EditorColor& rhs) { return !(lhs == rhs); }
    };

    /** @brief Integer rectangle in texel space. Mirrors Microsoft::Xna::Framework::Rectangle. */
    struct EditorRectangle
    {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;

        /** @brief Returns true when the rectangle selects no texels, i.e. "use the whole texture". */
        [[nodiscard]] bool isEmpty() const { return width <= 0 || height <= 0; }

        friend bool operator==(const EditorRectangle& lhs, const EditorRectangle& rhs)
        {
            return lhs.x == rhs.x && lhs.y == rhs.y && lhs.width == rhs.width && lhs.height == rhs.height;
        }
        friend bool operator!=(const EditorRectangle& lhs, const EditorRectangle& rhs) { return !(lhs == rhs); }
    };
}
