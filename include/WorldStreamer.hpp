// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "ChunkCoord.hpp"
#include "ChunkPipeline.hpp"
#include "WorldConfig.hpp"
#include "WorldMap.hpp"
#include "generators/map/PlanetGenerator.hpp"  // for Map::PlanetParams (complete type)

namespace MeshWorld {

// Chunk state visible to callers.
enum class ChunkState { PENDING, LOADED, UNLOADED };

struct LoadedChunk {
    ChunkCoord  coord;
    std::string xml;         // MC3 XML content
    ChunkState  state{ChunkState::LOADED};
};

// Streams chunks in/out based on the player's world position.
// Background thread pool generates chunks; caller polls via chunk_at() or callback.
class WorldStreamer {
public:
    using ChunkCallback = std::function<void(const LoadedChunk&)>;

    // `map_world_dir`, when non-empty, attaches the planetary map layer
    // (Map::MapPipeline) to every generated chunk's ChunkContext.map_context —
    // the same optional/additive hand-off ChunkPipeline already supports
    // (see its own doc comment), just reachable from the streaming path too.
    // The directory must already contain a PlanetWorld (created via
    // PlanetWorld::create_new()) before this constructor runs — worker
    // threads only ever open_existing() it, never create it, to avoid a
    // create-vs-create race between threads on first launch. Each worker
    // thread opens its own PlanetWorld/MapPipeline (mirrors why each worker
    // already builds its own ChunkCache/ChunkPipeline): MapPipeline's
    // in-memory tile cache and PlanetWorld's lazily-opened MapTileStore map
    // are not synchronized, so sharing either across threads would race.
    // Independent connections to the same on-disk map_level{z}.db files are
    // safe (MapTileStore already opens them in WAL mode). Empty (default)
    // disables the map layer entirely, preserving prior behavior exactly.
    explicit WorldStreamer(const WorldConfig& cfg,
                           const WorldMap&    map,
                           int                load_radius   = 3,
                           int                thread_count  = 2,
                           std::string        cache_dir     = "cache/chunks",
                           std::string        map_world_dir = "",
                           Map::PlanetParams  map_params    = {});
    WorldStreamer(const WorldConfig& cfg,
                           const WorldMap&    map,
                           int                load_radius,
                           int                thread_count,
                           std::string        cache_dir,
                           std::string        map_world_dir,
                           Map::PlanetParams  map_params,
                           std::size_t        cache_max_entries);
    ~WorldStreamer();

    // Update the player position (world-space meters). Queues loads and unloads.
    void update(float world_x, float world_z);

    // Returns nullptr if the chunk is not yet loaded.
    const LoadedChunk* chunk_at(const ChunkCoord& c) const;

    // Optional callback invoked (on a background thread) when a chunk finishes loading.
    void set_chunk_loaded_callback(ChunkCallback cb);

    // Flush all pending work and join all threads. Called by destructor.
    void shutdown();

    int loaded_count() const;

private:
    struct Job { ChunkCoord coord; };

    void worker_loop();
    void schedule_load(const ChunkCoord& c);
    void unload_chunk(const ChunkCoord& c);

    const WorldConfig& cfg_;
    const WorldMap&    map_;
    int                load_radius_;
    std::string        cache_dir_;
    std::size_t        cache_max_entries_{0};
    std::string        map_world_dir_;
    Map::PlanetParams  map_params_;

    // Thread pool
    std::vector<std::thread>        workers_;
    std::queue<Job>                 job_queue_;
    mutable std::mutex              queue_mutex_;
    std::condition_variable         queue_cv_;
    bool                            stopping_{false};

    // Chunk map
    mutable std::mutex                                      chunks_mutex_;
    std::unordered_map<ChunkCoord, LoadedChunk>             chunks_;
    std::unordered_map<ChunkCoord, std::future<void>>       in_flight_;

    ChunkCallback chunk_loaded_cb_;
    std::mutex    cb_mutex_;

    ChunkCoord    last_center_{-9999, -9999};
};

} // namespace MeshWorld
