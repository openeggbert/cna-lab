#include "People/Navigation/StaticNavigationGrid.hpp"

#include <array>
#include <stdexcept>

namespace People::Navigation
{
    namespace
    {
        struct DirectionDefinition
        {
            CardinalDirection direction;
            int deltaX;
            int deltaY;
            World::TileEdge edge;
        };

        constexpr std::array<DirectionDefinition, 4> Directions{{
            {CardinalDirection::North, 0, -1, World::TileEdge::MinY},
            {CardinalDirection::East, 1, 0, World::TileEdge::MaxX},
            {CardinalDirection::South, 0, 1, World::TileEdge::MaxY},
            {CardinalDirection::West, -1, 0, World::TileEdge::MinX}
        }};

        [[nodiscard]] const DirectionDefinition* FindDirection(
            const World::TileCoordinate from,
            const World::TileCoordinate to) noexcept
        {
            const int deltaX = to.x - from.x;
            const int deltaY = to.y - from.y;
            for (const DirectionDefinition& direction : Directions)
            {
                if (direction.deltaX == deltaX && direction.deltaY == deltaY)
                    return &direction;
            }
            return nullptr;
        }
    }

    StaticNavigationGrid::StaticNavigationGrid(const World::LotSize size, const int floor)
        : size_(size),
          floor_(floor),
          walkable_(static_cast<std::size_t>(size.width)
                    * static_cast<std::size_t>(size.height), 1U),
          blockedEdges_(walkable_.size(), 0U)
    {
    }

    StaticNavigationGrid StaticNavigationGrid::Build(
        const World::LotGrid& lot,
        const Objects::ObjectWorld& objects,
        const int floor)
    {
        if (floor < 0 || floor >= lot.FloorCount())
            throw std::out_of_range("navigation floor is outside the lot");

        StaticNavigationGrid result(lot.Size(), floor);
        for (int y = 0; y < result.size_.height; ++y)
        {
            for (int x = 0; x < result.size_.width; ++x)
            {
                const World::TileCoordinate tile{x, y, floor};
                const std::size_t index = result.CellIndex(tile);
                result.walkable_[index] = objects.OccupiedBy(tile).has_value() ? 0U : 1U;

                std::uint8_t blocked = 0U;
                for (const DirectionDefinition& direction : Directions)
                {
                    if (lot.WallBlocksRouting(tile, direction.edge))
                    {
                        blocked = static_cast<std::uint8_t>(
                            blocked | DirectionBit(direction.direction));
                    }
                }
                result.blockedEdges_[index] = blocked;
            }
        }
        return result;
    }

    World::LotSize StaticNavigationGrid::Size() const noexcept
    {
        return size_;
    }

    int StaticNavigationGrid::Floor() const noexcept
    {
        return floor_;
    }

    bool StaticNavigationGrid::Contains(const World::TileCoordinate tile) const noexcept
    {
        return tile.floor == floor_ && tile.x >= 0 && tile.y >= 0
            && tile.x < size_.width && tile.y < size_.height;
    }

    std::size_t StaticNavigationGrid::CellIndex(
        const World::TileCoordinate tile) const noexcept
    {
        return static_cast<std::size_t>(tile.y) * static_cast<std::size_t>(size_.width)
            + static_cast<std::size_t>(tile.x);
    }

    std::uint8_t StaticNavigationGrid::DirectionBit(
        const CardinalDirection direction) noexcept
    {
        return static_cast<std::uint8_t>(1U << static_cast<std::uint8_t>(direction));
    }

    bool StaticNavigationGrid::IsWalkable(const World::TileCoordinate tile) const noexcept
    {
        return Contains(tile) && walkable_[CellIndex(tile)] != 0U;
    }

    bool StaticNavigationGrid::CanTraverse(
        const World::TileCoordinate from,
        const World::TileCoordinate to) const noexcept
    {
        if (!IsWalkable(from) || !IsWalkable(to) || from.floor != to.floor)
            return false;
        const DirectionDefinition* direction = FindDirection(from, to);
        if (direction == nullptr)
            return false;
        return (blockedEdges_[CellIndex(from)] & DirectionBit(direction->direction)) == 0U;
    }

    std::vector<NavigationNeighbor> StaticNavigationGrid::Neighbors(
        const World::TileCoordinate tile) const
    {
        std::vector<NavigationNeighbor> result;
        result.reserve(Directions.size());
        for (const DirectionDefinition& direction : Directions)
        {
            const World::TileCoordinate neighbor{
                tile.x + direction.deltaX,
                tile.y + direction.deltaY,
                tile.floor
            };
            if (CanTraverse(tile, neighbor))
                result.push_back({neighbor, direction.direction});
        }
        return result;
    }
}
