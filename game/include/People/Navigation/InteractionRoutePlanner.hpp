#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "People/Navigation/AStarPathfinder.hpp"
#include "People/Objects/ObjectModel.hpp"

namespace People::Navigation
{
    enum class InteractionRouteFailure : std::uint8_t
    {
        None,
        UnknownObject,
        UnknownSlot,
        StartOutsideGrid,
        StartBlocked,
        SlotOutsideGrid,
        SlotBlocked,
        ClearanceOutsideGrid,
        ClearanceBlocked,
        NoPath
    };

    struct InteractionRouteResult
    {
        InteractionRouteFailure failure = InteractionRouteFailure::None;
        Objects::ResolvedInteractionSlot slot;
        std::vector<World::TileCoordinate> path;
        std::size_t expandedNodes = 0;

        [[nodiscard]] bool Succeeded() const noexcept;
    };

    /** @brief Resolves and routes to an authored object interaction approach. */
    class InteractionRoutePlanner final
    {
    public:
        [[nodiscard]] static InteractionRouteResult Plan(
            const StaticNavigationGrid& grid,
            const Objects::ObjectWorld& objects,
            World::TileCoordinate start,
            Objects::ObjectInstanceId objectId,
            std::string_view slotId);
    };
}
