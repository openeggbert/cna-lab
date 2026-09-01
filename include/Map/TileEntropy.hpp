// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cstdint>

#include "Map/TileCoord.hpp"

namespace MeshWorld::Map {

// Per-tile entropy derived from the world's entropy and the tile address.
// Generalizes ChunkCoord's chunk_seed() to the 3-coordinate (level, x, y) tile
// space. Seeds a tile's generator deterministically *within a world* so a
// revisit is consistent; the world entropy itself is non-reproducible /
// time-based (see map.md §8), so different worlds diverge.
//
// x,y are mixed as full 64-bit (no 32-bit truncation) since deep-level indices
// can be large. Finalizer is the Murmur3 64-bit mixer.
inline uint64_t tile_entropy(uint64_t world_entropy, int level, int64_t x, int64_t y) {
    uint64_t h = world_entropy;
    h ^= static_cast<uint64_t>(static_cast<uint32_t>(level)) * 0x9e3779b97f4a7c15ULL;
    h += 0x6c62272e07bb0142ULL;
    h ^= static_cast<uint64_t>(x) * 0x517cc1b727220a95ULL;
    h += 0x2e7b5b82d8f8d8d7ULL;
    h ^= static_cast<uint64_t>(y) * 0x9e3779b97f4a7c15ULL;
    h += 0x14057b7ef767814fULL;
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;
    return h;
}

// Convenience overload taking a TileCoord.
inline uint64_t tile_entropy(uint64_t world_entropy, const TileCoord& t) {
    return tile_entropy(world_entropy, t.level, t.x, t.y);
}

} // namespace MeshWorld::Map
