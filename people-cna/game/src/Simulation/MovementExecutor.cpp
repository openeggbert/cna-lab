#include "People/Simulation/MovementExecutor.hpp"

#include <stdexcept>
#include <utility>

#include "People/Navigation/AStarPathfinder.hpp"

namespace People::Simulation
{
    bool MovementStartResult::IsValid() const noexcept
    {
        return failure == MovementFailure::None;
    }

    bool MovementTickResult::IsValid() const noexcept
    {
        return failure == MovementFailure::None;
    }

    MovementExecutor::MovementExecutor(ResidentRegistry& residents) : residents_(residents)
    {
    }

    ResidentFacing MovementExecutor::FacingForStep(
        const World::TileCoordinate from,
        const World::TileCoordinate to)
    {
        if (from.floor != to.floor)
            throw std::invalid_argument("movement step cannot change floor without a portal");
        const int deltaX = to.x - from.x;
        const int deltaY = to.y - from.y;
        if (deltaX == 0 && deltaY == -1) return ResidentFacing::North;
        if (deltaX == 1 && deltaY == 0) return ResidentFacing::East;
        if (deltaX == 0 && deltaY == 1) return ResidentFacing::South;
        if (deltaX == -1 && deltaY == 0) return ResidentFacing::West;
        throw std::invalid_argument("movement path contains a non-cardinal step");
    }

    MovementStartResult MovementExecutor::Begin(
        const MovementRequestId requestId,
        const ResidentId residentId,
        std::vector<World::TileCoordinate> path,
        const Navigation::StaticNavigationGrid& grid)
    {
        if (requestId == 0)
            return {MovementFailure::InvalidRequestId, false};
        if (movements_.contains(requestId))
            return {MovementFailure::DuplicateRequestId, false};
        const ResidentState* resident = residents_.Find(residentId);
        if (resident == nullptr)
            return {MovementFailure::UnknownResident, false};
        if (resident->movementRequest.has_value())
            return {MovementFailure::ResidentBusy, false};
        if (path.empty())
            return {MovementFailure::EmptyPath, false};
        if (path.front() != resident->tile)
            return {MovementFailure::StartMismatch, false};
        if (!grid.IsWalkable(path.front()))
            return {MovementFailure::InvalidPath, false};
        for (std::size_t index = 1; index < path.size(); ++index)
        {
            if (!grid.CanTraverse(path[index - 1], path[index]))
                return {MovementFailure::InvalidPath, false};
        }
        if (path.size() == 1)
            return {MovementFailure::None, true};

        const ResidentMutationResult facing = residents_.SetFacing(
            residentId, FacingForStep(path[0], path[1]));
        if (!facing.IsValid())
            return {MovementFailure::ResidentMutationFailed, false};

        MovementState state{
            requestId,
            residentId,
            path.back(),
            std::move(path),
            1,
            0,
            0
        };
        movements_.emplace(requestId, std::move(state));
        const ResidentMutationResult attached = residents_.SetMovementRequest(
            residentId, requestId);
        if (!attached.IsValid())
        {
            movements_.erase(requestId);
            return {MovementFailure::ResidentMutationFailed, false};
        }
        return {};
    }

    MovementTickResult MovementExecutor::FailAndRemove(
        const std::map<MovementRequestId, MovementState>::iterator movement,
        const MovementFailure failure)
    {
        const ResidentId residentId = movement->second.residentId;
        World::TileCoordinate tile{};
        const ResidentState* resident = residents_.Find(residentId);
        if (resident != nullptr)
        {
            tile = resident->tile;
            if (resident->movementRequest == movement->first)
                (void)residents_.SetMovementRequest(residentId, std::nullopt);
        }
        movements_.erase(movement);
        return {MovementTickStatus::Failed, failure, residentId, tile};
    }

