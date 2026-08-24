#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "People/World/IsometricProjection.hpp"

namespace People::World
{
    /** @brief Original logical base terrain; presentation chooses its texture. */
    enum class TerrainKind : std::uint8_t
    {
        Grass,
        Soil,
        Stone
    };

    /** @brief Persistent floor-cell data with transient presentation dirtiness. */
    struct FloorTileState
    {
        TerrainKind terrain = TerrainKind::Grass;
        std::optional<std::string> floorCoveringId;
        bool visualDirty = true;
    };

    /** @brief A side of one logical tile in simulation coordinates. */
    enum class TileEdge : std::uint8_t
    {
        MinY,
        MaxX,
        MaxY,
        MinX
    };

    /** @brief Direction of a canonical wall segment between grid vertices. */
    enum class WallAxis : std::uint8_t
    {
        AlongX,
        AlongY
    };

    /**
     * @brief Unique wall edge represented by its minimum grid vertex and axis.
     *
     * AlongX accepts x in [0,width-1], y in [0,height]. AlongY accepts x in
     * [0,width], y in [0,height-1]. This representation removes neighbor-side
     * duplication by construction.
     */
    struct WallEdge
    {
        int floor = 0;
        int y = 0;
        int x = 0;
        WallAxis axis = WallAxis::AlongX;

        auto operator<=>(const WallEdge&) const = default;
    };

    /** @brief Renderer-independent bounded lot, floor cells, and wall topology. */
    class LotGrid final
    {
    public:
        explicit LotGrid(int width, int height, int floorCount = 1);

        [[nodiscard]] LotSize Size() const noexcept;
        [[nodiscard]] int FloorCount() const noexcept;
        [[nodiscard]] bool Contains(TileCoordinate tile) const noexcept;

        [[nodiscard]] const FloorTileState& FloorAt(TileCoordinate tile) const;
        [[nodiscard]] bool SetTerrain(TileCoordinate tile, TerrainKind terrain);
        [[nodiscard]] bool SetFloorCovering(
            TileCoordinate tile, std::optional<std::string> floorCoveringId);
        void ClearFloorVisualDirty(TileCoordinate tile);
        [[nodiscard]] bool IsFloorVisualDirty(TileCoordinate tile) const;

        [[nodiscard]] WallEdge CanonicalWall(TileCoordinate tile, TileEdge edge) const;
        [[nodiscard]] bool AddWall(TileCoordinate tile, TileEdge edge);
        [[nodiscard]] bool RemoveWall(TileCoordinate tile, TileEdge edge);
        [[nodiscard]] bool HasWall(TileCoordinate tile, TileEdge edge) const;
        [[nodiscard]] const std::set<WallEdge>& Walls() const noexcept;
        [[nodiscard]] std::vector<TileCoordinate> AdjacentTiles(WallEdge wall) const;

        [[nodiscard]] bool RoomsDirty(int floor) const;
        void AcknowledgeRoomsRebuilt(int floor);

    private:
        [[nodiscard]] std::size_t CellIndex(TileCoordinate tile) const;
        void ValidateFloor(int floor) const;
        void ValidateWall(WallEdge wall) const;

        int width_;
        int height_;
        int floorCount_;
        std::vector<FloorTileState> floors_;
        std::set<WallEdge> walls_;
        std::vector<bool> roomsDirty_;
    };
}
