#pragma once

#include <cstdint>
#include <vector>

#include "People/Objects/ObjectModel.hpp"
#include "People/World/LotGrid.hpp"

namespace People::Navigation
{
    /** @brief Stable four-neighbor order used by routing and deterministic ties. */
    enum class CardinalDirection : std::uint8_t
    {
        North = 0,
        East = 1,
        South = 2,
        West = 3
    };

    struct NavigationNeighbor
    {
        World::TileCoordinate tile;
        CardinalDirection direction = CardinalDirection::North;

        bool operator==(const NavigationNeighbor&) const = default;
    };

    /**
     * @brief Immutable renderer-independent routing snapshot for one lot floor.
     *
     * Physical object footprints block cells. Walls block cardinal edges unless
     * their attached door is open. Object clearance intentionally remains
     * traversable because it represents required approach space.
     */
    class StaticNavigationGrid final
    {
    public:
        [[nodiscard]] static StaticNavigationGrid Build(
            const World::LotGrid& lot,
            const Objects::ObjectWorld& objects,
            int floor);

        [[nodiscard]] World::LotSize Size() const noexcept;
        [[nodiscard]] int Floor() const noexcept;
        [[nodiscard]] bool Contains(World::TileCoordinate tile) const noexcept;
        [[nodiscard]] bool IsWalkable(World::TileCoordinate tile) const noexcept;
        [[nodiscard]] bool CanTraverse(
            World::TileCoordinate from,
            World::TileCoordinate to) const noexcept;

        /** @brief Returns passable neighbors in North, East, South, West order. */
        [[nodiscard]] std::vector<NavigationNeighbor> Neighbors(
            World::TileCoordinate tile) const;

    private:
        StaticNavigationGrid(World::LotSize size, int floor);

        [[nodiscard]] std::size_t CellIndex(World::TileCoordinate tile) const noexcept;
        [[nodiscard]] static std::uint8_t DirectionBit(
            CardinalDirection direction) noexcept;

        World::LotSize size_;
        int floor_ = 0;
        std::vector<std::uint8_t> walkable_;
        std::vector<std::uint8_t> blockedEdges_;
    };
}
