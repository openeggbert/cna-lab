#include "People/Rendering/WallPresentation.hpp"

#include <utility>

namespace People::Rendering
{
    WallRenderDescriptor WallPresentation::Describe(
        const World::LotGrid& lot,
        const World::WallEdge wall,
        const World::ViewRotation rotation)
    {
        const World::WorldPoint first{
            static_cast<double>(wall.x) - 0.5,
            static_cast<double>(wall.y) - 0.5,
            wall.floor
        };
        const World::WorldPoint second = wall.axis == World::WallAxis::AlongX
            ? World::WorldPoint{static_cast<double>(wall.x) + 0.5,
                                static_cast<double>(wall.y) - 0.5, wall.floor}
            : World::WorldPoint{static_cast<double>(wall.x) - 0.5,
                                static_cast<double>(wall.y) + 0.5, wall.floor};

        std::vector<World::TileCoordinate> footprint = lot.AdjacentTiles(wall);
        World::TileCoordinate anchor = footprint.front();
        for (const World::TileCoordinate candidate : footprint)
        {
            const World::ViewCoordinate candidateView = World::IsometricProjection::Rotate(
                candidate, lot.Size(), rotation);
            const World::ViewCoordinate anchorView = World::IsometricProjection::Rotate(
                anchor, lot.Size(), rotation);
            if (candidateView.x + candidateView.y > anchorView.x + anchorView.y
                || (candidateView.x + candidateView.y == anchorView.x + anchorView.y
                    && (candidateView.y > anchorView.y
                        || (candidateView.y == anchorView.y
                            && candidateView.x > anchorView.x))))
                anchor = candidate;
        }

        DrawLayer layer = DrawLayer::WallFront;
        if (footprint.size() == 1)
        {
            const World::WorldPoint midpoint{
                (first.x + second.x) * 0.5,
                (first.y + second.y) * 0.5,
                wall.floor
            };
            const World::Camera camera{{0.0, 0.0}, 1.0, rotation};
            const World::PixelPoint wallCenter = World::IsometricProjection::WorldPointToScreen(
                midpoint, lot.Size(), camera);
            const World::PixelPoint tileCenter = World::IsometricProjection::WorldToScreen(
                footprint.front(), lot.Size(), camera);
            layer = wallCenter.y < tileCenter.y ? DrawLayer::WallBack : DrawLayer::WallFront;
        }

        return {{first, second}, std::move(footprint), anchor, layer};
    }
}
