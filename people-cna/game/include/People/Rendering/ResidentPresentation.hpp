#pragma once

#include <array>
#include <cstddef>
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

    /**
     * @brief Authored walk frames for all four presented directions.
     *
     * Frames are ordered by walk-cycle phase, not by direction. Every frame
     * shares the idle foot anchor so a resident never shifts when the runtime
     * switches between idle and walk metadata.
     */
    struct ResidentWalkSpriteSet
    {
        static constexpr std::size_t FrameCount = 2;

        std::array<std::array<ResidentSpriteReference, 4>, FrameCount> frames;
    };

    /** @brief Pure resident-facing presentation with no animation or texture state. */
    class ResidentPresentation final
    {
    public:
        /**
         * @brief Travelled movement units that advance the walk cycle by one frame.
         *
         * `MovementExecutor::ProgressUnitsPerTile` is 1000, so the default cycle
         * shows both frames exactly once per traversed tile. The value is a
         * simulation-unit constant rather than a duration because presentation
         * must never derive animation state from the render frame rate.
         */
        static constexpr std::uint32_t WalkUnitsPerFrame = 500;

        [[nodiscard]] static SpriteDirection PresentedDirection(
            Simulation::ResidentFacing facing,
            World::ViewRotation viewRotation);

        [[nodiscard]] static const ResidentSpriteReference& SelectIdleSprite(
            const ResidentIdleSpriteSet& sprites,
            Simulation::ResidentFacing facing,
            World::ViewRotation viewRotation);

        /**
         * @brief Walk frame for monotone travelled units.
         *
         * Takes travelled units by value: selection reads inspectable movement
         * progress and can never advance, complete, or cancel a route.
         */
        [[nodiscard]] static std::size_t WalkFrameIndex(
            std::uint32_t travelledUnits) noexcept;

        [[nodiscard]] static const ResidentSpriteReference& SelectWalkSprite(
            const ResidentWalkSpriteSet& sprites,
            Simulation::ResidentFacing facing,
            World::ViewRotation viewRotation,
            std::uint32_t travelledUnits);
    };
}
