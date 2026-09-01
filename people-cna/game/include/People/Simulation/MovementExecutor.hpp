#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "People/Navigation/StaticNavigationGrid.hpp"
#include "People/Simulation/ResidentModel.hpp"

namespace People::Simulation
{
    enum class MovementFailure : std::uint8_t
    {
        None,
        InvalidRequestId,
        DuplicateRequestId,
        UnknownResident,
        ResidentBusy,
        EmptyPath,
        StartMismatch,
        InvalidPath,
        UnknownRequest,
        ResidentDeleted,
        ResidentDetached,
        NoReplanPath,
        ResidentMutationFailed
    };

    struct MovementStartResult
    {
        MovementFailure failure = MovementFailure::None;
        bool completedImmediately = false;

        [[nodiscard]] bool IsValid() const noexcept;
    };

    enum class MovementTickStatus : std::uint8_t
    {
        Advanced,
        ArrivedTile,
        Replanned,
        Completed,
        Failed
    };

    struct MovementTickResult
    {
        MovementTickStatus status = MovementTickStatus::Failed;
        MovementFailure failure = MovementFailure::None;
        ResidentId residentId = 0;
        World::TileCoordinate tile;

        [[nodiscard]] bool IsValid() const noexcept;
    };

    /** @brief Inspectable fixed-point progress for one active route. */
    struct MovementState
    {
        MovementRequestId id = 0;
        ResidentId residentId = 0;
        World::TileCoordinate destination;
        std::vector<World::TileCoordinate> path;
        std::size_t nextTileIndex = 1;
        std::uint16_t progressUnits = 0;
        std::uint32_t replanCount = 0;

        /**
         * @brief Movement units travelled since this route began.
         *
         * Monotone for the whole route: a replan rebuilds the path but never
         * rewinds this counter, so a presentation walk cycle derived from it
         * stays continuous instead of snapping back at a replan boundary.
         */
        std::uint32_t travelledUnits = 0;
    };

    /** @brief Read-only movement facts a renderer may consume for one resident. */
    struct ResidentMovementProgress
    {
        bool moving = false;
        std::uint32_t travelledUnits = 0;

        bool operator==(const ResidentMovementProgress&) const = default;
    };

    /** @brief Deterministic fixed-tick resident movement independent of rendering. */
    class MovementExecutor final
    {
    public:
        static constexpr std::uint16_t ProgressUnitsPerTile = 1000;
        static constexpr std::uint16_t ProgressUnitsPerTick = 125;

        explicit MovementExecutor(ResidentRegistry& residents);

        [[nodiscard]] MovementStartResult Begin(
            MovementRequestId requestId,
            ResidentId residentId,
            std::vector<World::TileCoordinate> path,
            const Navigation::StaticNavigationGrid& grid);
        [[nodiscard]] MovementTickResult Advance(
            MovementRequestId requestId,
            const Navigation::StaticNavigationGrid& grid);
        [[nodiscard]] bool Cancel(MovementRequestId requestId);

        [[nodiscard]] const MovementState* Find(MovementRequestId requestId) const noexcept;
        [[nodiscard]] std::optional<World::WorldPoint> PositionFor(
            ResidentId residentId) const noexcept;
        [[nodiscard]] std::optional<ResidentMovementProgress> ProgressFor(
            ResidentId residentId) const noexcept;
        [[nodiscard]] const std::map<MovementRequestId, MovementState>& Active() const noexcept;

    private:
        [[nodiscard]] static ResidentFacing FacingForStep(
            World::TileCoordinate from,
            World::TileCoordinate to);
        [[nodiscard]] MovementTickResult FailAndRemove(
            std::map<MovementRequestId, MovementState>::iterator movement,
            MovementFailure failure);

        ResidentRegistry& residents_;
        std::map<MovementRequestId, MovementState> movements_;
    };
}
