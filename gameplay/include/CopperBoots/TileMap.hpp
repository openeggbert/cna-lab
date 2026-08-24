#pragma once

#include <cstddef>
#include <vector>

namespace CopperBoots
{
    enum class TileKind
    {
        Empty,
        Solid,
    };

    class TileMap
    {
    public:
        static constexpr int TileSize = 16;

        TileMap(int width, int height);

        [[nodiscard]] static TileMap CreateTestRoom();

        void Set(int x, int y, TileKind kind);
        [[nodiscard]] TileKind Get(int x, int y) const noexcept;
        [[nodiscard]] bool IsSolid(int x, int y) const noexcept;
        [[nodiscard]] int Width() const noexcept { return width_; }
        [[nodiscard]] int Height() const noexcept { return height_; }
        [[nodiscard]] int PixelWidth() const noexcept { return width_ * TileSize; }
        [[nodiscard]] int PixelHeight() const noexcept { return height_ * TileSize; }

    private:
        [[nodiscard]] std::size_t Index(int x, int y) const noexcept;

        int width_;
        int height_;
        std::vector<TileKind> tiles_;
    };
}

