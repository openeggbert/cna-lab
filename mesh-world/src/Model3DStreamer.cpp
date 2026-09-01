// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Model3DStreamer.hpp"

#include <algorithm>

namespace MeshWorld {

Model3DStreamer::Model3DStreamer(std::string world_dir, int horizontal_radius_chunks,
                                  double vertical_radius_m, int thread_count, int chunk_size_m,
                                  std::size_t store_pool_capacity)
    : world_dir_(std::move(world_dir)),
      horizontal_radius_chunks_(horizontal_radius_chunks),
      vertical_radius_m_(vertical_radius_m),
      thread_count_(thread_count),
      chunk_size_m_(chunk_size_m),
      store_pool_capacity_(store_pool_capacity) {
    for (int i = 0; i < thread_count_; ++i) workers_.emplace_back([this] { worker_loop(); });
}

Model3DStreamer::~Model3DStreamer() {
    shutdown();
}

void Model3DStreamer::shutdown() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stopping_ = true;
    }
    queue_cv_.notify_all();
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
    workers_.clear();
}

void Model3DStreamer::set_delta_callback(DeltaCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    delta_callback_ = std::move(cb);
}

std::size_t Model3DStreamer::open_shard_count() const {
    std::lock_guard<std::mutex> lock(store_pool_mutex_);
    return store_pool_.size();
}

ModelPlacementStore& Model3DStreamer::store_for(const RegionId& region) {
    auto it = store_pool_.find(region);
    if (it != store_pool_.end()) {
        // M218 — touch: move to the back (most-recently-used end) of
        // store_order_ so this region survives the next eviction.
        const auto order_it = std::find(store_order_.begin(), store_order_.end(), region);
        if (order_it != store_order_.end()) store_order_.erase(order_it);
        store_order_.push_back(region);
        return *it->second;
    }

    // M218 — evict the least-recently-used shard (closes its SQLite handle
    // via ~ModelPlacementStore()) before opening one more past capacity.
    if (store_pool_.size() >= store_pool_capacity_ && !store_order_.empty()) {
        store_pool_.erase(store_order_.front());
        store_order_.pop_front();
    }

    store_order_.push_back(region);
    return *store_pool_.emplace(region, std::make_unique<ModelPlacementStore>(world_dir_, region))
                .first->second;
}

namespace {

// M173 -- mark-and-sweep value diff: for each entry in `updated`, find an
// unmatched equal entry in `previous`; anything left unmatched in `previous`
// is `removed`, anything left unmatched in `updated` is `added`.
PlacementDelta diff_placements(const std::vector<ModelPlacement>& previous,
                                const std::vector<ModelPlacement>& updated) {
    std::vector<bool> matched_old(previous.size(), false);
    std::vector<bool> matched_new(updated.size(), false);

    for (std::size_t ni = 0; ni < updated.size(); ++ni) {
        for (std::size_t oi = 0; oi < previous.size(); ++oi) {
            if (!matched_old[oi] && previous[oi] == updated[ni]) {
                matched_old[oi] = true;
                matched_new[ni] = true;
                break;
            }
        }
    }

    PlacementDelta delta;
    for (std::size_t oi = 0; oi < previous.size(); ++oi) {
        if (!matched_old[oi]) delta.removed.push_back(previous[oi]);
    }
    for (std::size_t ni = 0; ni < updated.size(); ++ni) {
        if (!matched_new[ni]) delta.added.push_back(updated[ni]);
    }
    return delta;
}

} // namespace

PlacementDelta Model3DStreamer::recompute(double world_x, double world_y, double world_z) {
    const ChunkCoord center = ChunkCoord::from_world(static_cast<float>(world_x),
                                                      static_cast<float>(world_z), chunk_size_m_);
    const ChunkCoord chunk_min{center.x - horizontal_radius_chunks_,
                                center.y - horizontal_radius_chunks_};
    const ChunkCoord chunk_max{center.x + horizontal_radius_chunks_,
                                center.y + horizontal_radius_chunks_};
    const int alt_band_min = alt_band_for(world_y - vertical_radius_m_);
    const int alt_band_max = alt_band_for(world_y + vertical_radius_m_);

    // A player's 3D proximity box can straddle a region boundary -- find
    // EVERY region the chunk box spans, not just the center's own region.
    const RegionId region_min = region_for_chunk(chunk_min);
    const RegionId region_max = region_for_chunk(chunk_max);

    std::vector<ModelPlacement> result;
    {
        std::lock_guard<std::mutex> pool_lock(store_pool_mutex_);
        for (int rx = region_min.rx; rx <= region_max.rx; ++rx) {
            for (int rz = region_min.rz; rz <= region_max.rz; ++rz) {
                ModelPlacementStore& store = store_for(RegionId{rx, rz});
                auto placements = store.query_box(chunk_min, chunk_max, alt_band_min, alt_band_max);
                result.insert(result.end(), std::make_move_iterator(placements.begin()),
                              std::make_move_iterator(placements.end()));
            }
        }
    }

    std::lock_guard<std::mutex> loaded_lock(loaded_mutex_);
    PlacementDelta delta = diff_placements(loaded_, result);
    loaded_ = std::move(result);
    return delta;
}

void Model3DStreamer::update(double world_x, double world_y, double world_z) {
    const QueryExtent extent{
        ChunkCoord::from_world(static_cast<float>(world_x), static_cast<float>(world_z), chunk_size_m_),
        alt_band_for(world_y - vertical_radius_m_),
        alt_band_for(world_y + vertical_radius_m_)
    };
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (last_requested_extent_ && *last_requested_extent_ == extent) return;
        last_requested_extent_ = extent;
        pending_x_ = world_x;
        pending_y_ = world_y;
        pending_z_ = world_z;
        has_pending_ = true;
    }
    queue_cv_.notify_one();
}

void Model3DStreamer::worker_loop() {
    while (true) {
        double x, y, z;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] { return has_pending_ || stopping_; });
            if (stopping_ && !has_pending_) return;
            x = pending_x_;
            y = pending_y_;
            z = pending_z_;
            has_pending_ = false; // claimed -- a later update() will set this again
        }

        PlacementDelta delta = recompute(x, y, z);

        DeltaCallback cb;
        {
            std::lock_guard<std::mutex> cb_lock(callback_mutex_);
            cb = delta_callback_;
        }
        if (cb) cb(delta);
    }
}

std::vector<ModelPlacement> Model3DStreamer::loaded_placements() const {
    std::lock_guard<std::mutex> lock(loaded_mutex_);
    return loaded_;
}

} // namespace MeshWorld
