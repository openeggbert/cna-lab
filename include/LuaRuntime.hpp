// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <memory>
#include <string>

namespace MeshWorld {

class  Mc3SceneBuilder;
class  MapBuilder;
struct ChunkContext;

namespace Map { struct MapTilePayload; }

// Executes a single Lua generator script in a sandboxed sol::state.
// sol2/Lua headers are kept out of this public header via pimpl.
//
// A fresh LuaRuntime must be constructed for every generator invocation to
// prevent state leaking between runs.
//
// Two modes, selected by which constructor is used — both expect the Lua
// module to return a table M with a function M.generate(ctx, X):
//   - Chunk generators (existing): X is an Mc3SceneBuilder userdata bound to
//     the global `scene`; ctx is a table of chunk fields (ChunkContext).
//   - Map generators (MAP6, M090): X is a MapBuilder userdata bound to the
//     global `map`. This mode also exposes a `names` table (M091:
//     culture/continent/country/city/river/mountain/street) and a `ctx`
//     table (M092/M093): level/tile_x/tile_y/tile_size_m/variation always
//     present; parent/edges present only when `parent` is non-null (nil at
//     the level-0 planet root, which has no parent); ctx.noise(x,y[,octaves,
//     lacunarity,gain]) and ctx.random()/ctx.randomInt(lo,hi), both seeded
//     from the tile's entropy for determinism (map.md §8).
class LuaRuntime {
public:
    LuaRuntime(Mc3SceneBuilder& scene, const ChunkContext& ctx);
    explicit LuaRuntime(MapBuilder& map, const Map::MapTilePayload* parent = nullptr);
    ~LuaRuntime();

    // Load source, call M.generate(ctx, scene|map) per the mode this
    // LuaRuntime was constructed with.
    // Returns "" on success, error description on failure.
    std::string run(const std::string& source);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace MeshWorld
