#include "People/Navigation/InteractionRoutePlanner.hpp"

#include <stdexcept>
#include <utility>

namespace People::Navigation
{
    namespace
    {
        [[nodiscard]] InteractionRouteFailure MapPathFailure(const PathFailure failure)
        {
            switch (failure)
            {
                case PathFailure::None: return InteractionRouteFailure::None;
                case PathFailure::StartOutsideGrid:
                    return InteractionRouteFailure::StartOutsideGrid;
                case PathFailure::StartBlocked: return InteractionRouteFailure::StartBlocked;
                case PathFailure::NoPath: return InteractionRouteFailure::NoPath;
                case PathFailure::GoalOutsideGrid:
                case PathFailure::GoalBlocked:
                    throw std::logic_error(
                        "validated interaction slot produced an invalid A* goal");
            }
            throw std::logic_error("A* returned an invalid path failure");
        }
    }

    bool InteractionRouteResult::Succeeded() const noexcept
    {
        return failure == InteractionRouteFailure::None;
    }

    InteractionRouteResult InteractionRoutePlanner::Plan(
        const StaticNavigationGrid& grid,
        const Objects::ObjectWorld& objects,
        const World::TileCoordinate start,
        const Objects::ObjectInstanceId objectId,
        const std::string_view slotId)
    {
        Objects::SlotResolutionResult resolved = objects.ResolveInteractionSlot(
            objectId, slotId);
        if (resolved.failure == Objects::SlotResolutionFailure::UnknownInstance)
            return {InteractionRouteFailure::UnknownObject, {}, {}, 0};
        if (resolved.failure == Objects::SlotResolutionFailure::UnknownSlot)
            return {InteractionRouteFailure::UnknownSlot, {}, {}, 0};
        if (!resolved.IsValid())
            throw std::logic_error("slot resolution returned an invalid failure");

        if (!grid.Contains(resolved.slot.approachTile))
        {
            return {
                InteractionRouteFailure::SlotOutsideGrid,
                std::move(resolved.slot),
                {},
                0
            };
        }
        if (!grid.IsWalkable(resolved.slot.approachTile))
        {
            return {
                InteractionRouteFailure::SlotBlocked,
                std::move(resolved.slot),
                {},
                0
            };
        }
        for (const World::TileCoordinate clearance : resolved.slot.clearanceTiles)
        {
            if (!grid.Contains(clearance))
            {
                return {
                    InteractionRouteFailure::ClearanceOutsideGrid,
                    std::move(resolved.slot),
                    {},
                    0
                };
            }
            if (!grid.IsWalkable(clearance))
            {
                return {
                    InteractionRouteFailure::ClearanceBlocked,
                    std::move(resolved.slot),
                    {},
                    0
                };
            }
        }

        PathResult path = AStarPathfinder::FindPath(
            grid, start, resolved.slot.approachTile);
        const InteractionRouteFailure failure = MapPathFailure(path.failure);
        return {
            failure,
            std::move(resolved.slot),
            std::move(path.tiles),
            path.expandedNodes
        };
    }
}
