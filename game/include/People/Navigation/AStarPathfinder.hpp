#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "People/Navigation/StaticNavigationGrid.hpp"

namespace People::Navigation
{
    enum class PathFailure : std::uint8_t
    {
        None,
        StartOutsideGrid,
        GoalOutsideGrid,
        StartBlocked,
        GoalBlocked,
        NoPath
    };

    struct PathResult
    {
        PathFailure failure = PathFailure::None;
        std::vector<World::TileCoordinate> tiles;
        std::size_t expandedNodes = 0;

        [[nodiscard]] bool Succeeded() const noexcept;
    };

    /** @brief Deterministic unit-cost A* over a static four-neighbor snapshot. */
    class AStarPathfinder final
    {
    public:
        /**
         * @brief Finds a shortest path containing both start and goal.
         *
         * Equal-cost choices preserve the navigation grid's North, East,
         * South, West neighbor order through a monotonically increasing queue
         * sequence number.
         */
        [[nodiscard]] static PathResult FindPath(
            const StaticNavigationGrid& grid,
            World::TileCoordinate start,
            World::TileCoordinate goal);
    };
}
