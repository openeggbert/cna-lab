// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cstdint>
#include <string>
#include <functional>

namespace MeshWorld {

struct ChunkCoord {
    int32_t x{0};
    int32_t y{0};

    constexpr ChunkCoord() = default;
    constexpr ChunkCoord(int32_t x_, int32_t y_) : x(x_), y(y_) {}

    // Derive ChunkCoord from world-space position (in meters).
    static ChunkCoord from_world(float world_x, float world_z, int chunk_size_m) {
        return {
            static_cast<int32_t>(world_x / static_cast<float>(chunk_size_m)),
            static_cast<int32_t>(world_z / static_cast<float>(chunk_size_m))
        };
    }

    // World-space origin (corner) of this chunk in meters.
    float world_x(int chunk_size_m) const { return static_cast<float>(x * chunk_size_m); }
    float world_z(int chunk_size_m) const { return static_cast<float>(y * chunk_size_m); }

    ChunkCoord north() const { return {x,     y - 1}; }
    ChunkCoord south() const { return {x,     y + 1}; }
    ChunkCoord west()  const { return {x - 1, y    }; }
    ChunkCoord east()  const { return {x + 1, y    }; }

    std::string to_string() const {
        return std::to_string(x) + "_" + std::to_string(y);
    }

    constexpr bool operator==(const ChunkCoord& o) const { return x == o.x && y == o.y; }
    constexpr bool operator!=(const ChunkCoord& o) const { return !(*this == o); }
    constexpr bool operator< (const ChunkCoord& o) const {
        return x < o.x || (x == o.x && y < o.y);
    }
};

// Derive a per-chunk deterministic seed.
inline uint64_t chunk_seed(uint64_t world_seed, const ChunkCoord& c) {
    // FNV-1a inspired mixing; keeps seeds stable across platforms.
    uint64_t h = world_seed;
    h ^= static_cast<uint64_t>(static_cast<uint32_t>(c.x)) * 0x9e3779b97f4a7c15ULL;
    h += 0x6c62272e07bb0142ULL;
    h ^= static_cast<uint64_t>(static_cast<uint32_t>(c.y)) * 0x517cc1b727220a95ULL;
    h += 0x2e7b5b82d8f8d8d7ULL;
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;
    return h;
}

} // namespace MeshWorld

// std::hash support for use in unordered_map.
template<>
struct std::hash<MeshWorld::ChunkCoord> {
    std::size_t operator()(const MeshWorld::ChunkCoord& c) const noexcept {
        return std::hash<int64_t>{}(
            (static_cast<int64_t>(c.x) << 32) | static_cast<uint32_t>(c.y));
    }
};
