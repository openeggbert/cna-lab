#include "People/Navigation/AStarPathfinder.hpp"

#include <algorithm>
#include <limits>
#include <queue>
#include <utility>

namespace People::Navigation
{
    namespace
    {
        constexpr std::size_t NoIndex = std::numeric_limits<std::size_t>::max();
        constexpr std::size_t InfiniteCost = std::numeric_limits<std::size_t>::max();

        struct OpenNode
        {
            std::size_t index;
            std::size_t pathCost;
            std::size_t heuristic;
            std::size_t sequence;
        };

        struct OpenNodeLater
        {
            [[nodiscard]] bool operator()(const OpenNode& left, const OpenNode& right) const
            {
                const std::size_t leftTotal = left.pathCost + left.heuristic;
                const std::size_t rightTotal = right.pathCost + right.heuristic;
                if (leftTotal != rightTotal)
                    return leftTotal > rightTotal;
                if (left.heuristic != right.heuristic)
                    return left.heuristic > right.heuristic;
                return left.sequence > right.sequence;
            }
        };

        [[nodiscard]] std::size_t CellIndex(
            const World::TileCoordinate tile,
            const World::LotSize size) noexcept
        {
            return static_cast<std::size_t>(tile.y) * static_cast<std::size_t>(size.width)
                + static_cast<std::size_t>(tile.x);
        }

        [[nodiscard]] World::TileCoordinate TileAt(
            const std::size_t index,
            const World::LotSize size,
            const int floor) noexcept
        {
            const std::size_t width = static_cast<std::size_t>(size.width);
            return {
                static_cast<int>(index % width),
                static_cast<int>(index / width),
                floor
            };
        }

        [[nodiscard]] std::size_t ManhattanDistance(
            const World::TileCoordinate left,
            const World::TileCoordinate right) noexcept
        {
            const int deltaX = left.x >= right.x ? left.x - right.x : right.x - left.x;
            const int deltaY = left.y >= right.y ? left.y - right.y : right.y - left.y;
            return static_cast<std::size_t>(deltaX) + static_cast<std::size_t>(deltaY);
        }
    }

    bool PathResult::Succeeded() const noexcept
    {
        return failure == PathFailure::None;
    }

    PathResult AStarPathfinder::FindPath(
        const StaticNavigationGrid& grid,
        const World::TileCoordinate start,
        const World::TileCoordinate goal)
    {
        if (!grid.Contains(start))
            return {PathFailure::StartOutsideGrid, {}, 0};
        if (!grid.Contains(goal))
            return {PathFailure::GoalOutsideGrid, {}, 0};
        if (!grid.IsWalkable(start))
            return {PathFailure::StartBlocked, {}, 0};
        if (!grid.IsWalkable(goal))
            return {PathFailure::GoalBlocked, {}, 0};
        if (start == goal)
            return {PathFailure::None, {start}, 0};

        const World::LotSize size = grid.Size();
        const std::size_t cellCount = static_cast<std::size_t>(size.width)
            * static_cast<std::size_t>(size.height);
        const std::size_t startIndex = CellIndex(start, size);
        const std::size_t goalIndex = CellIndex(goal, size);

        std::vector<std::size_t> bestCost(cellCount, InfiniteCost);
        std::vector<std::size_t> predecessor(cellCount, NoIndex);
        std::vector<std::uint8_t> closed(cellCount, 0U);
        std::priority_queue<OpenNode, std::vector<OpenNode>, OpenNodeLater> open;

        std::size_t nextSequence = 0;
        bestCost[startIndex] = 0;
        open.push({startIndex, 0, ManhattanDistance(start, goal), nextSequence++});

        std::size_t expandedNodes = 0;
        while (!open.empty())
        {
            const OpenNode current = open.top();
            open.pop();
            if (closed[current.index] != 0U || current.pathCost != bestCost[current.index])
                continue;

            closed[current.index] = 1U;
            ++expandedNodes;
            if (current.index == goalIndex)
            {
                std::vector<World::TileCoordinate> path;
                for (std::size_t index = goalIndex; index != NoIndex;
                     index = predecessor[index])
                {
                    path.push_back(TileAt(index, size, grid.Floor()));
                }
                std::reverse(path.begin(), path.end());
                return {PathFailure::None, std::move(path), expandedNodes};
            }

            const World::TileCoordinate currentTile = TileAt(
                current.index, size, grid.Floor());
            for (const NavigationNeighbor& neighbor : grid.Neighbors(currentTile))
            {
                const std::size_t neighborIndex = CellIndex(neighbor.tile, size);
                if (closed[neighborIndex] != 0U)
                    continue;
                const std::size_t candidateCost = current.pathCost + 1U;
                if (candidateCost >= bestCost[neighborIndex])
                    continue;

                bestCost[neighborIndex] = candidateCost;
                predecessor[neighborIndex] = current.index;
                open.push({
                    neighborIndex,
                    candidateCost,
                    ManhattanDistance(neighbor.tile, goal),
                    nextSequence++
                });
            }
        }

        return {PathFailure::NoPath, {}, expandedNodes};
    }
}
