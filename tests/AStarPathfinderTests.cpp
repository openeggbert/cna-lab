#include "People/Navigation/AStarPathfinder.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace People::Navigation;
using namespace People::Objects;
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

    ObjectDefinition MakeSingleTileBlocker()
    {
        return {
            "people.navigation.single_blocker", "Single Blocker",
            ObjectCategory::Miscellaneous, 1, {{0, 0}}, {}, 0x0F, {}, {}
        };
    }

    void RegisterBlocker(ObjectWorld& objects)
    {
        Check(objects.RegisterDefinition(MakeSingleTileBlocker()),
              "test blocker definition registers");
    }

    void PlaceBlocker(
        ObjectWorld& objects,
        const ObjectInstanceId id,
        const int x,
        const int y)
    {
        Check(objects.Place({id, "people.navigation.single_blocker", {x, y, 0},
                             ObjectRotation::North}).IsValid(),
              "test blocker instance places");
    }

    void CheckPathTraversable(
        const StaticNavigationGrid& grid,
        const PathResult& result,
        const std::string& message)
    {
        bool traversable = result.Succeeded() && !result.tiles.empty();
        for (std::size_t index = 1; traversable && index < result.tiles.size(); ++index)
            traversable = grid.CanTraverse(result.tiles[index - 1], result.tiles[index]);
        Check(traversable, message);
    }

    void TestStartEqualsGoalAndInputFailures()
    {
        LotGrid lot(3, 3);
        ObjectWorld objects(lot);
        RegisterBlocker(objects);
        PlaceBlocker(objects, 1, 1, 1);
        const StaticNavigationGrid grid = StaticNavigationGrid::Build(lot, objects, 0);

        const PathResult same = AStarPathfinder::FindPath(grid, {0, 0, 0}, {0, 0, 0});
        Check(same.Succeeded() && same.tiles == std::vector<TileCoordinate>{{0, 0, 0}}
                  && same.expandedNodes == 0,
              "start equal to goal returns one tile without search");

        Check(AStarPathfinder::FindPath(grid, {-1, 0, 0}, {0, 0, 0}).failure
                  == PathFailure::StartOutsideGrid,
              "outside start returns exact failure");
        Check(AStarPathfinder::FindPath(grid, {0, 0, 0}, {3, 0, 0}).failure
                  == PathFailure::GoalOutsideGrid,
              "outside goal returns exact failure");
        Check(AStarPathfinder::FindPath(grid, {1, 1, 0}, {0, 0, 0}).failure
                  == PathFailure::StartBlocked,
              "blocked start returns exact failure");
        Check(AStarPathfinder::FindPath(grid, {0, 0, 0}, {1, 1, 0}).failure
                  == PathFailure::GoalBlocked,
              "blocked goal returns exact failure");
    }

    void TestShortestOpenPathAndStableTie()
    {
        LotGrid lot(3, 3);
        ObjectWorld objects(lot);
        const StaticNavigationGrid grid = StaticNavigationGrid::Build(lot, objects, 0);
        const std::vector<TileCoordinate> expected{
            {0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {2, 1, 0}, {2, 2, 0}
        };

        for (int repetition = 0; repetition < 32; ++repetition)
        {
            const PathResult result = AStarPathfinder::FindPath(
                grid, {0, 0, 0}, {2, 2, 0});
            Check(result.Succeeded() && result.tiles == expected,
                  "equal-cost open route uses the same stable tie on every run");
            Check(result.tiles.size() == 5,
                  "open route has Manhattan-optimal tile count");
            CheckPathTraversable(grid, result, "open route contains only passable edges");
        }
    }

    void TestObstacleDetourAndNoPath()
    {
        LotGrid detourLot(5, 3);
        ObjectWorld detourObjects(detourLot);
        RegisterBlocker(detourObjects);
        PlaceBlocker(detourObjects, 10, 2, 0);
        PlaceBlocker(detourObjects, 11, 2, 1);
        const StaticNavigationGrid detourGrid = StaticNavigationGrid::Build(
            detourLot, detourObjects, 0);

        const PathResult detour = AStarPathfinder::FindPath(
            detourGrid, {0, 1, 0}, {4, 1, 0});
        const std::vector<TileCoordinate> expected{
            {0, 1, 0}, {1, 1, 0}, {1, 2, 0}, {2, 2, 0},
            {3, 2, 0}, {3, 1, 0}, {4, 1, 0}
        };
        Check(detour.Succeeded() && detour.tiles == expected,
              "A* takes the deterministic shortest route around a fixed obstacle");
        Check(detour.tiles.size() == 7, "detour length is shortest for the fixed map");
        CheckPathTraversable(detourGrid, detour,
                             "detour route contains only passable edges");
        Check(detour.expandedNodes > 0, "successful search reports expanded nodes");

        LotGrid blockedLot(3, 3);
        ObjectWorld blockedObjects(blockedLot);
        RegisterBlocker(blockedObjects);
        PlaceBlocker(blockedObjects, 20, 0, 1);
        PlaceBlocker(blockedObjects, 21, 1, 1);
        PlaceBlocker(blockedObjects, 22, 2, 1);
        const StaticNavigationGrid blockedGrid = StaticNavigationGrid::Build(
            blockedLot, blockedObjects, 0);
        const PathResult noPath = AStarPathfinder::FindPath(
            blockedGrid, {1, 0, 0}, {1, 2, 0});
        Check(noPath.failure == PathFailure::NoPath && noPath.tiles.empty(),
              "complete obstacle barrier returns no path and no partial route");
        Check(noPath.expandedNodes == 3,
              "no-path search deterministically expands the reachable component");
    }
}

int main()
{
    TestStartEqualsGoalAndInputFailures();
    TestShortestOpenPathAndStableTie();
    TestObstacleDetourAndNoPath();

    if (failures != 0)
    {
        std::cerr << failures << " People A* test(s) failed\n";
        return 1;
    }
    std::cout << "All People A* tests passed\n";
    return 0;
}
