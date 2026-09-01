// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include "ChunkCoord.hpp"
#include "ModelPlacement.hpp"
#include "RegionShard.hpp"

struct sqlite3;

namespace MeshWorld {

// Altitude band size in meters (map.md §10.1's own example: "e.g. 64 m
// bands"). A placement's alt_band = floor(pos_y / kAltBandM) -- the vertical
// analogue of chunk_x/chunk_z, letting query_box() cull far-above/far-below
// placements without reading them.
constexpr double kAltBandM = 64.0;

inline int alt_band_for(double pos_y) {
    return static_cast<int>(std::floor(pos_y / kAltBandM));
}

// M027 (MAP2, brought forward as a MAP11 M169 prerequisite -- see NEXT.md §8)
// -- region-sharded SQLite store for 3D model placements. One DB file per
// region: <world_dir>/models/<rx>_<rz>.db (RegionId::shard_path()). Mirrors
// Map::MapTileStore's open-or-create + WAL lifecycle (M018/M019), adapted
// for placements instead of map tiles.
//
// A store instance is scoped to exactly one region; placements whose chunk
// falls outside that region are a programming error (the caller is
// responsible for routing a chunk's placements to the right store via
// region_for_chunk() before ever constructing/using one -- ModelPlacementStore
// itself does not re-derive or re-check this, mirroring MapTileStore's own
// "the level is implied by the DB file" convention).
class ModelPlacementStore {
public:
    ModelPlacementStore(const std::string& world_dir, const RegionId& region);
    ~ModelPlacementStore();

    ModelPlacementStore(const ModelPlacementStore&)            = delete;
    ModelPlacementStore& operator=(const ModelPlacementStore&) = delete;

    const RegionId&    region() const { return region_; }
    const std::string& path()   const { return path_; }
    bool                is_open() const { return db_ != nullptr; }

    // M030 -- insert every placement for one chunk, in a single transaction.
    // chunk_x/chunk_z (the schema's own indexing columns, map.md §10.1) are
    // derived from `chunk` here, not carried on ModelPlacement itself.
    void insert_batch(const ChunkCoord& chunk, const std::vector<ModelPlacement>& placements);

    // M031 -- 3D proximity query: placements whose chunk_x/chunk_z fall
    // within [chunk_min, chunk_max] (inclusive, both axes) AND whose
    // alt_band falls within [alt_band_min, alt_band_max] (inclusive).
    std::vector<ModelPlacement> query_box(const ChunkCoord& chunk_min, const ChunkCoord& chunk_max,
                                           int alt_band_min, int alt_band_max) const;

    // M178 (optional, plan.md's own wording) -- an optional compiled chunk
    // scene BLOB (MC3/MCB), keyed by chunk, stored in the same region DB
    // (map.md section 10.1's own summary table). Lets a caller prefer a
    // cached compiled scene over re-running the chunk generator. Separate
    // table from `placement` -- a chunk blob is a cached rendering artifact,
    // not placement data, and a chunk may have zero placements but still
    // want a cached blob (or vice versa). Upsert semantics, mirroring
    // MapTileStore::store()'s own ON CONFLICT pattern.
    void store_chunk_blob(const ChunkCoord& chunk, const std::string& blob);

    // Returns std::nullopt if no blob has ever been stored for this chunk.
    std::optional<std::string> load_chunk_blob(const ChunkCoord& chunk) const;

private:
    RegionId    region_;
    std::string path_;
    sqlite3*    db_{nullptr};
};

} // namespace MeshWorld
