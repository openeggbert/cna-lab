#include "People/World/LotGrid.hpp"
#include "People/World/RoomMap.hpp"

#include <exception>
#include <functional>
#include <iostream>
#include <string>

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

    void AddPerimeter(LotGrid& lot, const int floor = 0)
    {
        const LotSize size = lot.Size();
        for (int x = 0; x < size.width; ++x)
        {
            (void)lot.AddWall({x, 0, floor}, TileEdge::MinY);
            (void)lot.AddWall({x, size.height - 1, floor}, TileEdge::MaxY);
        }
        for (int y = 0; y < size.height; ++y)
        {
            (void)lot.AddWall({0, y, floor}, TileEdge::MinX);
            (void)lot.AddWall({size.width - 1, y, floor}, TileEdge::MaxX);
        }
    }

    void TestBoundsAndFloorStorage()
    {
        LotGrid lot(3, 2, 2);
        Check(lot.Size() == LotSize{3, 2}, "lot reports bounded width and height");
        Check(lot.FloorCount() == 2, "lot reports floor count");
        Check(lot.Contains({0, 0, 0}) && lot.Contains({2, 1, 1}),
              "lot contains inclusive minimum and maximum valid cells");
        Check(!lot.Contains({3, 1, 0}) && !lot.Contains({0, 0, 2}),
              "lot rejects cells beyond spatial and floor bounds");

        Check(lot.FloorAt({1, 1, 0}).terrain == TerrainKind::Grass,
              "new floor defaults to grass terrain");
        Check(lot.IsFloorVisualDirty({1, 1, 0}), "new floor starts visually dirty");
        lot.ClearFloorVisualDirty({1, 1, 0});
        Check(!lot.IsFloorVisualDirty({1, 1, 0}), "floor dirtiness can be acknowledged");
        Check(lot.SetTerrain({1, 1, 0}, TerrainKind::Soil),
              "changing terrain reports mutation");
        Check(lot.IsFloorVisualDirty({1, 1, 0}), "terrain mutation dirties presentation");
        lot.ClearFloorVisualDirty({1, 1, 0});
        Check(!lot.SetTerrain({1, 1, 0}, TerrainKind::Soil),
              "setting identical terrain is a no-op");
        Check(!lot.IsFloorVisualDirty({1, 1, 0}), "no-op terrain does not dirty presentation");

        Check(lot.SetFloorCovering({1, 1, 1}, std::string("people.floor.warm_oak")),
              "floor covering stores a stable content ID");
        Check(lot.FloorAt({1, 1, 1}).floorCoveringId
                  == std::optional<std::string>("people.floor.warm_oak"),
              "covering state is independent on upper floor");
        Check(!lot.FloorAt({1, 1, 0}).floorCoveringId.has_value(),
              "covering does not leak between floors");
        Check(lot.SetFloorCovering({1, 1, 1}, std::nullopt),
              "covering can be removed");

        CheckThrows([] { (void)LotGrid(0, 2); }, "zero lot dimension is rejected");
        CheckThrows([&] { (void)lot.FloorAt({-1, 0, 0}); }, "out-of-bounds floor read throws");
        CheckThrows([&] {
            (void)lot.SetFloorCovering({0, 0, 0}, std::string());
        }, "empty floor-covering ID is rejected");
    }

    void TestCanonicalWalls()
    {
        LotGrid lot(3, 2, 2);
        const WallEdge fromLeft = lot.CanonicalWall({0, 0, 0}, TileEdge::MaxX);
        const WallEdge fromRight = lot.CanonicalWall({1, 0, 0}, TileEdge::MinX);
        Check(fromLeft == fromRight, "neighbor tile sides normalize to one wall edge");

        Check(lot.AddWall({0, 0, 0}, TileEdge::MaxX), "new wall insertion succeeds");
        Check(!lot.AddWall({1, 0, 0}, TileEdge::MinX),
              "duplicate wall through neighbor side is rejected as no-op");
        Check(lot.HasWall({0, 0, 0}, TileEdge::MaxX)
                  && lot.HasWall({1, 0, 0}, TileEdge::MinX),
              "wall can be queried from either adjacent tile");
        Check(lot.Walls().size() == 1, "canonical wall is stored once");
        Check(!lot.HasWall({0, 0, 1}, TileEdge::MaxX), "walls are isolated by floor");

        lot.AcknowledgeRoomsRebuilt(0);
        Check(!lot.AddWall({0, 0, 0}, TileEdge::MaxX),
              "duplicate insertion remains a no-op after room rebuild");
        Check(!lot.RoomsDirty(0), "duplicate insertion does not dirty rooms");
        Check(lot.RemoveWall({1, 0, 0}, TileEdge::MinX),
              "wall removal works through equivalent neighbor side");
        Check(lot.RoomsDirty(0), "topology mutation dirties room map");
        Check(!lot.RemoveWall({1, 0, 0}, TileEdge::MinX),
              "missing wall removal is a no-op");

        Check(lot.AddWall({0, 0, 0}, TileEdge::MinY), "minimum boundary wall is valid");
        Check(lot.AddWall({2, 1, 0}, TileEdge::MaxX), "maximum boundary wall is valid");
        CheckThrows([&] {
            (void)lot.CanonicalWall({3, 0, 0}, TileEdge::MinX);
        }, "wall request from outside tile is rejected");
    }

    void TestOutsideAndEnclosedRoom()
    {
        LotGrid openLot(3, 3);
        RoomMap open = RoomMap::Rebuild(openLot, 0);
        Check(open.EnclosedRoomCount() == 0, "unwalled lot has no enclosed room");
        for (int y = 0; y < 3; ++y)
            for (int x = 0; x < 3; ++x)
                Check(open.RoomAt({x, y, 0}) == OutsideRoom,
                      "unwalled component is assigned outside");
        Check(!openLot.RoomsDirty(0), "successful room rebuild acknowledges dirty flag");

        (void)openLot.AddWall({1, 1, 0}, TileEdge::MinY);
        (void)openLot.AddWall({1, 1, 0}, TileEdge::MaxX);
        (void)openLot.AddWall({1, 1, 0}, TileEdge::MaxY);
        (void)openLot.AddWall({1, 1, 0}, TileEdge::MinX);
        RoomMap enclosed = RoomMap::Rebuild(openLot, 0);
        Check(enclosed.EnclosedRoomCount() == 1, "four walls enclose center room");
        Check(enclosed.RoomAt({1, 1, 0}) == 1, "enclosed center receives first room ID");
        Check(enclosed.RoomAt({0, 0, 0}) == OutsideRoom,
              "surrounding open cells remain outside");
    }

    void TestRoomSplitAndMerge()
    {
        LotGrid lot(4, 2);
        AddPerimeter(lot);
        (void)lot.AddWall({1, 0, 0}, TileEdge::MaxX);
        (void)lot.AddWall({1, 1, 0}, TileEdge::MaxX);

        RoomMap split = RoomMap::Rebuild(lot, 0);
        Check(split.EnclosedRoomCount() == 2, "complete divider splits enclosed lot");
        Check(split.RoomAt({0, 0, 0}) == 1 && split.RoomAt({1, 1, 0}) == 1,
              "left component receives stable first ID");
        Check(split.RoomAt({2, 0, 0}) == 2 && split.RoomAt({3, 1, 0}) == 2,
              "right component receives stable second ID");

        Check(lot.RemoveWall({2, 0, 0}, TileEdge::MinX),
              "opening one divider segment mutates topology");
        RoomMap merged = RoomMap::Rebuild(lot, 0);
        Check(merged.EnclosedRoomCount() == 1, "open divider merges room components");
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 4; ++x)
                Check(merged.RoomAt({x, y, 0}) == 1, "merged room has deterministic first ID");
    }

    void TestFloorIsolationAndValidation()
    {
        LotGrid lot(2, 2, 2);
        AddPerimeter(lot, 1);
        RoomMap lower = RoomMap::Rebuild(lot, 0);
        RoomMap upper = RoomMap::Rebuild(lot, 1);
        Check(lower.EnclosedRoomCount() == 0, "lower open floor remains outside");
        Check(upper.EnclosedRoomCount() == 1, "upper perimeter creates independent room");
        CheckThrows([&] { (void)upper.RoomAt({0, 0, 0}); },
                    "room map rejects coordinate from another floor");
        CheckThrows([&] { (void)RoomMap::Rebuild(lot, 2); },
                    "room rebuild rejects invalid floor");
    }
}

int main()
{
    TestBoundsAndFloorStorage();
    TestCanonicalWalls();
    TestOutsideAndEnclosedRoom();
    TestRoomSplitAndMerge();
    TestFloorIsolationAndValidation();

    if (failures != 0)
    {
        std::cerr << failures << " People lot-grid test(s) failed\n";
        return 1;
    }
    std::cout << "All People lot-grid tests passed\n";
    return 0;
}
