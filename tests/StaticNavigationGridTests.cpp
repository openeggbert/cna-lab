#include "People/Navigation/StaticNavigationGrid.hpp"

#include <exception>
#include <functional>
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

    void CheckThrows(const std::function<void()>& operation, const std::string& message)
    {
        try
        {
            operation();
            Check(false, message);
        }
        catch (const std::exception&)
        {
        }
    }

    ObjectDefinition MakeBlocker()
    {
        return {
            "people.navigation.blocker", "Navigation Blocker", ObjectCategory::Miscellaneous,
            1, {{0, 0}, {1, 0}}, {{0, 1}}, 0x0F, {}
        };
    }

    void TestEmptyGridAndStableNeighborOrder()
    {
        LotGrid lot(3, 3, 2);
        ObjectWorld objects(lot);
        const StaticNavigationGrid grid = StaticNavigationGrid::Build(lot, objects, 1);

        Check(grid.Size() == LotSize{3, 3} && grid.Floor() == 1,
              "snapshot records its lot dimensions and floor");
        Check(grid.IsWalkable({1, 1, 1}), "empty in-floor tile is walkable");
        Check(!grid.IsWalkable({1, 1, 0}) && !grid.IsWalkable({3, 1, 1}),
              "other-floor and out-of-bounds tiles are not in the snapshot");

        const std::vector<NavigationNeighbor> center = grid.Neighbors({1, 1, 1});
        const std::vector<NavigationNeighbor> expected{
            {{1, 0, 1}, CardinalDirection::North},
            {{2, 1, 1}, CardinalDirection::East},
            {{1, 2, 1}, CardinalDirection::South},
            {{0, 1, 1}, CardinalDirection::West}
        };
        Check(center == expected, "neighbors use stable North-East-South-West order");
        Check(grid.Neighbors({0, 0, 1}) == std::vector<NavigationNeighbor>{
                  {{1, 0, 1}, CardinalDirection::East},
                  {{0, 1, 1}, CardinalDirection::South}},
              "lot bounds trim neighbors without changing relative order");
        Check(!grid.CanTraverse({0, 0, 1}, {1, 1, 1}),
              "diagonal traversal is rejected");
        Check(!grid.CanTraverse({0, 0, 1}, {2, 0, 1}),
              "non-adjacent traversal is rejected");
        CheckThrows([&] { (void)StaticNavigationGrid::Build(lot, objects, 2); },
                    "invalid floor snapshot is rejected");
    }

    void TestObjectFootprintsAndClearance()
    {
        LotGrid lot(4, 3);
        ObjectWorld objects(lot);
        (void)objects.RegisterDefinition(MakeBlocker());
        Check(objects.Place({11, "people.navigation.blocker", {1, 1, 0},
                             ObjectRotation::North}).IsValid(),
              "test blocker places");

        const StaticNavigationGrid grid = StaticNavigationGrid::Build(lot, objects, 0);
        Check(!grid.IsWalkable({1, 1, 0}) && !grid.IsWalkable({2, 1, 0}),
              "every physical footprint cell blocks navigation");
        Check(grid.IsWalkable({1, 2, 0}),
              "authored clearance remains traversable approach space");
        Check(!grid.CanTraverse({0, 1, 0}, {1, 1, 0}),
              "routes cannot enter an occupied footprint");
        Check(!grid.CanTraverse({1, 1, 0}, {0, 1, 0}),
              "routes cannot leave an occupied footprint");

        Check(objects.Remove(11), "test blocker removes");
        Check(!grid.IsWalkable({1, 1, 0}),
              "existing snapshot remains immutable after object removal");
        const StaticNavigationGrid rebuilt = StaticNavigationGrid::Build(lot, objects, 0);
        Check(rebuilt.IsWalkable({1, 1, 0}) && rebuilt.IsWalkable({2, 1, 0}),
              "rebuilt snapshot observes released footprint cells");
    }

    void TestWallsAndDoorSnapshots()
    {
        LotGrid lot(3, 2);
        ObjectWorld objects(lot);
        const TileCoordinate left{0, 0, 0};
        const TileCoordinate right{1, 0, 0};
        (void)lot.AddWall(left, TileEdge::MaxX);

        const StaticNavigationGrid wallGrid = StaticNavigationGrid::Build(lot, objects, 0);
        Check(!wallGrid.CanTraverse(left, right) && !wallGrid.CanTraverse(right, left),
              "canonical wall blocks traversal from both adjacent tiles");
        Check(lot.AddDoor(left, TileEdge::MaxX, false), "closed test door attaches");
        const StaticNavigationGrid closedGrid = StaticNavigationGrid::Build(lot, objects, 0);
        Check(!closedGrid.CanTraverse(left, right), "closed door remains a blocked portal");

        Check(lot.SetDoorOpen(left, TileEdge::MaxX, true), "test door opens");
        Check(!closedGrid.CanTraverse(left, right),
              "door mutation does not alter an existing routing snapshot");
        const StaticNavigationGrid openGrid = StaticNavigationGrid::Build(lot, objects, 0);
        Check(openGrid.CanTraverse(left, right) && openGrid.CanTraverse(right, left),
              "rebuilt snapshot exposes an open door in both directions");
        Check(openGrid.Neighbors(left) == std::vector<NavigationNeighbor>{
                  {{1, 0, 0}, CardinalDirection::East},
                  {{0, 1, 0}, CardinalDirection::South}},
              "open portal participates in stable neighbor ordering");
    }
}

int main()
{
    TestEmptyGridAndStableNeighborOrder();
    TestObjectFootprintsAndClearance();
    TestWallsAndDoorSnapshots();

    if (failures != 0)
    {
        std::cerr << failures << " People static-navigation test(s) failed\n";
        return 1;
    }
    std::cout << "All People static-navigation tests passed\n";
    return 0;
}
