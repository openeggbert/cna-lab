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

    enum class BlockContent
    {
        None,
        Cog,
        Plating,
        Capacitor,
    };

    struct InteractiveBlockDefinition
    {
        TileCoordinate Position;
        BlockContent Content;
    };

    struct CrawlerDefinition
    {
        TileCoordinate Position;
        bool FallsAtEdges;
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
        std::vector<CrawlerDefinition> Crawlers;
        std::vector<TileCoordinate> PlatingPickups;
        std::vector<TileCoordinate> CapacitorPickups;
        std::vector<TileCoordinate> Checkpoints;
        std::vector<InteractiveBlockDefinition> InteractiveBlocks;

        [[nodiscard]] static LevelDefinition Parse(
            std::string_view text,
            std::string_view sourceName = "<level>");
    };
}
