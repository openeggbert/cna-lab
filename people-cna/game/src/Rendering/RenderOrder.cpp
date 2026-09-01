#include "People/Rendering/RenderOrder.hpp"

#include <algorithm>
#include <stdexcept>

namespace People::Rendering
{
    RenderKey RenderOrder::BuildKey(
        const std::span<const World::TileCoordinate> footprint,
        const World::TileCoordinate sortAnchor,
        const World::LotSize lot,
        const World::ViewRotation rotation,
        const DrawLayer layer,
        const int localOffset,
        const std::uint64_t stableId)
    {
        if (footprint.empty())
            throw std::invalid_argument("render footprint must not be empty");

        const World::ViewCoordinate anchor = World::IsometricProjection::Rotate(
            sortAnchor, lot, rotation);
        int footprintDepth = anchor.x + anchor.y;
        for (const World::TileCoordinate tile : footprint)
        {
            if (tile.floor != sortAnchor.floor)
                throw std::invalid_argument("render footprint must share the sort-anchor floor");
            const World::ViewCoordinate view = World::IsometricProjection::Rotate(
                tile, lot, rotation);
            footprintDepth = std::max(footprintDepth, view.x + view.y);
        }

        return {
            sortAnchor.floor,
            footprintDepth,
            layer,
            anchor.y,
            anchor.x,
            localOffset,
            stableId
        };
    }
}
