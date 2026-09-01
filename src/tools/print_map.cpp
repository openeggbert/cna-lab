// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MeshWorldMap — ASCII grid visualizer for the world zone/region map.
//
// Usage:
//   MeshWorldMap [world.json] [-r] [--chunk X,Y]
//   MeshWorldMap --planet <world-dir>
//
// Flags:
//   -r / --region     show RegionType layer instead of ZoneType (default: zone)
//   --chunk X,Y       highlight chunk (X,Y) in the grid with [brackets] and
//                      print its zone/region/exits below the map (legacy flat
//                      WorldMap grid only -- not supported with --planet)
//   --planet <dir>    (M208) view an existing planet world's level-0 map instead of
//                      the legacy flat WorldMap zone/region grid -- reuses
//                      PlanetMapLogic's render_ascii_biome_map() and
//                      PlanetWorld::open_existing() rather than duplicating the
//                      letter-rendering logic. <dir> must already be a planet world
//                      (created by MeshWorldPlanet); this tool only views, never creates.

#include <iostream>
#include <sstream>
#include <string>
#include "BuiltinMaterials.hpp"
#include "ContentPackLoader.hpp"
#include "Map/MapPipeline.hpp"
#include "PlanetMapLogic.hpp"
#include "PlanetWorld.hpp"
#include "WorldConfig.hpp"
#include "WorldMap.hpp"
#include "ZoneType.hpp"
#include "RegionType.hpp"

using namespace MeshWorld;

// Grown 12->52, M235 (MAP16, 2026-07-10) -- same letter assignment as
// PlanetMapLogic.cpp's zone_ascii_char(); keep the two in sync if either
// changes (this one is the legacy flat-WorldMap visualizer's own copy,
// M206's own comment already notes the two were meant to match).
static char zone_char(ZoneType z) {
    switch (z) {
        case ZoneType::city:     return 'C';
        case ZoneType::jungle:   return 'J';
        case ZoneType::desert:   return 'D';
        case ZoneType::forest:   return 'F';
        case ZoneType::ocean:    return 'O';
        case ZoneType::mountain: return 'M';
        case ZoneType::tundra:   return 'T';
        case ZoneType::swamp:    return 'S';
        case ZoneType::cave:     return 'V';
        case ZoneType::meadow:   return 'E';
        case ZoneType::beach:    return 'B';

        case ZoneType::savanna:               return 'A';
        case ZoneType::steppe:                return 'G';
        case ZoneType::prairie:               return 'H';
        case ZoneType::chaparral:             return 'I';
        case ZoneType::shrubland:             return 'K';

        case ZoneType::taiga:                 return 'L';
        case ZoneType::temperate_rainforest:  return 'N';
        case ZoneType::mixed_forest:          return 'P';
        case ZoneType::cloud_forest:          return 'Q';
        case ZoneType::mangrove:              return 'R';
        case ZoneType::bamboo_forest:         return 'U';
        case ZoneType::riparian_forest:       return 'W';
        case ZoneType::tropical_dry_forest:   return 'X';

        case ZoneType::marsh:                 return 'Y';
        case ZoneType::floodplain:            return 'Z';
        case ZoneType::bog:                   return 'a';
        case ZoneType::muskeg:                return 'b';

        case ZoneType::dunes:                 return 'c';
        case ZoneType::rocky_desert:          return 'd';
        case ZoneType::cold_desert:           return 'e';
        case ZoneType::salt_flat:             return 'f';
        case ZoneType::badlands:              return 'g';
        case ZoneType::mesa:                  return 'h';
        case ZoneType::canyon:                return 'i';
        case ZoneType::oasis:                 return 'j';

        case ZoneType::glacier:               return 'k';
        case ZoneType::permafrost:            return 'l';
        case ZoneType::alpine_meadow:         return 'm';
        case ZoneType::ice_cap:               return 'n';

        case ZoneType::volcanic:              return 'o';
        case ZoneType::geothermal:            return 'p';
        case ZoneType::ash_plain:             return 'q';
        case ZoneType::volcanic_island:       return 'r';

        case ZoneType::coral_reef:            return 's';
        case ZoneType::kelp_forest:           return 't';
        case ZoneType::deep_ocean:            return 'u';
        case ZoneType::lagoon:                return 'v';
        case ZoneType::fjord:                 return 'w';
        case ZoneType::tidal_flat:            return 'x';
        case ZoneType::sea_cliff:             return 'y';

        case ZoneType::empty:    return '.';
    }
    return '?';
}

static char region_char(RegionType r) {
    switch (r) {
        case RegionType::open:              return 'o';
        case RegionType::water:             return 'w';
        case RegionType::ruins:             return 'u';
        case RegionType::cliff:             return 'c';
        case RegionType::clearing:          return 'l';
        case RegionType::highland:          return 'h';
        case RegionType::road:              return 'r';
        case RegionType::crossroad:         return 'x';
        case RegionType::small_house_block: return 's';
        case RegionType::apartment_block:   return 'a';
        case RegionType::shop_street:       return 'P';
        case RegionType::square:            return 'q';
        case RegionType::park:              return 'p';
        case RegionType::river_bank:        return 'b';
        case RegionType::bridge:            return 'g';
        case RegionType::oasis:             return 'z';
        case RegionType::cave_chamber:      return 'v';
        case RegionType::empty:             return '.';
    }
    return '?';
}

