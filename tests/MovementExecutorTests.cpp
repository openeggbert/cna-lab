#include "People/Navigation/AStarPathfinder.hpp"
#include "People/Simulation/MovementExecutor.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

using namespace People::Navigation;
using namespace People::Objects;
using namespace People::Simulation;
using namespace People::World;

namespace
{
    int failures = 0;

    void Check(const bool condition, const std::string& message)
    {
        if (condition)
            return;
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }

    bool Near(const double left, const double right)
    {
        return std::abs(left - right) < 0.000001;
    }

    ResidentState MakeResident(const TileCoordinate tile = {0, 1, 0})
    {
        return {
            100, 1, "Movement Tester", tile, ResidentFacing::South,
            std::nullopt, std::nullopt
        };
    }

    void TestFixedTickProgressAndExactArrival()
    {
        LotGrid lot(4, 3);
        ObjectWorld objects(lot);
        ResidentRegistry residents(lot);
        (void)residents.Add(MakeResident());
        MovementExecutor movement(residents);
        const StaticNavigationGrid grid = StaticNavigationGrid::Build(lot, objects, 0);
        const PathResult path = AStarPathfinder::FindPath(grid, {0, 1, 0}, {2, 1, 0});

        const MovementStartResult started = movement.Begin(500, 100, path.tiles, grid);
        Check(started.IsValid() && !started.completedImmediately,
              "multi-tile path starts fixed-tick movement");
        Check(residents.Find(100)->movementRequest == std::optional<MovementRequestId>(500),
              "active resident references movement request");
        Check(residents.Find(100)->facing == ResidentFacing::East,
              "movement immediately faces first cardinal step");

        for (int tick = 0; tick < 4; ++tick)
            (void)movement.Advance(500, grid);
        const std::optional<WorldPoint> halfway = movement.PositionFor(100);
        Check(halfway.has_value() && Near(halfway->x, 0.5) && Near(halfway->y, 1.0),
              "four fixed ticks reach exact half-tile presentation position");
        Check(residents.Find(100)->tile == TileCoordinate{0, 1, 0},
              "logical resident tile changes only at segment arrival");

        MovementTickResult arrived;
        for (int tick = 4; tick < 8; ++tick)
            arrived = movement.Advance(500, grid);
        Check(arrived.status == MovementTickStatus::ArrivedTile
                  && residents.Find(100)->tile == TileCoordinate{1, 1, 0},
              "eighth fixed tick arrives exactly on first tile");
        Check(movement.Find(500) != nullptr && movement.Find(500)->progressUnits == 0,
              "segment arrival has no floating-point progress drift");

        MovementTickResult completed;
        for (int tick = 0; tick < 8; ++tick)
            completed = movement.Advance(500, grid);
        Check(completed.status == MovementTickStatus::Completed
                  && completed.tile == TileCoordinate{2, 1, 0},
              "second segment completes on exact destination");
        Check(residents.Find(100)->tile == TileCoordinate{2, 1, 0}
                  && !residents.Find(100)->movementRequest.has_value(),
              "completion commits tile and detaches movement request");
        Check(movement.Find(500) == nullptr && movement.Active().empty(),
              "completed movement leaves no hidden active state");
        const std::optional<WorldPoint> finalPosition = movement.PositionFor(100);
        Check(finalPosition.has_value() && Near(finalPosition->x, 2.0)
                  && Near(finalPosition->y, 1.0),
              "completed presentation position equals logical destination exactly");
    }

    void TestDeterministicObstructionReplan()
    {
        LotGrid lot(4, 3);
        ObjectWorld objects(lot);
        ResidentRegistry residents(lot);
        (void)residents.Add(MakeResident());
        MovementExecutor movement(residents);
        StaticNavigationGrid grid = StaticNavigationGrid::Build(lot, objects, 0);
        const PathResult initial = AStarPathfinder::FindPath(grid, {0, 1, 0}, {3, 1, 0});
        (void)movement.Begin(600, 100, initial.tiles, grid);
        for (int tick = 0; tick < 8; ++tick)
            (void)movement.Advance(600, grid);
        Check(residents.Find(100)->tile == TileCoordinate{1, 1, 0},
              "resident reaches obstruction setup tile");

        (void)lot.AddWall({1, 1, 0}, TileEdge::MaxX);
        grid = StaticNavigationGrid::Build(lot, objects, 0);
        const MovementTickResult replanned = movement.Advance(600, grid);
        const MovementState* state = movement.Find(600);
        Check(replanned.status == MovementTickStatus::Replanned
                  && state != nullptr && state->replanCount == 1,
              "new static obstruction triggers one deterministic replan");
        Check(state != nullptr && state->path == std::vector<TileCoordinate>{
                  {1, 1, 0}, {1, 0, 0}, {2, 0, 0}, {3, 0, 0}, {3, 1, 0}},
              "replan replaces remaining route with stable shortest detour");
        Check(residents.Find(100)->facing == ResidentFacing::North,
              "replan updates simulation facing to new first step");

        MovementTickResult result = replanned;
        int safetyTicks = 0;
        while (result.status != MovementTickStatus::Completed && safetyTicks < 40)
        {
            result = movement.Advance(600, grid);
            ++safetyTicks;
        }
        Check(result.status == MovementTickStatus::Completed
                  && residents.Find(100)->tile == TileCoordinate{3, 1, 0},
              "replanned movement reaches original destination exactly");
        Check(residents.Find(100)->facing == ResidentFacing::South,
              "final segment updates facing toward destination");
    }

