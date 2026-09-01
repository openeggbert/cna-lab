// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "MapTileFetchQueue.hpp"

#include "Map/MapPipeline.hpp"
#include "PlanetWorld.hpp"

namespace MeshWorld {

MapTileFetchQueue::MapTileFetchQueue(std::string map_dir, Map::PlanetParams params)
    : map_dir_(std::move(map_dir)), params_(std::move(params))
{
    worker_ = std::thread([this] { worker_loop(); });
}

MapTileFetchQueue::~MapTileFetchQueue() {
    {
        std::lock_guard<std::mutex> lk(mutex_);
        stopping_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void MapTileFetchQueue::request(const Map::TileCoord& tile) {
    std::lock_guard<std::mutex> lk(mutex_);
    if (ready_.count(tile) || requested_.count(tile)) return;
    requested_.insert(tile);
    queue_.push(tile);
    cv_.notify_one();
}

bool MapTileFetchQueue::is_ready(const Map::TileCoord& tile) const {
    std::lock_guard<std::mutex> lk(mutex_);
    return ready_.count(tile) > 0;
}

bool MapTileFetchQueue::is_pending(const Map::TileCoord& tile) const {
    std::lock_guard<std::mutex> lk(mutex_);
    return requested_.count(tile) > 0 && ready_.count(tile) == 0;
}

void MapTileFetchQueue::cancel_unneeded(const std::vector<Map::TileCoord>& still_needed) {
    const std::set<Map::TileCoord> keep(still_needed.begin(), still_needed.end());
    std::lock_guard<std::mutex>    lk(mutex_);

    std::queue<Map::TileCoord> kept;
    while (!queue_.empty()) {
        const Map::TileCoord tile = queue_.front();
        queue_.pop();
        if (keep.count(tile) > 0) {
            kept.push(tile);
        } else {
            // Still purely queued (never popped by the worker) and no
            // longer needed -- drop it entirely, not just from the queue,
            // so a later request() for the same tile isn't silently
            // swallowed as "already requested".
            requested_.erase(tile);
        }
    }
    queue_ = std::move(kept);
}

void MapTileFetchQueue::worker_loop() {
    PlanetWorld       world = PlanetWorld::open_existing(map_dir_);
    Map::MapPipeline  pipeline(world, params_);

    while (true) {
        Map::TileCoord tile;
        {
            std::unique_lock<std::mutex> lk(mutex_);
            cv_.wait(lk, [this] { return !queue_.empty() || stopping_; });
            if (stopping_ && queue_.empty()) break;
            tile = queue_.front();
            queue_.pop();
        }

        pipeline.get(tile);  // generates (parent-constrained) + persists

        {
            std::lock_guard<std::mutex> lk(mutex_);
            ready_.insert(tile);
            requested_.erase(tile);
        }
    }
}

} // namespace MeshWorld
