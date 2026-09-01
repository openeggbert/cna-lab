// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cstdint>
#include <string>

#include "Map/MapTilePayload.hpp"
#include "Map/TileCoord.hpp"

namespace MeshWorld {

struct ChunkContext;

// Inputs the map-generator execute() path needs beyond what
// Map::MapGenerator::generate() itself takes (tile, parent, entropy): just
// sea_level_m, needed by MapBuilder::setBiomeField for biome classification
// (mirrors Map::PlanetParams::sea_level_m). parent is null at level 0 (the
// planet root has no parent).
struct MapGenContext {
    Map::TileCoord             tile;
    std::uint64_t              entropy{0};
    double                     sea_level_m{0.0};
    const Map::MapTilePayload* parent{nullptr};
};

// High-level Lua generator sandbox.
// Creates an Mc3SceneBuilder from the ChunkContext, runs the Lua generator
// script via LuaRuntime, and returns the resulting MC3 XML string.
//
// On error: returns empty string and writes a description to *error_out
// (if error_out is non-null). Never throws or crashes the C++ process.
class LuaSandbox {
public:
    std::string execute(const std::string& source,
                        const ChunkContext& ctx,
                        std::string*        error_out = nullptr);

    // Map-generator mode (MAP6, M094). Builds a MapBuilder from mapctx, runs
    // the Lua generator script via LuaRuntime (same sandbox guarantees:
    // io/os/debug/package/require blocked), and returns the resulting
    // MapTilePayload.
    //
    // On error: returns a default-constructed MapTilePayload — callers must
    // check *error_out (if non-null) to detect failure, exactly as execute()
    // callers must check for an empty string. Never throws or crashes the
    // C++ process.
    Map::MapTilePayload executeMap(const std::string&   source,
                                   const MapGenContext&  mapctx,
                                   std::string*          error_out = nullptr);
};

} // namespace MeshWorld
