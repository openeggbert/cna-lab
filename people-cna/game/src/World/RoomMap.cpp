#include "People/World/RoomMap.hpp"

#include <array>
#include <queue>
#include <stdexcept>
#include <utility>

namespace People::World
{
    namespace
    {
        struct Neighbor
        {
            int dx;
            int dy;
            TileEdge edge;
        };

        constexpr std::array<Neighbor, 4> Neighbors{{
            {0, -1, TileEdge::MinY},
            {1, 0, TileEdge::MaxX},
            {0, 1, TileEdge::MaxY},
            {-1, 0, TileEdge::MinX}
        }};

        [[nodiscard]] std::size_t FlatIndex(const TileCoordinate tile, const LotSize size)
        {
            return static_cast<std::size_t>(tile.y) * static_cast<std::size_t>(size.width)
                + static_cast<std::size_t>(tile.x);
        }
    }

    RoomMap::RoomMap(
        const LotSize size, const int floor, std::vector<RoomId> rooms,
        const RoomId roomCount)
        : size_(size), floor_(floor), rooms_(std::move(rooms)), roomCount_(roomCount)
    {
    }

    RoomMap RoomMap::Rebuild(LotGrid& lot, const int floor)
    {
        if (floor < 0 || floor >= lot.FloorCount())
            throw std::out_of_range("room-map floor is outside the lot");

        const LotSize size = lot.Size();
        const std::size_t cellCount = static_cast<std::size_t>(size.width)
            * static_cast<std::size_t>(size.height);
        std::vector<RoomId> rooms(cellCount, OutsideRoom);
        std::vector<bool> visited(cellCount, false);
        RoomId nextRoom = 1;

        for (int startY = 0; startY < size.height; ++startY)
        {
            for (int startX = 0; startX < size.width; ++startX)
            {
                const TileCoordinate start{startX, startY, floor};
                const std::size_t startIndex = FlatIndex(start, size);
                if (visited[startIndex])
                    continue;

                std::queue<TileCoordinate> pending;
                std::vector<std::size_t> component;
                bool reachesOutside = false;
                pending.push(start);
                visited[startIndex] = true;

                while (!pending.empty())
                {
                    const TileCoordinate current = pending.front();
                    pending.pop();
                    component.push_back(FlatIndex(current, size));

                    for (const Neighbor neighbor : Neighbors)
                    {
                        if (lot.HasWall(current, neighbor.edge))
                            continue;
                        const TileCoordinate next{
                            current.x + neighbor.dx,
                            current.y + neighbor.dy,
                            floor
                        };
                        if (!lot.Contains(next))
                        {
                            reachesOutside = true;
                            continue;
                        }
                        const std::size_t nextIndex = FlatIndex(next, size);
                        if (!visited[nextIndex])
                        {
                            visited[nextIndex] = true;
                            pending.push(next);
                        }
                    }
                }

                const RoomId assignment = reachesOutside ? OutsideRoom : nextRoom++;
                for (const std::size_t index : component)
                    rooms[index] = assignment;
            }
        }

        lot.AcknowledgeRoomsRebuilt(floor);
        return RoomMap(size, floor, std::move(rooms), nextRoom - 1);
    }

    std::size_t RoomMap::Index(const TileCoordinate tile) const
    {
        if (tile.floor != floor_ || tile.x < 0 || tile.y < 0
            || tile.x >= size_.width || tile.y >= size_.height)
            throw std::out_of_range("tile is outside this room map");
        return FlatIndex(tile, size_);
    }

    RoomId RoomMap::RoomAt(const TileCoordinate tile) const
    {
        return rooms_[Index(tile)];
    }

    int RoomMap::Floor() const noexcept
    {
        return floor_;
    }

    RoomId RoomMap::EnclosedRoomCount() const noexcept
    {
        return roomCount_;
    }
}
