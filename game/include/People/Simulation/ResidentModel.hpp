#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include "People/World/LotGrid.hpp"

namespace People::Simulation
{
    using ResidentId = std::uint64_t;
    using HouseholdId = std::uint64_t;
    using MovementRequestId = std::uint64_t;
    using ActionId = std::uint64_t;

    enum class ResidentFacing : std::uint8_t
    {
        North = 0,
        East = 1,
        South = 2,
        West = 3
    };

    /** @brief Persistent resident identity and current simulation references. */
    struct ResidentState
    {
        ResidentId id = 0;
        HouseholdId householdId = 0;
        std::string displayName;
        World::TileCoordinate tile;
        ResidentFacing facing = ResidentFacing::South;
        std::optional<MovementRequestId> movementRequest;
        std::optional<ActionId> activeAction;
    };

    enum class ResidentFailure : std::uint8_t
    {
        None,
        InvalidResidentId,
        InvalidHouseholdId,
        EmptyDisplayName,
        OutsideLot,
        InvalidFacing,
        DuplicateResidentId,
        UnknownResident,
        InvalidMovementRequestId,
        InvalidActionId
    };

    struct ResidentMutationResult
    {
        ResidentFailure failure = ResidentFailure::None;

        [[nodiscard]] bool IsValid() const noexcept;
    };

    /** @brief Cleanup information returned before a resident identity disappears. */
    struct ResidentRemovalResult
    {
        ResidentFailure failure = ResidentFailure::None;
        ResidentId residentId = 0;
        HouseholdId householdId = 0;
        std::optional<MovementRequestId> movementRequest;
        std::optional<ActionId> activeAction;

        [[nodiscard]] bool IsValid() const noexcept;
    };

    /** @brief Renderer-independent resident registry for one active lot. */
    class ResidentRegistry final
    {
    public:
        explicit ResidentRegistry(const World::LotGrid& lot);

        [[nodiscard]] ResidentMutationResult Add(ResidentState resident);
        [[nodiscard]] ResidentRemovalResult Remove(ResidentId id);

        [[nodiscard]] const ResidentState* Find(ResidentId id) const noexcept;
        [[nodiscard]] const std::map<ResidentId, ResidentState>& Residents() const noexcept;

        [[nodiscard]] ResidentMutationResult SetTile(
            ResidentId id, World::TileCoordinate tile);
        [[nodiscard]] ResidentMutationResult SetFacing(
            ResidentId id, ResidentFacing facing);
        [[nodiscard]] ResidentMutationResult SetMovementRequest(
            ResidentId id, std::optional<MovementRequestId> requestId);
        [[nodiscard]] ResidentMutationResult SetActiveAction(
            ResidentId id, std::optional<ActionId> actionId);

    private:
        const World::LotGrid& lot_;
        std::map<ResidentId, ResidentState> residents_;
    };
}
