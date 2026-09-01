#include "People/Navigation/InteractionRoutePlanner.hpp"

#include <iostream>
#include <string>
#include <utility>
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

    ObjectDefinition MakeTarget(
        const std::string& id,
        const FootprintOffset approach,
        std::vector<FootprintOffset> clearance = {})
    {
        ObjectDefinition definition{
            id, "Route Target", ObjectCategory::Miscellaneous,
            1, {{0, 0}}, {}, 0x0F, {}, {}
        };
        definition.interactionSlots.push_back({
            "use", approach, SlotFacing::North, SlotPosture::Standing, 1,
            std::move(clearance)
        });
        return definition;
    }

    ObjectDefinition MakeBlocker()
    {
        return {
            "people.route.blocker", "Route Blocker", ObjectCategory::Miscellaneous,
            1, {{0, 0}}, {}, 0x0F, {}, {}
        };
    }

    void TestRoutesToRotatedSlotInsteadOfObjectAnchor()
    {
        LotGrid lot(8, 8);
        ObjectWorld objects(lot);
        (void)objects.RegisterDefinition(MakeTarget("people.route.target", {0, 1}));
        (void)objects.Place({10, "people.route.target", {4, 3, 0}, ObjectRotation::East});
        const StaticNavigationGrid grid = StaticNavigationGrid::Build(lot, objects, 0);

        const InteractionRouteResult result = InteractionRoutePlanner::Plan(
            grid, objects, {1, 3, 0}, 10, "use");
        Check(result.Succeeded(), "known reachable interaction slot produces route");
        Check(result.slot.approachTile == TileCoordinate{3, 3, 0}
                  && result.slot.approachTile != TileCoordinate{4, 3, 0},
              "route target is rotated authored approach, not object anchor");
        Check(result.slot.facing == SlotFacing::East,
              "route result preserves rotated required facing");
        Check(result.path == std::vector<TileCoordinate>{
                  {1, 3, 0}, {2, 3, 0}, {3, 3, 0}},
              "route ends exactly at interaction approach tile");
        Check(result.expandedNodes > 0, "successful interaction route exposes A* work");
    }

    void TestIdentityAndEndpointFailures()
    {
        LotGrid lot(5, 5);
        ObjectWorld objects(lot);
        (void)objects.RegisterDefinition(MakeTarget("people.route.target", {1, 0}));
        (void)objects.RegisterDefinition(MakeBlocker());
        (void)objects.Place({10, "people.route.target", {2, 2, 0}, ObjectRotation::North});
        (void)objects.Place({20, "people.route.blocker", {3, 2, 0}, ObjectRotation::North});
        const StaticNavigationGrid grid = StaticNavigationGrid::Build(lot, objects, 0);

        Check(InteractionRoutePlanner::Plan(grid, objects, {0, 0, 0}, 999, "use").failure
                  == InteractionRouteFailure::UnknownObject,
              "unknown target object has exact route failure");
        Check(InteractionRoutePlanner::Plan(grid, objects, {0, 0, 0}, 10, "missing").failure
                  == InteractionRouteFailure::UnknownSlot,
              "unknown target slot has exact route failure");
        Check(InteractionRoutePlanner::Plan(grid, objects, {0, 0, 1}, 10, "use").failure
                  == InteractionRouteFailure::SlotBlocked,
              "slot validation precedes start validation deterministically");
        Check(InteractionRoutePlanner::Plan(grid, objects, {-1, 0, 0}, 10, "use").failure
                  == InteractionRouteFailure::SlotBlocked,
              "blocked slot remains the primary actionable failure");

        Check(objects.Remove(20), "blocking object removes");
        const StaticNavigationGrid clearGrid = StaticNavigationGrid::Build(lot, objects, 0);
        Check(InteractionRoutePlanner::Plan(clearGrid, objects, {-1, 0, 0}, 10, "use").failure
                  == InteractionRouteFailure::StartOutsideGrid,
              "outside route start has exact failure after valid slot");
        (void)objects.Place({21, "people.route.blocker", {0, 0, 0}, ObjectRotation::North});
        const StaticNavigationGrid blockedStartGrid = StaticNavigationGrid::Build(
            lot, objects, 0);
        Check(InteractionRoutePlanner::Plan(
                  blockedStartGrid, objects, {0, 0, 0}, 10, "use").failure
                  == InteractionRouteFailure::StartBlocked,
              "blocked route start has exact failure after valid slot");
    }

    void TestSlotAndClearanceFailures()
    {
        {
            LotGrid lot(3, 3);
            ObjectWorld objects(lot);
            (void)objects.RegisterDefinition(MakeTarget("people.route.outside", {1, 0}));
            (void)objects.Place({1, "people.route.outside", {2, 1, 0}, ObjectRotation::North});
            const StaticNavigationGrid grid = StaticNavigationGrid::Build(lot, objects, 0);
            Check(InteractionRoutePlanner::Plan(grid, objects, {0, 0, 0}, 1, "use").failure
                      == InteractionRouteFailure::SlotOutsideGrid,
                  "out-of-lot approach has exact failure");
        }
        {
            LotGrid lot(4, 4);
            ObjectWorld objects(lot);
            (void)objects.RegisterDefinition(MakeTarget(
                "people.route.clearance", {1, 0}, {{0, 1}}));
            (void)objects.RegisterDefinition(MakeBlocker());
            (void)objects.Place({1, "people.route.clearance", {1, 1, 0},
                                 ObjectRotation::North});
            (void)objects.Place({2, "people.route.blocker", {2, 2, 0},
                                 ObjectRotation::North});
            const StaticNavigationGrid grid = StaticNavigationGrid::Build(lot, objects, 0);
            const InteractionRouteResult result = InteractionRoutePlanner::Plan(
                grid, objects, {0, 0, 0}, 1, "use");
            Check(result.failure == InteractionRouteFailure::ClearanceBlocked,
                  "occupied slot clearance has exact failure");
            Check(result.slot.approachTile == TileCoordinate{2, 1, 0},
                  "clearance failure still reports resolved interaction target");
        }
        {
            LotGrid lot(3, 3);
            ObjectWorld objects(lot);
            (void)objects.RegisterDefinition(MakeTarget(
                "people.route.clearance_outside", {1, 0}, {{1, 0}}));
            (void)objects.Place({1, "people.route.clearance_outside", {1, 1, 0},
                                 ObjectRotation::North});
            const StaticNavigationGrid grid = StaticNavigationGrid::Build(lot, objects, 0);
            Check(InteractionRoutePlanner::Plan(grid, objects, {0, 0, 0}, 1, "use").failure
                      == InteractionRouteFailure::ClearanceOutsideGrid,
                  "out-of-lot slot clearance has exact failure");
        }
    }

    void TestNoPathToValidSlot()
    {
        LotGrid lot(4, 3);
        ObjectWorld objects(lot);
        (void)objects.RegisterDefinition(MakeTarget("people.route.target", {-1, 0}));
        (void)objects.Place({1, "people.route.target", {3, 1, 0}, ObjectRotation::North});
        for (int y = 0; y < 3; ++y)
            (void)lot.AddWall({1, y, 0}, TileEdge::MaxX);
        const StaticNavigationGrid grid = StaticNavigationGrid::Build(lot, objects, 0);
        const InteractionRouteResult result = InteractionRoutePlanner::Plan(
            grid, objects, {0, 1, 0}, 1, "use");
        Check(result.failure == InteractionRouteFailure::NoPath && result.path.empty(),
              "valid but unreachable approach reports no path without partial route");
        Check(result.slot.approachTile == TileCoordinate{2, 1, 0},
              "no-path result retains its resolved slot target");
    }
}

int main()
{
    TestRoutesToRotatedSlotInsteadOfObjectAnchor();
    TestIdentityAndEndpointFailures();
    TestSlotAndClearanceFailures();
    TestNoPathToValidSlot();

    if (failures != 0)
    {
        std::cerr << failures << " People interaction-route test(s) failed\n";
        return 1;
    }
    std::cout << "All People interaction-route tests passed\n";
    return 0;
}
