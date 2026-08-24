#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "CopperBoots/TileMap.hpp"

namespace CopperBoots
{
    struct TileCoordinate
    {
        int X;
        int Y;
    };

    struct LevelDefinition
    {
        std::string Name;
        TileMap Map;
        int SpawnTileX;
        int SpawnFootTileY;
        int CheckpointTileX;
        int CheckpointFootTileY;
        std::array<float, 3> ParallaxFactors;
        std::vector<TileCoordinate> Cogs;

        [[nodiscard]] static LevelDefinition Parse(
            std::string_view text,
            std::string_view sourceName = "<level>");
    };
}
