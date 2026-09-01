// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <functional>

#include "Map/PlanetConstants.hpp"

// Planetary map subsystem. All map-mechanism code lives under include/Map/ and
// src/Map/ in namespace MeshWorld::Map (see map.md, plan.md MAP-series).
namespace MeshWorld::Map {

// Axis-aligned world-space extent of a tile, in meters. The planet spans
// [0, PLANET_SIZE_M] on both axes; tile y maps to the world Z axis (matching
// ChunkCoord, where coord.y → world_z).
struct WorldBounds {
    double min_x{0}, min_z{0}, max_x{0}, max_z{0};
};

// Inclusive range of chunk indices a tile overlaps (y = world-Z chunk axis,
// matching ChunkCoord). Build ChunkCoord{int32_t(x), int32_t(y)} per cell.
struct ChunkRange {
    int64_t x_min{0}, y_min{0}, x_max{0}, y_max{0};
    constexpr int64_t count_x() const { return x_max - x_min + 1; }
    constexpr int64_t count_y() const { return y_max - y_min + 1; }
};

// A tile in the planetary LOD quadtree (see map.md §5, plan.md MAP1).
//
//   level — quadtree depth: 0 = the whole planet (one tile). Each deeper level
//           subdivides every tile into 2x2 children, halving the tile edge.
//   x, y  — tile indices within the level. At level z the valid range of each
//           axis is [0, 2^z). Stored as 64-bit for headroom at deep levels.
//
// M001 provides only the value type and its comparison operators; quadtree
// navigation (parent/child), sizes and conversions arrive in M003+.
struct TileCoord {
    int     level{0};
    int64_t x{0};
    int64_t y{0};

    constexpr TileCoord() = default;
    constexpr TileCoord(int level_, int64_t x_, int64_t y_)
        : level(level_), x(x_), y(y_) {}

    // Tile one level up that contains this tile: (level-1, x/2, y/2).
    // Level 0 is the planet root and has no parent — it returns itself.
    constexpr TileCoord parent() const {
        if (level == 0) return *this;
        return {level - 1, x / 2, y / 2};
    }

    // One of the four children one level down. qx, qy in {0,1}.
    // Inverse of parent(): child(qx,qy).parent() == *this.
    constexpr TileCoord child(int qx, int qy) const {
        return {level + 1, x * 2 + qx, y * 2 + qy};
    }

    // All four children one level down, in (qx,qy) order: (0,0),(1,0),(0,1),(1,1).
    constexpr std::array<TileCoord, 4> children() const {
        return {child(0, 0), child(1, 0), child(0, 1), child(1, 1)};
    }

    // Edge length (meters) of a tile at the given level: PLANET_SIZE_M / 2^level.
    static constexpr double tile_size_m(int level) {
        return PLANET_SIZE_M / static_cast<double>(int64_t{1} << level);
    }

    // Edge length (meters) of this tile.
    constexpr double size_m() const { return tile_size_m(level); }

    // World-space extent of this tile in meters: corner (x,y) * size .. + size.
    constexpr WorldBounds world_bounds() const {
        const double s = size_m();
        const double min_x = static_cast<double>(x) * s;
        const double min_z = static_cast<double>(y) * s;
        return {min_x, min_z, min_x + s, min_z + s};
    }

    // Tile at the given level containing the world point (meters). Inverse of
    // world_bounds(): a point inside a tile maps back to that tile. floor()
    // makes the min corner inclusive and handles out-of-bounds/negative inputs.
    static TileCoord from_world(double world_x, double world_z, int level) {
        const double s = tile_size_m(level);
        return {level,
                static_cast<int64_t>(std::floor(world_x / s)),
                static_cast<int64_t>(std::floor(world_z / s))};
    }

    // Inclusive chunk-index range this tile overlaps, for the given chunk size.
    // Tile bounds are half-open [min,max): the chunk containing the exclusive
    // max edge belongs to the neighbouring tile, hence ceil(max/cs) - 1.
    ChunkRange chunk_range(int chunk_size_m = 64) const {
        const WorldBounds b = world_bounds();
        const double cs = static_cast<double>(chunk_size_m);
        return {
            static_cast<int64_t>(std::floor(b.min_x / cs)),
            static_cast<int64_t>(std::floor(b.min_z / cs)),
            static_cast<int64_t>(std::ceil (b.max_x / cs)) - 1,
            static_cast<int64_t>(std::ceil (b.max_z / cs)) - 1,
        };
    }

    constexpr bool operator==(const TileCoord& o) const {
        return level == o.level && x == o.x && y == o.y;
    }
    constexpr bool operator!=(const TileCoord& o) const { return !(*this == o); }

    // Lexicographic ordering by (level, x, y) — for use as a key in ordered containers.
    constexpr bool operator<(const TileCoord& o) const {
        if (level != o.level) return level < o.level;
        if (x     != o.x)     return x     < o.x;
        return y < o.y;
    }
};

} // namespace MeshWorld::Map

// M002 — std::hash so TileCoord can key unordered_map / unordered_set
// (the map pipeline's in-memory tile cache). boost::hash_combine-style mixing
// of (level, x, y) for a good spread across neighboring tiles.
template<>
struct std::hash<MeshWorld::Map::TileCoord> {
    std::size_t operator()(const MeshWorld::Map::TileCoord& t) const noexcept {
        auto mix = [](std::size_t seed, std::size_t v) {
            return seed ^ (v + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
        };
        std::size_t h = std::hash<int>{}(t.level);
        h = mix(h, std::hash<int64_t>{}(t.x));
        h = mix(h, std::hash<int64_t>{}(t.y));
        return h;
    }
};
