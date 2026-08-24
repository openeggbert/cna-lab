#include "CopperBoots/TileMap.hpp"

#include <stdexcept>

namespace CopperBoots
{
    TileMap::TileMap(const int width, const int height)
        : width_(width),
          height_(height),
          tiles_(static_cast<std::size_t>(width * height), Tiles::Empty)
    {
        if (width <= 0 || height <= 0)
            throw std::invalid_argument("TileMap dimensions must be positive");
    }

    TileMap TileMap::CreateTestRoom()
    {
        TileMap map(96, 12);

        for (int x = 0; x < map.Width(); ++x) {
            map.Set(x, 9, Tiles::Ruin);
            map.Set(x, 10, Tiles::Ruin);
            map.Set(x, 11, Tiles::Ruin);
        }

        for (int x = 10; x <= 14; ++x)
            map.Set(x, 7, Tiles::Ruin);
        for (int x = 22; x <= 26; ++x)
            map.Set(x, 6, Tiles::Ruin);
        for (int x = 35; x <= 39; ++x)
            map.Set(x, 8, Tiles::Ruin);
        for (int x = 48; x <= 55; ++x)
            map.Set(x, 7, Tiles::Ruin);
        for (int x = 66; x <= 70; ++x)
            map.Set(x, 5, Tiles::Ruin);

        for (int y = 6; y <= 8; ++y)
            map.Set(31, y, Tiles::Ruin);
        for (int y = 4; y <= 8; ++y)
            map.Set(78, y, Tiles::Ruin);

        return map;
    }

    void TileMap::Set(const int x, const int y, const Tile tile)
    {
        if (x < 0 || x >= width_ || y < 0 || y >= height_)
            throw std::out_of_range("TileMap::Set coordinate out of range");
        tiles_[Index(x, y)] = tile;
    }

    Tile TileMap::Get(const int x, const int y) const noexcept
    {
        if (x < 0 || x >= width_ || y >= height_)
            return Tiles::Empty;
        if (y < 0)
            return Tiles::Empty;
        return tiles_[Index(x, y)];
    }

    bool TileMap::IsSolid(const int x, const int y) const noexcept
    {
        if (y < 0)
            return false;
        if (x < 0 || x >= width_ || y >= height_)
            return true;
        return Get(x, y).Collision == TileCollision::Solid;
    }

    std::size_t TileMap::Index(const int x, const int y) const noexcept
    {
        return static_cast<std::size_t>(y * width_ + x);
    }
}