// Parses "X,Y" into (out_x, out_y). Returns false (and leaves out_x/out_y
// untouched) on any malformed input -- caller decides how to report it.
static bool parse_chunk_coord(const std::string& s, int* out_x, int* out_y) {
    const auto comma = s.find(',');
    if (comma == std::string::npos) return false;
    try {
        size_t x_end = 0, y_end = 0;
        const int x = std::stoi(s.substr(0, comma), &x_end);
        const int y = std::stoi(s.substr(comma + 1), &y_end);
        if (x_end != comma || y_end != s.size() - comma - 1) return false;
        *out_x = x;
        *out_y = y;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

int main(int argc, char* argv[]) {
    std::string config_path = "examples/world.json";
    bool show_region = false;
    std::string planet_dir;
    bool has_highlight = false;
    int  highlight_x = 0, highlight_y = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-r" || arg == "--region") {
            show_region = true;
        } else if (arg == "--planet" && i + 1 < argc) {
            planet_dir = argv[++i];
        } else if (arg == "--chunk" && i + 1 < argc) {
            const std::string coord_arg = argv[++i];
            if (!parse_chunk_coord(coord_arg, &highlight_x, &highlight_y)) {
                std::cerr << "Error: --chunk expects \"X,Y\" (got \"" << coord_arg << "\")\n";
                return 1;
            }
            has_highlight = true;
        } else {
            config_path = arg;
        }
    }

    if (!planet_dir.empty()) {
        if (!is_existing_planet_world(planet_dir)) {
            std::cerr << "Error: " << planet_dir << " is not an existing planet world "
                                                      "(create one first with MeshWorldPlanet)\n";
            return 1;
        }
        if (has_highlight) {
            std::cerr << "Warning: --chunk is not supported with --planet, ignoring\n";
        }

        register_builtin_materials();
        ContentPackLoader{}.load_auto(".", "meshworld_content.sqlite");

        PlanetWorld       world = PlanetWorld::open_existing(planet_dir);
        Map::MapPipeline  pipeline(world, planet_params_from_config(world.config()));
        const Map::MapTilePayload payload = pipeline.get(Map::TileCoord{0, 0, 0});

        std::cout << "Planet map (level 0) — " << planet_dir
                  << "  entropy=" << world.world_entropy()
                  << "  " << payload.biome.w << "x" << payload.biome.h << "\n\n";
        std::cout << render_ascii_biome_map(payload.biome);
        std::cout << "\nLegend (zones):\n";
        std::cout << "  C=city    J=jungle  D=desert  F=forest   O=ocean\n";
        std::cout << "  M=mountain T=tundra  S=swamp   V=cave    E=meadow\n";
        std::cout << "  B=beach   .=empty\n";
        return 0;
    }

    WorldConfig cfg;
    if (!cfg.load_from_file(config_path)) {
        std::cerr << "Error: cannot load " << config_path << "\n";
        return 1;
    }

    WorldMap map(cfg);

    const char* layer = show_region ? "Region" : "Zone";
    std::cout << layer << " map — " << cfg.name
              << "  seed=" << cfg.seed
              << "  " << cfg.grid_w << "x" << cfg.grid_h
              << "  (" << (cfg.procedural ? "procedural" : "config-driven") << ")\n\n";

    for (int y = 0; y < cfg.grid_h; ++y) {
        for (int x = 0; x < cfg.grid_w; ++x) {
            const bool is_highlight = has_highlight && x == highlight_x && y == highlight_y;
            if (x > 0) std::cout << (is_highlight ? "" : " ");
            auto info = map.info(x, y);
            const char c = show_region ? region_char(info.region) : zone_char(info.zone);
            if (is_highlight) {
                std::cout << '[' << c << ']';
            } else {
                std::cout << c;
            }
        }
        std::cout << '\n';
    }

    std::cout << '\n';

    if (has_highlight) {
        if (highlight_x < 0 || highlight_x >= cfg.grid_w ||
            highlight_y < 0 || highlight_y >= cfg.grid_h) {
            std::cerr << "Warning: --chunk " << highlight_x << "," << highlight_y
                      << " is outside the grid (" << cfg.grid_w << "x" << cfg.grid_h
                      << "), nothing highlighted\n";
        } else {
            const auto info = map.info(highlight_x, highlight_y);
            std::cout << "Chunk (" << highlight_x << "," << highlight_y << "): "
                      << "zone=" << to_string(info.zone)
                      << " region=" << to_string(info.region)
                      << " exits=[";
            bool first = true;
            auto add_exit = [&](bool present, const char* name) {
                if (!present) return;
                if (!first) std::cout << ' ';
                std::cout << name;
                first = false;
            };
            add_exit(info.exits.north_road, "N-road");
            add_exit(info.exits.south_road, "S-road");
            add_exit(info.exits.east_road,  "E-road");
            add_exit(info.exits.west_road,  "W-road");
            add_exit(info.exits.north_path, "N-path");
            add_exit(info.exits.south_path, "S-path");
            add_exit(info.exits.east_path,  "E-path");
            add_exit(info.exits.west_path,  "W-path");
            std::cout << "]\n\n";
        }
    }

    if (!show_region) {
        std::cout << "Legend (zones):\n";
        std::cout << "  C=city    J=jungle  D=desert  F=forest   O=ocean\n";
        std::cout << "  M=mountain T=tundra  S=swamp   V=cave    E=meadow\n";
        std::cout << "  B=beach   .=empty\n";
    } else {
        std::cout << "Legend (regions):\n";
        std::cout << "  r=road      x=crossroad   s=small_house  a=apartment  P=shop_street\n";
        std::cout << "  p=park      q=square      b=river_bank   g=bridge     w=water\n";
        std::cout << "  o=open      u=ruins       c=cliff        l=clearing   h=highland\n";
        std::cout << "  z=oasis     v=cave_chamber .=empty\n";
    }

    return 0;
}