    void TestTravelledUnitsFeedWalkAnimation()
    {
        LotGrid lot(4, 3);
        ObjectWorld objects(lot);
        ResidentRegistry residents(lot);
        (void)residents.Add(MakeResident());
        MovementExecutor movement(residents);
        StaticNavigationGrid grid = StaticNavigationGrid::Build(lot, objects, 0);

        Check(movement.ProgressFor(100)
                  == std::optional<ResidentMovementProgress>({false, 0}),
              "a resident with no route reports a still walk phase");
        Check(!movement.ProgressFor(999).has_value(),
              "an unknown resident has no inspectable movement progress");

        const PathResult initial = AStarPathfinder::FindPath(grid, {0, 1, 0}, {3, 1, 0});
        (void)movement.Begin(700, 100, initial.tiles, grid);
        Check(movement.ProgressFor(100)
                  == std::optional<ResidentMovementProgress>({true, 0}),
              "a route starts at the beginning of the walk cycle");

        for (int tick = 1; tick <= 8; ++tick)
        {
            (void)movement.Advance(700, grid);
            const std::optional<ResidentMovementProgress> progress =
                movement.ProgressFor(100);
            Check(progress.has_value()
                      && progress->travelledUnits
                          == static_cast<std::uint32_t>(tick)
                              * MovementExecutor::ProgressUnitsPerTick,
                  "travelled units advance by exactly one tick of movement");
        }

        const MovementState* before = movement.Find(700);
        const std::uint32_t beforeReplan = before->travelledUnits;
        Check(beforeReplan == MovementExecutor::ProgressUnitsPerTile,
              "one traversed tile equals one full walk cycle of travelled units");

        (void)lot.AddWall({1, 1, 0}, TileEdge::MaxX);
        grid = StaticNavigationGrid::Build(lot, objects, 0);
        const MovementTickResult replanned = movement.Advance(700, grid);
        const MovementState* after = movement.Find(700);
        Check(replanned.status == MovementTickStatus::Replanned
                  && after != nullptr && after->nextTileIndex == 1,
              "the replan rebuilt the remaining path");
        Check(after != nullptr
                  && after->travelledUnits
                      == beforeReplan + MovementExecutor::ProgressUnitsPerTick,
              "a replan never rewinds the walk cycle");

        // Reading progress is what a renderer does every frame; it must not
        // advance, complete, or otherwise mutate the route.
        const std::vector<TileCoordinate> pathBefore = after->path;
        const std::uint16_t progressBefore = after->progressUnits;
        for (int read = 0; read < 5; ++read)
        {
            (void)movement.ProgressFor(100);
            (void)movement.PositionFor(100);
        }
        const MovementState* unchanged = movement.Find(700);
        Check(unchanged != nullptr
                  && unchanged->path == pathBefore
                  && unchanged->progressUnits == progressBefore
                  && unchanged->travelledUnits
                      == beforeReplan + MovementExecutor::ProgressUnitsPerTick,
              "presentation reads leave every movement field untouched");

        MovementTickResult result = replanned;
        int safetyTicks = 0;
        while (result.status != MovementTickStatus::Completed && safetyTicks < 40)
        {
            result = movement.Advance(700, grid);
            ++safetyTicks;
        }
        Check(result.status == MovementTickStatus::Completed,
              "the walk-animation route still completes");
        Check(movement.ProgressFor(100)
                  == std::optional<ResidentMovementProgress>({false, 0}),
              "an arrived resident reports a still walk phase again");
    }

