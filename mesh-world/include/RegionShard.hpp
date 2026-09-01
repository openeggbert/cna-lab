// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "ChunkCoord.hpp"

namespace MeshWorld {

// M026 (MAP2, brought forward as a MAP11 M169 prerequisite -- see NEXT.md §8) --
// maps a global ChunkCoord to the region it shards into for 3D model
// placement storage (map.md §10.1): "shard by region (block of chunks) ...
// models/<rx>_<rz>.db". Region size is a fixed N x N block of chunks
// (kRegionBlockChunks), NOT tied to the Map:: quadtree's own level sizing --
// this keeps RegionShard decoupled from Map:: headers, mirroring
// ChunkContext.map_context's own decoupling precedent.
constexpr int32_t kRegionBlockChunks = 64;  // 64 chunks/side * 64 m/chunk ~= 4.1 km/region

// Floor division (not C++'s truncating a/b), so negative chunk coordinates
// route to the correct region rather than off-by-one toward zero -- e.g.
// chunk x=-1 with a 64-chunk block must land in region rx=-1 (the block
// covering [-64, -1]), not rx=0.
constexpr int32_t floor_div(int32_t a, int32_t b) {
    const int32_t q = a / b;
    const int32_t r = a % b;
    return (r != 0 && ((r < 0) != (b < 0))) ? q - 1 : q;
}

struct RegionId {
    int32_t rx{0};
    int32_t rz{0};

    constexpr RegionId() = default;
    constexpr RegionId(int32_t rx_, int32_t rz_) : rx(rx_), rz(rz_) {}

    // Shard file path, matching map.md §10.1's own worked example exactly:
    // "<world_dir>/models/<rx>_<rz>.db".
    std::string shard_path(const std::string& world_dir) const {
        return world_dir + "/models/" + std::to_string(rx) + "_" + std::to_string(rz) + ".db";
    }

    constexpr bool operator==(const RegionId& o) const { return rx == o.rx && rz == o.rz; }
    constexpr bool operator!=(const RegionId& o) const { return !(*this == o); }
    constexpr bool operator<(const RegionId& o) const {
        return rx < o.rx || (rx == o.rx && rz < o.rz);
    }
};

// Which region a chunk's placements shard into.
constexpr RegionId region_for_chunk(const ChunkCoord& c, int32_t block_chunks = kRegionBlockChunks) {
    return RegionId{floor_div(c.x, block_chunks), floor_div(c.y, block_chunks)};
}

}  // namespace MeshWorld

// std::hash support for use in unordered_map (mirrors ChunkCoord's own).
template <>
struct std::hash<MeshWorld::RegionId> {
    std::size_t operator()(const MeshWorld::RegionId& r) const noexcept {
        return std::hash<int64_t>{}(
            (static_cast<int64_t>(r.rx) << 32) | static_cast<uint32_t>(r.rz));
    }
};
