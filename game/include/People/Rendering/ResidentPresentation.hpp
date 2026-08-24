#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "People/Rendering/ObjectPresentation.hpp"
#include "People/Simulation/ResidentModel.hpp"

namespace People::Rendering
{
    struct ResidentSpriteReference
    {
        std::string assetId;
        int footAnchorX = 0;
        int footAnchorY = 0;

        bool operator==(const ResidentSpriteReference&) const = default;
    };

    struct ResidentIdleSpriteSet
    {
        std::array<ResidentSpriteReference, 4> directions;
    };

    /** @brief Pure resident-facing presentation with no animation or texture state. */
    class ResidentPresentation final
    {
    public:
        [[nodiscard]] static SpriteDirection PresentedDirection(
            Simulation::ResidentFacing facing,
            World::ViewRotation viewRotation);

        [[nodiscard]] static const ResidentSpriteReference& SelectIdleSprite(
            const ResidentIdleSpriteSet& sprites,
            Simulation::ResidentFacing facing,
            World::ViewRotation viewRotation);
    };
}
