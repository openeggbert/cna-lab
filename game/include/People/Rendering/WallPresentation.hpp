#pragma once

#include <array>
#include <vector>

#include "People/Rendering/RenderOrder.hpp"
#include "People/World/LotGrid.hpp"

namespace People::Rendering
{
    /** @brief View-derived data for one canonical logical wall segment. */
    struct WallRenderDescriptor
    {
        std::array<World::WorldPoint, 2> endpoints;
        std::vector<World::TileCoordinate> footprint;
        World::TileCoordinate sortAnchor;
        DrawLayer layer = DrawLayer::WallFront;
    };

    /** @brief Pure wall endpoint, anchor, footprint, and coarse-layer derivation. */
    class WallPresentation final
    {
    public:
        [[nodiscard]] static WallRenderDescriptor Describe(
            const World::LotGrid& lot,
            World::WallEdge wall,
            World::ViewRotation rotation);
    };
}
