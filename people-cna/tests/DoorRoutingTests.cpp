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

    void TestClosedDoorDetourAndOpenShortcut()
    {
        LotGrid lot(4, 3);
        ObjectWorld objects(lot);
        const TileCoordinate start{1, 1, 0};
        const TileCoordinate goal{2, 1, 0};
        (void)lot.AddWall(start, TileEdge::MaxX);
        (void)lot.AddDoor(start, TileEdge::MaxX, false);

        const StaticNavigationGrid closedGrid = StaticNavigationGrid::Build(
            lot, objects, 0);
        const PathResult closedPath = AStarPathfinder::FindPath(
            closedGrid, start, goal);
        const std::vector<TileCoordinate> expectedDetour{
            {1, 1, 0}, {1, 0, 0}, {2, 0, 0}, {2, 1, 0}
        };
        Check(closedPath.Succeeded() && closedPath.tiles == expectedDetour,
              "closed door produces stable shortest detour around its wall");

        Check(lot.SetDoorOpen(start, TileEdge::MaxX, true), "door opens for shortcut");
        const StaticNavigationGrid openGrid = StaticNavigationGrid::Build(lot, objects, 0);
        const PathResult openPath = AStarPathfinder::FindPath(openGrid, start, goal);
        Check(openPath.Succeeded()
                  && openPath.tiles == std::vector<TileCoordinate>{start, goal},
              "open door makes its wall edge a direct route portal");
        Check(closedGrid.CanTraverse(start, goal) == false,
              "opening a door cannot mutate the previous closed snapshot");

        Check(lot.SetDoorOpen(start, TileEdge::MaxX, false), "door closes again");
        const StaticNavigationGrid reclosedGrid = StaticNavigationGrid::Build(
            lot, objects, 0);
        Check(AStarPathfinder::FindPath(reclosedGrid, start, goal).tiles == expectedDetour,
              "reclosing door restores the same deterministic detour");
    }

    void TestDoorControlsCompleteDivider()
    {
        LotGrid lot(5, 3);
        ObjectWorld objects(lot);
        for (int y = 0; y < 3; ++y)
            (void)lot.AddWall({1, y, 0}, TileEdge::MaxX);
        (void)lot.AddDoor({1, 1, 0}, TileEdge::MaxX, false);

        const TileCoordinate start{0, 1, 0};
        const TileCoordinate goal{4, 1, 0};
        const StaticNavigationGrid closedGrid = StaticNavigationGrid::Build(
            lot, objects, 0);
        const PathResult closedPath = AStarPathfinder::FindPath(
            closedGrid, start, goal);
        Check(closedPath.failure == PathFailure::NoPath && closedPath.tiles.empty(),
              "closed door in a complete divider makes the other side unreachable");

        (void)lot.SetDoorOpen({1, 1, 0}, TileEdge::MaxX, true);
        const StaticNavigationGrid openGrid = StaticNavigationGrid::Build(lot, objects, 0);
        const std::vector<TileCoordinate> expected{
            {0, 1, 0}, {1, 1, 0}, {2, 1, 0}, {3, 1, 0}, {4, 1, 0}
        };
        const PathResult openPath = AStarPathfinder::FindPath(openGrid, start, goal);
        Check(openPath.Succeeded() && openPath.tiles == expected,
              "open divider door yields the exact shortest portal route");
    }
}

int main()
{
    TestClosedDoorDetourAndOpenShortcut();
    TestDoorControlsCompleteDivider();

    if (failures != 0)
    {
        std::cerr << failures << " People door-routing test(s) failed\n";
        return 1;
    }
    std::cout << "All People door-routing tests passed\n";
    return 0;
}
