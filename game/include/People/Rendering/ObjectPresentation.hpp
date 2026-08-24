#pragma once

#include <cstdint>
#include <string_view>

#include "People/Objects/ObjectModel.hpp"
#include "People/World/IsometricProjection.hpp"

namespace People::Rendering
{
    /** @brief Direction of an authored frame after applying presentation rotation. */
    enum class SpriteDirection : std::uint8_t
    {
        North = 0,
        East = 1,
        South = 2,
        West = 3
    };

    struct ObjectSpriteSelection
    {
        SpriteDirection direction = SpriteDirection::North;
        const Objects::ObjectSpriteReference* reference = nullptr;
    };

    /** @brief Pure selection of authored metadata; it never owns runtime textures. */
    class ObjectPresentation final
    {
    public:
        /**
         * @brief Rotates a simulation-facing direction into current view space.
         *
         * The transform uses the same clockwise convention as
         * IsometricProjection::Rotate: a north-facing object presents east
         * after one clockwise camera/world presentation turn.
         */
        [[nodiscard]] static SpriteDirection PresentedDirection(
            Objects::ObjectRotation objectRotation,
            World::ViewRotation viewRotation);

        /** @brief Resolves an exact state and direction to immutable asset metadata. */
        [[nodiscard]] static ObjectSpriteSelection SelectSprite(
            const Objects::ObjectDefinition& definition,
            std::string_view state,
            Objects::ObjectRotation objectRotation,
            World::ViewRotation viewRotation);

        /** @brief Resolves the definition's authored default visual state. */
        [[nodiscard]] static ObjectSpriteSelection SelectDefaultSprite(
            const Objects::ObjectDefinition& definition,
            Objects::ObjectRotation objectRotation,
            World::ViewRotation viewRotation);
    };
}
