#pragma once

#include <cstdint>
#include <vector>

#include "People/World/LotGrid.hpp"

namespace People::World
{
    using RoomId = std::uint32_t;
    inline constexpr RoomId OutsideRoom = 0;

    /** @brief Deterministic room assignment for one logical floor. */
    class RoomMap final
    {
    public:
        /**
         * @brief Flood-fills cells separated by walls and marks open components outside.
         *
         * Enclosed components receive IDs from one in row-major discovery order.
         * The acknowledged lot-floor dirty flag is cleared after success.
         */
        [[nodiscard]] static RoomMap Rebuild(LotGrid& lot, int floor);

        [[nodiscard]] RoomId RoomAt(TileCoordinate tile) const;
        [[nodiscard]] int Floor() const noexcept;
        [[nodiscard]] RoomId EnclosedRoomCount() const noexcept;

    private:
        RoomMap(LotSize size, int floor, std::vector<RoomId> rooms, RoomId roomCount);

        [[nodiscard]] std::size_t Index(TileCoordinate tile) const;

        LotSize size_;
        int floor_;
        std::vector<RoomId> rooms_;
        RoomId roomCount_;
    };
}
