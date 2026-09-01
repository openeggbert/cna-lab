#pragma once

#include <cstddef>
#include <vector>

namespace CopperBoots
{
    enum class TileVisual
    {
        None,
        Ruin,
        Breakable,
        Interactive,
        UsedBlock,
        OneWay,
        Hazard,
        Exit,
        Decoration,
    };

    enum class TileCollision
    {
        None,
        Solid,
        OneWay,
        Hazard,
        Exit,
    };

    struct Tile
    {
        TileVisual Visual = TileVisual::None;
        TileCollision Collision = TileCollision::None;

        [[nodiscard]] constexpr bool operator==(const Tile&) const = default;
    };

    namespace Tiles
    {
        inline constexpr Tile Empty{};
        inline constexpr Tile Ruin{TileVisual::Ruin, TileCollision::Solid};
        inline constexpr Tile Breakable{TileVisual::Breakable, TileCollision::Solid};
        inline constexpr Tile Interactive{TileVisual::Interactive, TileCollision::Solid};
        inline constexpr Tile UsedBlock{TileVisual::UsedBlock, TileCollision::Solid};
        inline constexpr Tile OneWay{TileVisual::OneWay, TileCollision::OneWay};
        inline constexpr Tile Hazard{TileVisual::Hazard, TileCollision::Hazard};
        inline constexpr Tile Exit{TileVisual::Exit, TileCollision::Exit};
        inline constexpr Tile Decoration{TileVisual::Decoration, TileCollision::None};
    };

    class TileMap
    {
    public:
        static constexpr int TileSize = 16;

        TileMap(int width, int height);

        [[nodiscard]] static TileMap CreateTestRoom();

        void Set(int x, int y, Tile tile);
        [[nodiscard]] Tile Get(int x, int y) const noexcept;
        [[nodiscard]] bool IsSolid(int x, int y) const noexcept;
        [[nodiscard]] int Width() const noexcept { return width_; }
        [[nodiscard]] int Height() const noexcept { return height_; }
        [[nodiscard]] int PixelWidth() const noexcept { return width_ * TileSize; }
        [[nodiscard]] int PixelHeight() const noexcept { return height_ * TileSize; }

    private:
        [[nodiscard]] std::size_t Index(int x, int y) const noexcept;

        int width_;
        int height_;
        std::vector<Tile> tiles_;
    };
}
