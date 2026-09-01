// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "ChunkCoord.hpp"
#include "ModelPlacement.hpp"
#include "ModelPlacementStore.hpp"
#include "RegionShard.hpp"

namespace MeshWorld {

// M171 (MAP11) -- streams 3D model placements in/out based on the player's
// world position, genuinely in 3D (unlike WorldStreamer, which is 2D:
// horizontal chunk radius only). Mirrors WorldStreamer's own
// construct/update()/query/shutdown() shape closely.
//
// Unlike WorldStreamer's LoadedChunk (naturally one-per-chunk-coord), loaded
// placements are exposed as a flat vector spanning many chunks/regions at
// once -- the same shape ModelPlacementStore::query_box() already returns
// (M031), not an artificial per-chunk grouping.

// M173 -- newly-loaded/newly-unloaded delta since the previous recompute.
// ModelPlacement has no stable row identity (M167), so membership is
// determined by ModelPlacement::operator==() (value equality), not a
// synthetic id.
struct PlacementDelta {
    std::vector<ModelPlacement> added;
    std::vector<ModelPlacement> removed;
};

class Model3DStreamer {
public:
    using DeltaCallback = std::function<void(const PlacementDelta&)>;

    // M218 (MAP14) — default cap on how many region shards (ModelPlacementStore
    // instances, each an open SQLite connection + WAL file) stay resident in
    // store_pool_ at once. Far smaller than MapPipeline::kDefaultCacheCapacity's
    // 256: each entry here is a whole open DB connection, not an in-memory
    // struct, and a player's proximity box only ever spans a handful of
    // regions at a time near a boundary (kRegionBlockChunks = 64 chunks/side,
    // RegionShard.hpp) -- 32 gives generous headroom over normal usage while
    // still bounding growth over a long traverse across many regions.
    static constexpr std::size_t kDefaultStorePoolCapacity = 32;

    // horizontal_radius_chunks: R_h, reuses WorldStreamer's own load_radius
    // concept directly (a chunk count, not a separate unit). vertical_radius_m:
    // R_v in meters -- the natural unit at this API boundary; converted to an
    // alt_band range via alt_band_for() only at query time (M172), not stored
    // as a band count here. chunk_size_m: needed to convert a world position
    // into a ChunkCoord (M172); defaults to 64, matching
    // WorldConfig::chunk_size_m's own default. thread_count spawns that many
    // background workers (M174), though only one is ever actively recomputing
    // at a time (see update()'s own doc comment) -- extra workers simply idle,
    // an honest simplification given there is only one "latest position" to
    // process, not independent parallel jobs the way WorldStreamer's
    // per-chunk loads are. store_pool_capacity (M218): overrides
    // kDefaultStorePoolCapacity -- exposed mainly so tests can exercise
    // shard eviction without opening dozens of real region DBs; production
    // call sites should leave this at its default.
    Model3DStreamer(std::string world_dir, int horizontal_radius_chunks,
                     double vertical_radius_m, int thread_count = 2, int chunk_size_m = 64,
                     std::size_t store_pool_capacity = kDefaultStorePoolCapacity);
    ~Model3DStreamer();

    Model3DStreamer(const Model3DStreamer&)            = delete;
    Model3DStreamer& operator=(const Model3DStreamer&) = delete;

    // Update the player's world position (meters, all 3 axes -- the axis
    // WorldStreamer's own 2D update(world_x, world_z) doesn't have).
    // Non-blocking (M174): records the latest desired position and wakes a
    // background worker; the actual query/diff happens off-thread, mirroring
    // WorldStreamer's own update() (which only queues work, real results
    // surface later via chunk_at()/set_chunk_loaded_callback()). A rapid
    // sequence of update() calls before any worker claims the previous one
    // simply overwrites the pending position -- coalescing to the LATEST
    // position only, the same spirit as WorldStreamer's own
    // `if (center == last_center_) return;` early-out, just implemented as
    // "last write wins" on a single pending slot instead.
    void update(double world_x, double world_y, double world_z);

    // Invoked (from a background worker thread, NOT the calling thread) each
    // time a recompute finishes, with exactly what changed. Mirrors
    // WorldStreamer::set_chunk_loaded_callback() exactly.
    void set_delta_callback(DeltaCallback cb);

    // All placements currently loaded, flat across every region/chunk within
    // the player's 3D proximity box. Thread-safe synchronous snapshot read.
    std::vector<ModelPlacement> loaded_placements() const;

    // M218 — how many region shards are currently open. Mirrors
    // WorldStreamer::loaded_count()'s own role: a plain introspection
    // getter, not a debug-only hook, mainly useful for confirming
    // store_pool_capacity_ is actually being honored.
    std::size_t open_shard_count() const;

    // Flush all pending work and join all threads. Called by destructor.
    void shutdown();

private:
    std::string world_dir_;
    int         horizontal_radius_chunks_;
    double      vertical_radius_m_;
    int         thread_count_;
    int         chunk_size_m_;
    std::size_t store_pool_capacity_;

    // Lazily gets-or-creates the ModelPlacementStore for `region` from
    // store_pool_. Caller must hold store_pool_mutex_.
    ModelPlacementStore& store_for(const RegionId& region);

    // Computes the full multi-region query + value diff for one position,
    // updates loaded_, and returns the delta. Called from worker_loop() only
    // (does real SQLite work -- never call this while holding queue_mutex_).
    PlacementDelta recompute(double world_x, double world_y, double world_z);

    void worker_loop();

    // Lazily-opened ModelPlacementStore per region -- a player's 3D proximity
    // box can span multiple regions near a region boundary, so this is a
    // pool, not a single store. This is the "real caching" M169's own
    // ModelPlacementWriter design notes explicitly deferred to here.
    //
    // M218 — bounded by store_pool_capacity_ via store_order_ (LRU order,
    // front = least-recently-used): store_for() touches an entry's position
    // on every access and evicts (closes) the LRU entry before opening a
    // new one past capacity, same shape as MapPipeline's tile cache.
    mutable std::mutex                                                store_pool_mutex_;
    std::unordered_map<RegionId, std::unique_ptr<ModelPlacementStore>> store_pool_;
    std::deque<RegionId>                                              store_order_;

    mutable std::mutex          loaded_mutex_;
    std::vector<ModelPlacement> loaded_;

    // M174/R140 -- background thread pool. A single pending-position "slot"
    // (not a queue): update() overwrites it and wakes one worker; whichever
    // worker wakes first claims it (clearing has_pending_) and processes the
    // latest query extent. `last_requested_extent_` skips repeated per-frame
    // SQLite work while the player remains in the same horizontal chunk and
    // altitude-band window; per-placement distance filtering still happens
    // every frame in WorldRenderer.
    struct QueryExtent {
        ChunkCoord center;
        int        alt_band_min{0};
        int        alt_band_max{0};

        bool operator==(const QueryExtent& other) const {
            return center == other.center && alt_band_min == other.alt_band_min &&
                   alt_band_max == other.alt_band_max;
        }
    };
    std::vector<std::thread> workers_;
    std::mutex               queue_mutex_;
    std::condition_variable  queue_cv_;
    bool                     stopping_{false};
    bool                     has_pending_{false};
    double                   pending_x_{0.0};
    double                   pending_y_{0.0};
    double                   pending_z_{0.0};
    std::optional<QueryExtent> last_requested_extent_;

    std::mutex    callback_mutex_;
    DeltaCallback delta_callback_;
};

} // namespace MeshWorld
