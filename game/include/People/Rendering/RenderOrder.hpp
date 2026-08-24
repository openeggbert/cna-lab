#pragma once

#include <compare>
#include <cstdint>
#include <span>

#include "People/World/IsometricProjection.hpp"

namespace People::Rendering
{
    /** @brief Coarse painter layers used only after floor and footprint depth. */
    enum class DrawLayer : std::uint8_t
    {
        Terrain,
        FloorCovering,
        FloorDetail,
        WallBack,
        WorldEntity,
        WallFront,
        Effect,
        Overlay
    };

    /**
     * @brief Fully deterministic painter-order key.
     *
     * Declaration order is the comparison contract. Stable ID resolves only a
     * genuine geometric tie and never substitutes for depth.
     */
    struct RenderKey
    {
        int floor = 0;
        int footprintDepth = 0;
        DrawLayer layer = DrawLayer::Terrain;
        int anchorY = 0;
        int anchorX = 0;
        int localOffset = 0;
        std::uint64_t stableId = 0;

        auto operator<=>(const RenderKey&) const = default;
    };

    /** @brief Pure construction of view-specific keys from simulation coordinates. */
    class RenderOrder final
    {
    public:
        /**
         * @brief Builds a key for one sprite or sprite segment.
         *
         * The footprint must be non-empty, in bounds, and on the anchor floor.
         * Its farthest rotated tile determines coarse depth. The independently
         * authored anchor resolves footprint ties.
         */
        [[nodiscard]] static RenderKey BuildKey(
            std::span<const World::TileCoordinate> footprint,
            World::TileCoordinate sortAnchor,
            World::LotSize lot,
            World::ViewRotation rotation,
            DrawLayer layer,
            int localOffset,
            std::uint64_t stableId);
    };
}
