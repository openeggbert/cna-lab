#pragma once

#include <cstddef>
#include <vector>

#include "LevelDefinition.hpp"

namespace WolfCna
{
    class ExplorationMap final
    {
    public:
        explicit ExplorationMap(const LevelDefinition& level);

        void Reset(const LevelDefinition& level);
        [[nodiscard]] bool Visit(float worldX, float worldZ);
        [[nodiscard]] bool IsVisited(int x, int z) const;
        [[nodiscard]] int Width() const;
        [[nodiscard]] int Height() const;
        [[nodiscard]] int GoalX() const;
        [[nodiscard]] int GoalZ() const;
        [[nodiscard]] const std::vector<bool>& CaptureVisited() const;
        [[nodiscard]] bool RestoreVisited(const std::vector<bool>& visited);

    private:
        int width_ = 0;
        int height_ = 0;
        int goalX_ = -1;
        int goalZ_ = -1;
        std::vector<bool> visited_;
        std::vector<bool> walkable_;

        [[nodiscard]] std::size_t Index(int x, int z) const;
    };
}
