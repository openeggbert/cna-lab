// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <functional>
#include <string>
#include "WorldConfig.hpp"
#include "WorldMap.hpp"
#include "ChunkGenerator.hpp"
#include "ChunkCache.hpp"
#include "ChunkDiagnostics.hpp"

// Forward-declared only: ChunkPipeline holds an optional, non-owning pointer.
// Keeps callers that don't use the map layer (main.cpp, WorldStreamer,
// export_chunks) free of a Map:: header dependency.
namespace MeshWorld::Map { class MapPipeline; }

namespace MeshWorld {

// Orchestrates chunk generation: cache-check → generate → store → return.
class ChunkPipeline {
public:
    // `map_pipeline`, when non-null, is queried at the hand-off level (MAP10,
    // M159) for the chunk's enclosing map tile to populate
    // ChunkContext.map_context. Null preserves the pre-map behavior exactly
    // (map_context.available stays false) — the flat WorldMap path is
    // untouched either way (M157/M160 will make generators read map_context).
    ChunkPipeline(const WorldConfig& cfg, const WorldMap& map, ChunkCache cache = ChunkCache{},
                  Map::MapPipeline* map_pipeline = nullptr);

    // Returns mc3.xml for the chunk at (x, y). Uses cache when available,
    // unless `force` is true (T237), which skips the cache-read and always
    // regenerates -- the freshly generated content still overwrites the
    // cache entry afterward (ChunkCache::store() always overwrites), so a
    // forced call also "repairs" a stale cache file for later plain get()s.
    std::string get(int x, int y, bool force = false);
    std::string get(const ChunkCoord& c, bool force = false) { return get(c.x, c.y, force); }

    // R108 -- same as get(x, y), but also fills `*out_diag` with which
    // generator/composer actually produced this call's content, the
    // fallback reason (never silent), and content stats. `out_diag` may
    // be null (equivalent to the plain get() above). A cache hit still
    // fills `*out_diag` (source == Cache) by re-reading the cached
    // content's own <metadata>, not by skipping diagnostics entirely.
    // `force` (T237) has the same meaning as on the plain get() above.
    std::string get(int x, int y, ChunkDiagnostics* out_diag, bool force = false);

    // Builds the ChunkContext for (x, y) without generating/caching XML —
    // the same context get() would hand to a generator. Exposed so tests can
    // verify map_context population directly (M163/M166).
    ChunkContext build_context(int x, int y) const;

    // T243 -- optional hook invoked once per get() call (cache hit or real
    // generation) right before it returns, reporting which coord was just
    // produced and where its content came from. Deliberately reports only
    // the per-call event, not a "N / total" count: different callers have
    // different notions of "total" (export_chunks.cpp's fixed grid loop vs.
    // WorldStreamer's on-demand per-connection requests vs. a future GUI),
    // so ChunkPipeline itself stays agnostic and lets the caller derive
    // whatever progress metric it needs from the event stream. Not
    // thread-safe on its own -- a future multi-threaded caller (T236) must
    // synchronize inside the callback body itself (e.g. a mutex around any
    // shared counter/stdout write); ChunkPipeline does not add locking here.
    using ProgressCallback = std::function<void(const ChunkCoord&, ChunkDiagnostics::Source)>;
    void set_progress_callback(ProgressCallback cb) { progress_callback_ = std::move(cb); }

private:
    const WorldConfig& cfg_;
    const WorldMap&    map_;
    ChunkCache         cache_;
    Map::MapPipeline*  map_pipeline_{nullptr};
    ProgressCallback   progress_callback_;
};

} // namespace MeshWorld
