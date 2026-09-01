#include "People/World/LotGrid.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace People::World
{
    namespace
    {
        [[nodiscard]] std::size_t CheckedCellCount(
            const int width, const int height, const int floorCount)
        {
            if (width <= 0 || height <= 0 || floorCount <= 0)
                throw std::invalid_argument("lot dimensions and floor count must be positive");

            const auto w = static_cast<std::size_t>(width);
            const auto h = static_cast<std::size_t>(height);
            const auto f = static_cast<std::size_t>(floorCount);
            const std::size_t maximum = std::numeric_limits<std::size_t>::max();
            if (w > maximum / h || w * h > maximum / f)
                throw std::length_error("lot cell count overflows addressable storage");
            return w * h * f;
        }
    }

    LotGrid::LotGrid(const int width, const int height, const int floorCount)
        : width_(width),
          height_(height),
          floorCount_(floorCount),
          floors_(CheckedCellCount(width, height, floorCount)),
          roomsDirty_(static_cast<std::size_t>(floorCount), true),
          routingDirty_(static_cast<std::size_t>(floorCount), true)
    {
    }

    LotSize LotGrid::Size() const noexcept
    {
        return {width_, height_};
    }

    int LotGrid::FloorCount() const noexcept
    {
        return floorCount_;
    }

    bool LotGrid::Contains(const TileCoordinate tile) const noexcept
    {
        return tile.x >= 0 && tile.y >= 0 && tile.floor >= 0
            && tile.x < width_ && tile.y < height_ && tile.floor < floorCount_;
    }

    std::size_t LotGrid::CellIndex(const TileCoordinate tile) const
    {
        if (!Contains(tile))
            throw std::out_of_range("tile is outside the lot");
        return (static_cast<std::size_t>(tile.floor) * static_cast<std::size_t>(height_)
                + static_cast<std::size_t>(tile.y)) * static_cast<std::size_t>(width_)
            + static_cast<std::size_t>(tile.x);
    }

    const FloorTileState& LotGrid::FloorAt(const TileCoordinate tile) const
    {
        return floors_[CellIndex(tile)];
    }

    bool LotGrid::SetTerrain(const TileCoordinate tile, const TerrainKind terrain)
    {
        FloorTileState& state = floors_[CellIndex(tile)];
        if (state.terrain == terrain)
            return false;
        state.terrain = terrain;
        state.visualDirty = true;
        return true;
    }

    bool LotGrid::SetFloorCovering(
        const TileCoordinate tile, std::optional<std::string> floorCoveringId)
    {
        if (floorCoveringId.has_value() && floorCoveringId->empty())
            throw std::invalid_argument("floor-covering ID must not be empty");
        FloorTileState& state = floors_[CellIndex(tile)];
        if (state.floorCoveringId == floorCoveringId)
            return false;
        state.floorCoveringId = std::move(floorCoveringId);
        state.visualDirty = true;
        return true;
    }

    void LotGrid::ClearFloorVisualDirty(const TileCoordinate tile)
    {
        floors_[CellIndex(tile)].visualDirty = false;
    }

    bool LotGrid::IsFloorVisualDirty(const TileCoordinate tile) const
    {
        return floors_[CellIndex(tile)].visualDirty;
    }

    WallEdge LotGrid::CanonicalWall(const TileCoordinate tile, const TileEdge edge) const
    {
        (void)CellIndex(tile);
        switch (edge)
        {
            case TileEdge::MinY: return {tile.floor, tile.y, tile.x, WallAxis::AlongX};
            case TileEdge::MaxX: return {tile.floor, tile.y, tile.x + 1, WallAxis::AlongY};
            case TileEdge::MaxY: return {tile.floor, tile.y + 1, tile.x, WallAxis::AlongX};
            case TileEdge::MinX: return {tile.floor, tile.y, tile.x, WallAxis::AlongY};
        }
        throw std::invalid_argument("tile edge is invalid");
    }

    bool LotGrid::AddWall(const TileCoordinate tile, const TileEdge edge)
    {
        const WallEdge wall = CanonicalWall(tile, edge);
        const bool inserted = walls_.insert(wall).second;
        if (inserted)
        {
            roomsDirty_[static_cast<std::size_t>(tile.floor)] = true;
            routingDirty_[static_cast<std::size_t>(tile.floor)] = true;
        }
        return inserted;
    }

    bool LotGrid::RemoveWall(const TileCoordinate tile, const TileEdge edge)
    {
        const WallEdge wall = CanonicalWall(tile, edge);
        const bool removed = walls_.erase(wall) != 0;
        if (removed)
        {
            doors_.erase(wall);
            roomsDirty_[static_cast<std::size_t>(tile.floor)] = true;
            routingDirty_[static_cast<std::size_t>(tile.floor)] = true;
        }
        return removed;
    }

    bool LotGrid::HasWall(const TileCoordinate tile, const TileEdge edge) const
    {
        return walls_.contains(CanonicalWall(tile, edge));
    }

    const std::set<WallEdge>& LotGrid::Walls() const noexcept
    {
        return walls_;
    }

    void LotGrid::ValidateWall(const WallEdge wall) const
    {
        ValidateFloor(wall.floor);
        const bool valid = wall.axis == WallAxis::AlongX
            ? wall.x >= 0 && wall.x < width_ && wall.y >= 0 && wall.y <= height_
            : wall.x >= 0 && wall.x <= width_ && wall.y >= 0 && wall.y < height_;
        if (!valid)
            throw std::out_of_range("canonical wall edge is outside the lot");
    }

    std::vector<TileCoordinate> LotGrid::AdjacentTiles(const WallEdge wall) const
    {
        ValidateWall(wall);
        std::vector<TileCoordinate> result;
        result.reserve(2);
        if (wall.axis == WallAxis::AlongX)
        {
            const TileCoordinate positive{wall.x, wall.y, wall.floor};
            const TileCoordinate negative{wall.x, wall.y - 1, wall.floor};
            if (Contains(positive)) result.push_back(positive);
            if (Contains(negative)) result.push_back(negative);
        }
        else
        {
            const TileCoordinate positive{wall.x, wall.y, wall.floor};
            const TileCoordinate negative{wall.x - 1, wall.y, wall.floor};
            if (Contains(positive)) result.push_back(positive);
            if (Contains(negative)) result.push_back(negative);
        }
        return result;
    }

    bool LotGrid::AddDoor(
        const TileCoordinate tile, const TileEdge edge, const bool open)
    {
        const WallEdge wall = CanonicalWall(tile, edge);
        if (!walls_.contains(wall))
            throw std::invalid_argument("door requires an existing wall edge");
        const bool inserted = doors_.emplace(wall, DoorState{open}).second;
        if (inserted)
            routingDirty_[static_cast<std::size_t>(tile.floor)] = true;
        return inserted;
    }

    bool LotGrid::RemoveDoor(const TileCoordinate tile, const TileEdge edge)
    {
        const WallEdge wall = CanonicalWall(tile, edge);
        const bool removed = doors_.erase(wall) != 0;
        if (removed)
            routingDirty_[static_cast<std::size_t>(tile.floor)] = true;
        return removed;
    }

    bool LotGrid::HasDoor(const TileCoordinate tile, const TileEdge edge) const
    {
        return HasDoor(CanonicalWall(tile, edge));
    }

    bool LotGrid::HasDoor(const WallEdge wall) const
    {
        ValidateWall(wall);
        return doors_.contains(wall);
    }

    bool LotGrid::IsDoorOpen(const TileCoordinate tile, const TileEdge edge) const
    {
        return IsDoorOpen(CanonicalWall(tile, edge));
    }

    bool LotGrid::IsDoorOpen(const WallEdge wall) const
    {
        ValidateWall(wall);
        const auto found = doors_.find(wall);
        if (found == doors_.end())
            throw std::invalid_argument("wall edge has no door");
        return found->second.open;
    }

    bool LotGrid::SetDoorOpen(
        const TileCoordinate tile, const TileEdge edge, const bool open)
    {
        return SetDoorOpen(CanonicalWall(tile, edge), open);
    }

    bool LotGrid::SetDoorOpen(const WallEdge wall, const bool open)
    {
        ValidateWall(wall);
        const auto found = doors_.find(wall);
        if (found == doors_.end())
            throw std::invalid_argument("wall edge has no door");
        if (found->second.open == open)
            return false;
        found->second.open = open;
        routingDirty_[static_cast<std::size_t>(wall.floor)] = true;
        return true;
    }

    bool LotGrid::WallBlocksRouting(const TileCoordinate tile, const TileEdge edge) const
    {
        const WallEdge wall = CanonicalWall(tile, edge);
        if (!walls_.contains(wall))
            return false;
        const auto door = doors_.find(wall);
        return door == doors_.end() || !door->second.open;
    }

    const std::map<WallEdge, DoorState>& LotGrid::Doors() const noexcept
    {
        return doors_;
    }

    void LotGrid::ValidateFloor(const int floor) const
    {
        if (floor < 0 || floor >= floorCount_)
            throw std::out_of_range("floor is outside the lot");
    }

    bool LotGrid::RoomsDirty(const int floor) const
    {
        ValidateFloor(floor);
        return roomsDirty_[static_cast<std::size_t>(floor)];
    }

    void LotGrid::AcknowledgeRoomsRebuilt(const int floor)
    {
        ValidateFloor(floor);
        roomsDirty_[static_cast<std::size_t>(floor)] = false;
    }

    bool LotGrid::RoutingDirty(const int floor) const
    {
        ValidateFloor(floor);
        return routingDirty_[static_cast<std::size_t>(floor)];
    }

    void LotGrid::AcknowledgeRoutingRebuilt(const int floor)
    {
        ValidateFloor(floor);
        routingDirty_[static_cast<std::size_t>(floor)] = false;
    }
}
