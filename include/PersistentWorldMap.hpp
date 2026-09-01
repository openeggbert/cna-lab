// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include "WorldMap.hpp"
#include "ChunkCoord.hpp"
#include <sqlite3.h>
#include <string>
#include <unordered_set>
#include <cstdint>

namespace MeshWorld {

// SQLite-backed world map — no fixed seed, non-reproducible, expandable.
// Chunk zone/region is generated using time-based entropy on first visit and
// stored permanently in the database.  Subsequent visits load from the DB.
// Multi-level: each level uses a separate DB file (map_level{N}.db).
//
// M161 — LEGACY / conceptually superseded by Map::MapPipeline's own
// planet -> chunk hand-off (MAP10, M157-M160), which is the intended
// long-term source of zone/region/elevation data. NOT actively deprecated
// (no functional change, no [[deprecated]] attribute) because it is
// currently the ONLY thing driving real chunk generation in the one live
// app that uses it: `apps/mesh-world-app/main.cpp` constructs this
// directly, and `WorldStreamer`'s own internal `ChunkPipeline` never
// receives a `Map::MapPipeline*` (see NEXT.md's own "map layer not wired
// into MeshWorldApp" tracked item — a separate, larger, non-MAP10 task).
// Removing or bypassing this class before that wiring exists would leave
// the interactive app with no chunk-generation path at all. Existing
// saves (an already-populated `map_level{N}.db`) MUST keep loading
// correctly regardless of any future change here — see
// `tests/PersistentWorldMapTests.cpp` (added by M161; no coverage existed
// before it) for the concrete round-trip proof plan.md's own "don't break
// existing saves" wording otherwise has nothing to stand on.
class PersistentWorldMap {
public:
    // world_dir: directory where DB and chunks live, e.g. "saves/world1"
    PersistentWorldMap(const std::string& world_dir, WorldMap& map, int level = 0);
    ~PersistentWorldMap();

    // Ensure (x,y) is populated in WorldMap (load from DB or generate).
    // Returns the chunk's ChunkInfo.
    ChunkInfo ensure_chunk(int x, int y);

    // ensure_chunk for every cell within circular radius of (cx,cy).
    void ensure_region(int cx, int cy, int radius);

private:
    WorldMap&     map_;
    sqlite3*      db_{nullptr};
    uint64_t      session_entropy_{0}; // differs each session — maps are non-reproducible

    // Tracks cells already written into WorldMap to skip redundant DB queries.
    std::unordered_set<ChunkCoord> populated_;

    bool      db_load(int x, int y, ChunkInfo& out);
    void      db_store(int x, int y, const ChunkInfo& info);
    ChunkInfo generate_new(int x, int y) const;

    static RegionType default_region_for_zone(ZoneType z, int x, int y, std::uint64_t variation);
};

} // namespace MeshWorld