    void TestNoReplanPathAndCancellation()
    {
        LotGrid lot(3, 1);
        ObjectWorld objects(lot);
        ResidentRegistry residents(lot);
        (void)residents.Add(MakeResident({0, 0, 0}));
        MovementExecutor movement(residents);
        StaticNavigationGrid grid = StaticNavigationGrid::Build(lot, objects, 0);
        const PathResult initial = AStarPathfinder::FindPath(grid, {0, 0, 0}, {2, 0, 0});
        (void)movement.Begin(700, 100, initial.tiles, grid);
        for (int tick = 0; tick < 8; ++tick)
            (void)movement.Advance(700, grid);
        (void)lot.AddWall({1, 0, 0}, TileEdge::MaxX);
        grid = StaticNavigationGrid::Build(lot, objects, 0);
        const MovementTickResult failed = movement.Advance(700, grid);
        Check(failed.status == MovementTickStatus::Failed
                  && failed.failure == MovementFailure::NoReplanPath,
              "unavoidable new obstruction produces exact replan failure");
        Check(movement.Find(700) == nullptr
                  && !residents.Find(100)->movementRequest.has_value(),
              "replan failure removes movement and resident reference");

        const PathResult back = AStarPathfinder::FindPath(grid, {1, 0, 0}, {0, 0, 0});
        Check(movement.Begin(701, 100, back.tiles, grid).IsValid(),
              "new request can start after failed route cleanup");
        Check(movement.Cancel(701) && !movement.Cancel(701),
              "cancellation succeeds once and missing request is explicit no-op");
        Check(!residents.Find(100)->movementRequest.has_value(),
              "cancellation detaches resident movement reference");
    }

    void TestStartValidationAndDeletionSafety()
    {
        LotGrid lot(3, 3);
        ObjectWorld objects(lot);
        ResidentRegistry residents(lot);
        (void)residents.Add(MakeResident({0, 0, 0}));
        MovementExecutor movement(residents);
        const StaticNavigationGrid grid = StaticNavigationGrid::Build(lot, objects, 0);

        Check(movement.Begin(0, 100, {{0, 0, 0}}, grid).failure
                  == MovementFailure::InvalidRequestId,
              "zero movement request ID is rejected");
        Check(movement.Begin(1, 999, {{0, 0, 0}}, grid).failure
                  == MovementFailure::UnknownResident,
              "unknown resident is rejected");
        Check(movement.Begin(1, 100, {}, grid).failure == MovementFailure::EmptyPath,
              "empty movement path is rejected");
        Check(movement.Begin(1, 100, {{1, 0, 0}}, grid).failure
                  == MovementFailure::StartMismatch,
              "path must start at resident logical tile");
        Check(movement.Begin(1, 100, {{0, 0, 0}, {1, 1, 0}}, grid).failure
                  == MovementFailure::InvalidPath,
              "non-cardinal path is rejected through navigation graph");
        Check(movement.Begin(1, 100, {{0, 0, 0}}, grid).completedImmediately,
              "one-tile path completes without attaching movement state");

        Check(movement.Begin(2, 100, {{0, 0, 0}, {1, 0, 0}}, grid).IsValid(),
              "valid deletion fixture movement starts");
        Check(movement.Begin(2, 100, {{0, 0, 0}, {1, 0, 0}}, grid).failure
                  == MovementFailure::DuplicateRequestId,
              "duplicate live request ID is rejected before resident-busy check");
        Check(movement.Begin(3, 100, {{0, 0, 0}, {0, 1, 0}}, grid).failure
                  == MovementFailure::ResidentBusy,
              "resident cannot own two movements");
        (void)residents.Remove(100);
        const MovementTickResult deleted = movement.Advance(2, grid);
        Check(deleted.failure == MovementFailure::ResidentDeleted
                  && movement.Active().empty(),
              "resident deletion converts orphan movement to safe terminal failure");
        Check(!movement.PositionFor(100).has_value(),
              "deleted resident has no movement presentation position");
        Check(movement.Advance(999, grid).failure == MovementFailure::UnknownRequest,
              "unknown advance request has exact failure");
    }
}

int main()
{
    TestFixedTickProgressAndExactArrival();
    TestDeterministicObstructionReplan();
    TestTravelledUnitsFeedWalkAnimation();
    TestNoReplanPathAndCancellation();
    TestStartValidationAndDeletionSafety();

    if (failures != 0)
    {
        std::cerr << failures << " People movement-executor test(s) failed\n";
        return 1;
    }
    std::cout << "All People movement-executor tests passed\n";
    return 0;
}
