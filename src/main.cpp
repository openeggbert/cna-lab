// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include <iostream>
#include "ContentPackLoader.hpp"
#include "WorldConfig.hpp"
#include "WorldMap.hpp"
#include "ChunkGenerator.hpp"
#include "ChunkPipeline.hpp"
#include "BuiltinMaterials.hpp"
#include "BuiltinStyles.hpp"

int main() {
    std::cout << "MeshWorld — open-source procedural world exploration demo\n";
    std::cout << "Version: 0.1 (M5)\n\n";

    // §5 #17 fix: populate LuaGeneratorRegistry (dev mode from ".", or a
    // packed "meshworld_content.sqlite" if present) before any chunk gets
    // generated, so Lua generators actually have a chance to run instead of
    // silently falling back to C++ every time.
    MeshWorld::ContentPackLoader{}.load_auto(".", "meshworld_content.sqlite");

    // G11 fix (2026-07-11, procedural-model-generator-roadmap): this binary
    // calls ChunkPipeline::get() below, which can dispatch to real C++
    // generators (ParkGenerator/RoadGenerator/SmallHouseBlockGenerator) that
    // try StyleRegistry::instance().get(ctx.style) -- previously neither
    // registry was ever populated here, so those lookups always silently
    // fell back to hardcoded defaults, and MaterialRegistry-driven checks
    // (MC3Validator's material warning, WorldRenderer's texture injection)
    // had nothing to find either.
    MeshWorld::register_builtin_materials();
    MeshWorld::register_builtin_styles();

    MeshWorld::WorldConfig cfg;
    const bool loaded = cfg.load_from_file("examples/world.json");
    if (!loaded) {
        std::cout << "examples/world.json not found — using built-in defaults.\n\n";
    } else {
        std::cout << "Loaded examples/world.json\n\n";
    }

    std::cout << "World:  " << cfg.name         << "\n";
    std::cout << "Seed:   " << cfg.seed          << "\n";
    std::cout << "Grid:   " << cfg.grid_w << " x " << cfg.grid_h << " chunks\n";
    std::cout << "Chunk:  " << cfg.chunk_size_m  << " m\n";
    std::cout << "Style:  " << cfg.style         << "\n";
    std::cout << "Zones:  " << cfg.zones.size()  << " zone override(s)\n\n";

    MeshWorld::WorldMap      map(cfg);
    MeshWorld::ChunkPipeline pipeline(cfg, map);

    const std::pair<int,int> samples[] = {
        {0, 0}, {9, 9}, {2, 9}, {10, 10}, {0, 5}
    };

    std::cout << "Sample chunks:\n";
    for (auto [x, y] : samples) {
        const auto ci = map.info(x, y);
        std::cout << "  (" << x << "," << y << ")"
                  << "  zone="   << MeshWorld::to_string(ci.zone)
                  << "  region=" << MeshWorld::to_string(ci.region);
        if (ci.exits.north_road || ci.exits.south_road ||
            ci.exits.east_road  || ci.exits.west_road) {
            std::cout << "  road: N=" << ci.exits.north_road
                      << " S="        << ci.exits.south_road
                      << " E="        << ci.exits.east_road
                      << " W="        << ci.exits.west_road;
        }
        std::cout << "\n";

        auto mc3 = pipeline.get(x, y);
        std::cout << "    mc3: " << mc3.size() << " bytes\n";
    }

    std::cout << "\nDone.\n";
    return 0;
}
