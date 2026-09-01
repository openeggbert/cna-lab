// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "Map/TileCoord.hpp"
#include "generators/map/PlanetGenerator.hpp"  // Map::PlanetParams

namespace MeshWorld {

// M192-193 (MAP12) — background generation queue for the zoomable map view:
// when MapView wants to show a tile that isn't yet generated/persisted,
// request() queues it for a dedicated background thread to generate,
// instead of blocking the UI thread on Map::MapPipeline::get().
//
// The worker thread owns its own PlanetWorld/Map::MapPipeline, opened once
// on construction — mirrors WorldStreamer's own "each worker thread owns
// its own PlanetWorld/MapPipeline" shape (see WorldStreamer.hpp's own doc
// comment): neither PlanetWorld nor MapPipeline is safe to share across
// threads (both hold unsynchronized mutable state — MapPipeline's in-memory
// tile cache, PlanetWorld's lazily-opened MapTileStore map), so the caller's
// own PlanetWorld/MapPipeline (used only for reads, e.g. MapTileStore::
// has()/load()) stays completely separate. Independent connections to the
// same on-disk map_level{z}.db files are safe (MapTileStore already opens
// them in WAL mode with a busy timeout, see its own doc comment).
//
// The caller keeps reading tile data through its own PlanetWorld/
// MapTileStore exactly as before this class existed (e.g. draw_map_view()'s
// own store.has()/load() calls) — once the background thread persists a
// tile, has() naturally starts returning true for it on the next poll, no
// extra plumbing needed. is_ready()/is_pending() exist only so a caller can
// show a "generating…" placeholder while waiting.
class MapTileFetchQueue {
public:
    // `map_dir` must already contain a PlanetWorld (created via
    // PlanetWorld::create_new()) — same precondition WorldStreamer's own
    // map_world_dir constructor parameter documents.
    MapTileFetchQueue(std::string map_dir, Map::PlanetParams params);
    ~MapTileFetchQueue();

    MapTileFetchQueue(const MapTileFetchQueue&)            = delete;
    MapTileFetchQueue& operator=(const MapTileFetchQueue&) = delete;

    // Queues `tile` for background generation unless it's already
    // ready or already queued/in-flight. Non-blocking, safe to call
    // every frame for the same tile (a no-op once queued).
    void request(const Map::TileCoord& tile);

    // True once the background thread has finished generating/persisting
    // `tile` (only tracks tiles requested through this queue — a tile
    // already persisted from an earlier session is not "ready" here until
    // request()ed at least once; callers should check their own
    // MapTileStore::has() first, same as before this class existed).
    bool is_ready(const Map::TileCoord& tile) const;

    // True while `tile` is queued or currently being generated.
    bool is_pending(const Map::TileCoord& tile) const;

    // M217 (MAP14) — drops every request that is still purely queued (the
    // worker hasn't popped it yet) and is not in `still_needed`, e.g. a
    // tile that was visible last frame but panned/zoomed away from before
    // generation even started. A tile the worker has already popped and
    // started generating always runs to completion and persists normally
    // -- interrupting a MapTilePayload mid-generation/mid-persist would be
    // a much larger, riskier change than this queue can safely make on its
    // own, so this only prevents *starting* wasted work, not stopping work
    // already in flight. Safe to call every frame with the current visible
    // tile set; a no-op if every queued tile is still needed.
    void cancel_unneeded(const std::vector<Map::TileCoord>& still_needed);

private:
    void worker_loop();

    std::string       map_dir_;
    Map::PlanetParams params_;

    std::thread              worker_;
    mutable std::mutex       mutex_;
    std::condition_variable  cv_;
    std::queue<Map::TileCoord> queue_;
    std::set<Map::TileCoord>   requested_;  // queued or in-flight, not yet ready
    std::set<Map::TileCoord>   ready_;
    bool stopping_{false};
};

} // namespace MeshWorld
