// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <set>
#include <unordered_map>

#include "Map/MapTilePayload.hpp"
#include "Map/TileCoord.hpp"
#include "generators/map/PlanetGenerator.hpp"  // for PlanetParams (complete type)

// Forward declaration — PlanetWorld is in the outer MeshWorld namespace.
namespace MeshWorld { class PlanetWorld; }

namespace MeshWorld::Map {

// M104 — cache → load from DB → generate (parent-constrained) → store → return.
//
// MapPipeline is the entry point for MAP7 LOD descent. A single call to get()
// will recursively ensure every ancestor tile exists (loaded from DB or
// freshly generated) before generating the requested tile from its parent.
//
// Level 0 uses PlanetGenerator; deeper levels use ChildGenerator (M108/M109).
// M113/M218: a true in-memory LRU cache avoids repeated DB reads/regeneration
// during descent -- a cache hit refreshes the tile's recency (cache_touch()),
// so a tile kept "warm" by repeated access survives capacity pressure that
// would evict it under plain insertion-order (FIFO) eviction. (M113's own
// plan.md wording already called this "LRU"; M218 is what actually made a
// cache hit refresh recency -- before that this was FIFO in every way that
// matters, since eviction only ever consulted insertion order.)
class MapPipeline {
public:
    // Default cache capacity (number of tiles kept in memory) -- see the
    // constructor's `cache_capacity` parameter.
    static constexpr std::size_t kDefaultCacheCapacity = 256;

    // `params` is the PlanetParams used for level-0 generation and child
    // refinement. Typically built from PlanetWorld::config() at the call site.
    //
    // `named_levels` (M114): which quadtree levels are persisted to SQLite.
    // Default (std::nullopt) means every level is named — identical to this
    // class's behavior before M114. When set, get() only reads/writes
    // world_.tile_store() for levels in the set; other levels are always
    // regenerated from their nearest persisted (or in-memory cached) ancestor,
    // never touching disk for themselves. Generation is fully deterministic
    // given the world's persisted entropy, so this only trades disk usage for
    // recomputation cost on a cache miss — it does not change what get()
    // returns for any tile.
    //
    // `cache_capacity` (M218): overrides kDefaultCacheCapacity -- exposed
    // mainly so tests can exercise eviction/LRU behavior without generating
    // hundreds of real tiles; production call sites should leave this at
    // its default.
    MapPipeline(MeshWorld::PlanetWorld& world, const PlanetParams& params,
                std::optional<std::set<int>> named_levels = std::nullopt,
                std::size_t                   cache_capacity = kDefaultCacheCapacity);

    // Return the payload for `tile`. Load from cache or DB if available;
    // otherwise generate recursively (parent first) and persist.
    MapTilePayload get(const TileCoord& tile);

    // M219 — how many tiles are currently cache-resident. Mirrors
    // Model3DStreamer::open_shard_count()'s own role (M218): a plain
    // introspection getter, mainly useful for confirming cache_capacity_
    // is actually being honored (e.g. across a long traverse, M219).
    std::size_t cache_size() const { return cache_.size(); }

private:
    MapTilePayload generate_new(const TileCoord& tile, const MapTilePayload* parent,
                                std::uint64_t entropy);
    void cache_put(const TileCoord& tile, const MapTilePayload& payload);
    // M218 — marks `tile` most-recently-used: moves it to the back of
    // cache_order_ (front = next to evict). Called on every cache hit;
    // cache_put() already appends new entries at the back, so this is the
    // only piece LRU needed beyond the pre-existing FIFO structure.
    void cache_touch(const TileCoord& tile);
    bool is_named_level(int level) const;

    MeshWorld::PlanetWorld&       world_;
    PlanetParams                  params_;
    std::optional<std::set<int>> named_levels_;
    std::size_t                   cache_capacity_;

    std::unordered_map<TileCoord, MapTilePayload> cache_;
    std::deque<TileCoord>                         cache_order_;
};

} // namespace MeshWorld::Map
