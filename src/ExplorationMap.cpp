#include "ExplorationMap.hpp"

#include <cmath>

namespace WolfCna
{
    bool MapToggleLatch::Update(bool mapIsDown, bool loadoutCheatIsDown)
    {
        const bool toggleRequested = mapIsDown && !wasDown_ && !loadoutCheatIsDown;
        wasDown_ = mapIsDown;
        return toggleRequested;
    }

    void MapToggleLatch::Reset()
    {
        wasDown_ = false;
    }

    ExplorationMap::ExplorationMap(const LevelDefinition& level)
    {
        Reset(level);
    }

    void ExplorationMap::Reset(const LevelDefinition& level)
    {
        const auto& rows = level.Rows();
        height_ = static_cast<int>(rows.size());
        width_ = rows.empty() ? 0 : static_cast<int>(rows.front().size());
        visited_.assign(static_cast<std::size_t>(width_ * height_), false);
        walkable_.assign(static_cast<std::size_t>(width_ * height_), false);
        for (int z = 0; z < height_; ++z)
            for (int x = 0; x < width_; ++x)
                walkable_[Index(x, z)] = rows[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)] != '#';
    }

    bool ExplorationMap::Visit(float worldX, float worldZ)
    {
        const int x = static_cast<int>(std::floor(worldX));
        const int z = static_cast<int>(std::floor(worldZ));
        if (x < 0 || z < 0 || x >= width_ || z >= height_ || !walkable_[Index(x, z)])
            return false;

        const std::size_t index = Index(x, z);
        const bool newlyVisited = !visited_[index];
        visited_[index] = true;
        return newlyVisited;
    }

    bool ExplorationMap::IsVisited(int x, int z) const
    {
        return x >= 0 && z >= 0 && x < width_ && z < height_ && visited_[Index(x, z)];
    }

    int ExplorationMap::Width() const
    {
        return width_;
    }

    int ExplorationMap::Height() const
    {
        return height_;
    }

    std::size_t ExplorationMap::Index(int x, int z) const
    {
        return static_cast<std::size_t>(z * width_ + x);
    }
}
