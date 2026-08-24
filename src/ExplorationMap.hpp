#pragma once

#include <cstddef>
#include <vector>

#include "LevelDefinition.hpp"

namespace WolfCna
{
    class MapToggleLatch final
    {
    public:
        [[nodiscard]] bool Update(bool mapIsDown, bool loadoutCheatIsDown);
        void Reset();

    private:
        bool wasDown_ = false;
    };

    class ExplorationMap final
    {
    public:
        explicit ExplorationMap(const LevelDefinition& level);

        void Reset(const LevelDefinition& level);
        [[nodiscard]] bool Visit(float worldX, float worldZ);
        [[nodiscard]] bool IsVisited(int x, int z) const;
        [[nodiscard]] int Width() const;
        [[nodiscard]] int Height() const;

    private:
        int width_ = 0;
        int height_ = 0;
        std::vector<bool> visited_;
        std::vector<bool> walkable_;

        [[nodiscard]] std::size_t Index(int x, int z) const;
    };
}
