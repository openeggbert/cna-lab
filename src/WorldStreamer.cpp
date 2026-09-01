// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "WorldStreamer.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <set>

#include "Map/MapPipeline.hpp"
#include "PlanetWorld.hpp"

namespace MeshWorld {

WorldStreamer::WorldStreamer(const WorldConfig& cfg,
                             const WorldMap&    map,
                             int                load_radius,
                             int                thread_count,
                             std::string        cache_dir,
                             std::string        map_world_dir,
                             Map::PlanetParams  map_params)
    : WorldStreamer(cfg, map, load_radius, thread_count, std::move(cache_dir),
                    std::move(map_world_dir), std::move(map_params), 0) {}

WorldStreamer::WorldStreamer(const WorldConfig& cfg,
                             const WorldMap&    map,
                             int                load_radius,
                             int                thread_count,
                             std::string        cache_dir,
                             std::string        map_world_dir,
                             Map::PlanetParams  map_params,
                             std::size_t        cache_max_entries)
    : cfg_(cfg), map_(map), load_radius_(load_radius), cache_dir_(std::move(cache_dir))
    , cache_max_entries_(cache_max_entries), map_world_dir_(std::move(map_world_dir))
    , map_params_(std::move(map_params))
{
    for (int i = 0; i < thread_count; ++i)
        workers_.emplace_back([this] { worker_loop(); });
}

WorldStreamer::~WorldStreamer() {
    shutdown();
}

void WorldStreamer::shutdown() {
    {
        std::lock_guard lk(queue_mutex_);
        stopping_ = true;
    }
    queue_cv_.notify_all();
    for (auto& t : workers_)
        if (t.joinable()) t.join();
    workers_.clear();
}

void WorldStreamer::set_chunk_loaded_callback(ChunkCallback cb) {
    std::lock_guard lk(cb_mutex_);
    chunk_loaded_cb_ = std::move(cb);
}

void WorldStreamer::update(float world_x, float world_z) {
    ChunkCoord center = ChunkCoord::from_world(world_x, world_z, cfg_.chunk_size_m);
    if (center == last_center_) return;
    last_center_ = center;

    // Build the desired set of chunks within radius.
    std::set<ChunkCoord> desired;
    for (int dy = -load_radius_; dy <= load_radius_; ++dy) {
        for (int dx = -load_radius_; dx <= load_radius_; ++dx) {
            if (dx * dx + dy * dy > load_radius_ * load_radius_) continue;
            ChunkCoord c{center.x + dx, center.y + dy};
            if (c.x < 0 || c.y < 0 || c.x >= cfg_.grid_w || c.y >= cfg_.grid_h) continue;
            desired.insert(c);
        }
    }

    // Unload chunks outside the desired set.
    {
        std::lock_guard lk(chunks_mutex_);
        std::vector<ChunkCoord> to_remove;
        for (auto& [coord, _] : chunks_)
            if (!desired.count(coord)) to_remove.push_back(coord);
        for (auto& c : to_remove) unload_chunk(c);
    }

    // Queue loads for chunks not already loaded or in-flight.
    for (const auto& c : desired) {
        std::lock_guard lk(chunks_mutex_);
        if (chunks_.count(c) || in_flight_.count(c)) continue;
        // Mark as in-flight placeholder.
        in_flight_[c]; // default-constructs invalid future; replaced by worker
        schedule_load(c);
    }
}

void WorldStreamer::schedule_load(const ChunkCoord& c) {
    // queue_mutex_ NOT held by caller; chunks_mutex_ IS held.
    std::lock_guard lk(queue_mutex_);
    job_queue_.push({c});
    queue_cv_.notify_one();
}

void WorldStreamer::unload_chunk(const ChunkCoord& c) {
    // Called with chunks_mutex_ held.
    chunks_.erase(c);
    in_flight_.erase(c);
}

void WorldStreamer::worker_loop() {
    // Each worker builds its own pipeline (ChunkPipeline is not thread-safe).
    ChunkCache    cache{cache_dir_, cache_max_entries_};

    // Each worker also builds its own PlanetWorld/MapPipeline, for the same
    // reason (see this class's constructor doc comment) — open_existing()
    // only reads the world.json + lazily opens per-level MapTileStores this
    // thread will itself use, never touched by another thread.
    std::optional<PlanetWorld>      map_world;
    std::optional<Map::MapPipeline> map_pipeline;
    if (!map_world_dir_.empty()) {
        map_world.emplace(PlanetWorld::open_existing(map_world_dir_));
        map_pipeline.emplace(*map_world, map_params_);
    }

    ChunkPipeline pipeline(cfg_, map_, std::move(cache),
                           map_pipeline ? &*map_pipeline : nullptr);

    while (true) {
        Job job;
        {
            std::unique_lock lk(queue_mutex_);
            queue_cv_.wait(lk, [this] { return !job_queue_.empty() || stopping_; });
            if (stopping_ && job_queue_.empty()) break;
            job = job_queue_.front();
            job_queue_.pop();
        }

        // Skip if chunk was unloaded while waiting in queue.
        {
            std::lock_guard lk(chunks_mutex_);
            if (!in_flight_.count(job.coord)) continue;
        }

        std::string xml = pipeline.get(job.coord);

        LoadedChunk lc{job.coord, std::move(xml), ChunkState::LOADED};

        ChunkCallback cb;
        {
            std::lock_guard lk(cb_mutex_);
            cb = chunk_loaded_cb_;
        }

        {
            std::lock_guard lk(chunks_mutex_);
            in_flight_.erase(job.coord);
            chunks_[job.coord] = lc;
        }

        if (cb) cb(lc);
    }
}

const LoadedChunk* WorldStreamer::chunk_at(const ChunkCoord& c) const {
    std::lock_guard lk(chunks_mutex_);
    auto it = chunks_.find(c);
    return it == chunks_.end() ? nullptr : &it->second;
}

int WorldStreamer::loaded_count() const {
    std::lock_guard lk(chunks_mutex_);
    return static_cast<int>(chunks_.size());
}

} // namespace MeshWorld
