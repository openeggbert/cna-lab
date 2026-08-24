#include "CopperBoots/TileMap.hpp"

#include <stdexcept>

namespace CopperBoots
{
    TileMap::TileMap(const int width, const int height)
        : width_(width),
          height_(height),
          tiles_(static_cast<std::size_t>(width * height), TileKind::Empty)
    {
        if (width <= 0 || height <= 0)
            throw std::invalid_argument("TileMap dimensions must be positive");
    }

    TileMap TileMap::CreateTestRoom()
    {
        TileMap map(96, 12);

        for (int x = 0; x < map.Width(); ++x) {
            map.Set(x, 9, TileKind::Solid);
            map.Set(x, 10, TileKind::Solid);
            map.Set(x, 11, TileKind::Solid);
        }

        for (int x = 10; x <= 14; ++x)
            map.Set(x, 7, TileKind::Solid);
        for (int x = 22; x <= 26; ++x)
            map.Set(x, 6, TileKind::Solid);
        for (int x = 35; x <= 39; ++x)
            map.Set(x, 8, TileKind::Solid);
        for (int x = 48; x <= 55; ++x)
            map.Set(x, 7, TileKind::Solid);
        for (int x = 66; x <= 70; ++x)
            map.Set(x, 5, TileKind::Solid);

        for (int y = 6; y <= 8; ++y)
            map.Set(31, y, TileKind::Solid);
        for (int y = 4; y <= 8; ++y)
            map.Set(78, y, TileKind::Solid);

        return map;
    }

    void TileMap::Set(const int x, const int y, const TileKind kind)
    {
        if (x < 0 || x >= width_ || y < 0 || y >= height_)
            throw std::out_of_range("TileMap::Set coordinate out of range");
        tiles_[Index(x, y)] = kind;
    }

    TileKind TileMap::Get(const int x, const int y) const noexcept
    {
        if (x < 0 || x >= width_ || y >= height_)
            return TileKind::Solid;
        if (y < 0)
            return TileKind::Empty;
        return tiles_[Index(x, y)];
    }

    bool TileMap::IsSolid(const int x, const int y) const noexcept
    {
        return Get(x, y) == TileKind::Solid;
    }

    std::size_t TileMap::Index(const int x, const int y) const noexcept
    {
        return static_cast<std::size_t>(y * width_ + x);
    }
}