    MovementTickResult MovementExecutor::Advance(
        const MovementRequestId requestId,
        const Navigation::StaticNavigationGrid& grid)
    {
        auto movement = movements_.find(requestId);
        if (movement == movements_.end())
            return {MovementTickStatus::Failed, MovementFailure::UnknownRequest, 0, {}};
        ResidentState const* resident = residents_.Find(movement->second.residentId);
        if (resident == nullptr)
            return FailAndRemove(movement, MovementFailure::ResidentDeleted);
        if (resident->movementRequest != requestId)
            return FailAndRemove(movement, MovementFailure::ResidentDetached);

        MovementState& state = movement->second;
        if (state.nextTileIndex >= state.path.size())
            return FailAndRemove(movement, MovementFailure::InvalidPath);

        bool replanned = false;
        const World::TileCoordinate next = state.path[state.nextTileIndex];
        if (state.progressUnits == 0 && !grid.CanTraverse(resident->tile, next))
        {
            Navigation::PathResult replacement = Navigation::AStarPathfinder::FindPath(
                grid, resident->tile, state.destination);
            if (!replacement.Succeeded() || replacement.tiles.size() < 2)
                return FailAndRemove(movement, MovementFailure::NoReplanPath);
            state.path = std::move(replacement.tiles);
            state.nextTileIndex = 1;
            ++state.replanCount;
            const ResidentMutationResult facing = residents_.SetFacing(
                state.residentId, FacingForStep(state.path[0], state.path[1]));
            if (!facing.IsValid())
                return FailAndRemove(movement, MovementFailure::ResidentMutationFailed);
            resident = residents_.Find(state.residentId);
            replanned = true;
        }

        state.progressUnits = static_cast<std::uint16_t>(
            state.progressUnits + ProgressUnitsPerTick);
        state.travelledUnits += ProgressUnitsPerTick;
        if (state.progressUnits < ProgressUnitsPerTile)
        {
            return {
                replanned ? MovementTickStatus::Replanned : MovementTickStatus::Advanced,
                MovementFailure::None,
                state.residentId,
                resident->tile
            };
        }

        state.progressUnits = static_cast<std::uint16_t>(
            state.progressUnits - ProgressUnitsPerTile);
        const World::TileCoordinate arrived = state.path[state.nextTileIndex];
        const ResidentMutationResult moved = residents_.SetTile(state.residentId, arrived);
        if (!moved.IsValid())
            return FailAndRemove(movement, MovementFailure::ResidentMutationFailed);
        ++state.nextTileIndex;

        if (state.nextTileIndex == state.path.size())
        {
            const ResidentId residentId = state.residentId;
            const ResidentMutationResult detached = residents_.SetMovementRequest(
                residentId, std::nullopt);
            movements_.erase(movement);
            if (!detached.IsValid())
            {
                return {
                    MovementTickStatus::Failed,
                    MovementFailure::ResidentMutationFailed,
                    residentId,
                    arrived
                };
            }
            return {
                MovementTickStatus::Completed,
                MovementFailure::None,
                residentId,
                arrived
            };
        }

        const ResidentMutationResult facing = residents_.SetFacing(
            state.residentId,
            FacingForStep(state.path[state.nextTileIndex - 1],
                          state.path[state.nextTileIndex]));
        if (!facing.IsValid())
            return FailAndRemove(movement, MovementFailure::ResidentMutationFailed);
        return {
            MovementTickStatus::ArrivedTile,
            MovementFailure::None,
            state.residentId,
            arrived
        };
    }

    bool MovementExecutor::Cancel(const MovementRequestId requestId)
    {
        const auto found = movements_.find(requestId);
        if (found == movements_.end())
            return false;
        const ResidentState* resident = residents_.Find(found->second.residentId);
        if (resident != nullptr && resident->movementRequest == requestId)
            (void)residents_.SetMovementRequest(resident->id, std::nullopt);
        movements_.erase(found);
        return true;
    }

    const MovementState* MovementExecutor::Find(
        const MovementRequestId requestId) const noexcept
    {
        const auto found = movements_.find(requestId);
        return found == movements_.end() ? nullptr : &found->second;
    }

    std::optional<ResidentMovementProgress> MovementExecutor::ProgressFor(
        const ResidentId residentId) const noexcept
    {
        const ResidentState* resident = residents_.Find(residentId);
        if (resident == nullptr)
            return std::nullopt;
        if (!resident->movementRequest.has_value())
            return ResidentMovementProgress{false, 0};
        const MovementState* state = Find(*resident->movementRequest);
        if (state == nullptr)
            return ResidentMovementProgress{false, 0};
        return ResidentMovementProgress{true, state->travelledUnits};
    }

    std::optional<World::WorldPoint> MovementExecutor::PositionFor(
        const ResidentId residentId) const noexcept
    {
        const ResidentState* resident = residents_.Find(residentId);
        if (resident == nullptr)
            return std::nullopt;
        if (!resident->movementRequest.has_value())
        {
            return World::WorldPoint{
                static_cast<double>(resident->tile.x),
                static_cast<double>(resident->tile.y),
                resident->tile.floor
            };
        }
        const MovementState* state = Find(*resident->movementRequest);
        if (state == nullptr || state->nextTileIndex >= state->path.size())
            return std::nullopt;
        const World::TileCoordinate next = state->path[state->nextTileIndex];
        const double progress = static_cast<double>(state->progressUnits)
            / ProgressUnitsPerTile;
        return World::WorldPoint{
            static_cast<double>(resident->tile.x)
                + static_cast<double>(next.x - resident->tile.x) * progress,
            static_cast<double>(resident->tile.y)
                + static_cast<double>(next.y - resident->tile.y) * progress,
            resident->tile.floor
        };
    }

    const std::map<MovementRequestId, MovementState>& MovementExecutor::Active() const noexcept
    {
        return movements_;
    }
}
