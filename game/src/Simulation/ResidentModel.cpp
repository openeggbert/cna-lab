#include "People/Simulation/ResidentModel.hpp"

#include <utility>

namespace People::Simulation
{
    namespace
    {
        [[nodiscard]] bool IsValidFacing(const ResidentFacing facing)
        {
            switch (facing)
            {
                case ResidentFacing::North:
                case ResidentFacing::East:
                case ResidentFacing::South:
                case ResidentFacing::West: return true;
            }
            return false;
        }
    }

    bool ResidentMutationResult::IsValid() const noexcept
    {
        return failure == ResidentFailure::None;
    }

    bool ResidentRemovalResult::IsValid() const noexcept
    {
        return failure == ResidentFailure::None;
    }

    ResidentRegistry::ResidentRegistry(const World::LotGrid& lot) : lot_(lot)
    {
    }

    ResidentMutationResult ResidentRegistry::Add(ResidentState resident)
    {
        if (resident.id == 0)
            return {ResidentFailure::InvalidResidentId};
        if (resident.householdId == 0)
            return {ResidentFailure::InvalidHouseholdId};
        if (resident.displayName.empty())
            return {ResidentFailure::EmptyDisplayName};
        if (!lot_.Contains(resident.tile))
            return {ResidentFailure::OutsideLot};
        if (!IsValidFacing(resident.facing))
            return {ResidentFailure::InvalidFacing};
        if (resident.movementRequest == MovementRequestId{0})
            return {ResidentFailure::InvalidMovementRequestId};
        if (resident.activeAction == ActionId{0})
            return {ResidentFailure::InvalidActionId};
        if (residents_.contains(resident.id))
            return {ResidentFailure::DuplicateResidentId};

        residents_.emplace(resident.id, std::move(resident));
        return {};
    }

    ResidentRemovalResult ResidentRegistry::Remove(const ResidentId id)
    {
        const auto found = residents_.find(id);
        if (found == residents_.end())
        {
            return {
                ResidentFailure::UnknownResident,
                id,
                0,
                std::nullopt,
                std::nullopt
            };
        }

        ResidentRemovalResult result{
            ResidentFailure::None,
            found->second.id,
            found->second.householdId,
            found->second.movementRequest,
            found->second.activeAction
        };
        residents_.erase(found);
        return result;
    }

    const ResidentState* ResidentRegistry::Find(const ResidentId id) const noexcept
    {
        const auto found = residents_.find(id);
        return found == residents_.end() ? nullptr : &found->second;
    }

    const std::map<ResidentId, ResidentState>& ResidentRegistry::Residents() const noexcept
    {
        return residents_;
    }

    ResidentMutationResult ResidentRegistry::SetTile(
        const ResidentId id, const World::TileCoordinate tile)
    {
        const auto found = residents_.find(id);
        if (found == residents_.end())
            return {ResidentFailure::UnknownResident};
        if (!lot_.Contains(tile))
            return {ResidentFailure::OutsideLot};
        found->second.tile = tile;
        return {};
    }

    ResidentMutationResult ResidentRegistry::SetMovementRequest(
        const ResidentId id, const std::optional<MovementRequestId> requestId)
    {
        const auto found = residents_.find(id);
        if (found == residents_.end())
            return {ResidentFailure::UnknownResident};
        if (requestId == MovementRequestId{0})
            return {ResidentFailure::InvalidMovementRequestId};
        found->second.movementRequest = requestId;
        return {};
    }

    ResidentMutationResult ResidentRegistry::SetFacing(
        const ResidentId id, const ResidentFacing facing)
    {
        const auto found = residents_.find(id);
        if (found == residents_.end())
            return {ResidentFailure::UnknownResident};
        if (!IsValidFacing(facing))
            return {ResidentFailure::InvalidFacing};
        found->second.facing = facing;
        return {};
    }

    ResidentMutationResult ResidentRegistry::SetActiveAction(
        const ResidentId id, const std::optional<ActionId> actionId)
    {
        const auto found = residents_.find(id);
        if (found == residents_.end())
            return {ResidentFailure::UnknownResident};
        if (actionId == ActionId{0})
            return {ResidentFailure::InvalidActionId};
        found->second.activeAction = actionId;
        return {};
    }
}
