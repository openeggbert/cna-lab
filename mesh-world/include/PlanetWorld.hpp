// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "WorldConfig.hpp"
#include "Map/MapTileStore.hpp"

namespace MeshWorld {

// Top-level handle for one persisted planet world (MAP3 bootstrap). It owns the
// world's time-based entropy and lazily-opened per-level MapTileStores.
//
// A world lives in a directory holding:
//   world.json        — config + world_entropy
//   map_level{z}.db    — one MapTileStore per quadtree level (created on demand)
//
// The entropy is generated once at create_new() (from steady_clock) and
// persisted. Revisit consistency comes from persistence, never from a fixed
// seed — different worlds diverge because each gets a fresh entropy (map.md §8).
class PlanetWorld {
public:
    // Create a brand-new world in `dir`: generate fresh time entropy, write
    // world.json from `config` (its world_entropy field is overwritten), and
    // create the directory. An existing world.json in `dir` is replaced.
    static PlanetWorld create_new(const std::string& dir, WorldConfig config = {});

    // Open an existing world: load world.json (config + entropy) from `dir`.
    // Throws std::runtime_error if world.json is missing or carries no entropy
    // (i.e. it is not a planet world).
    static PlanetWorld open_existing(const std::string& dir);

    std::uint64_t      world_entropy() const { return world_entropy_; }
    const WorldConfig& config()        const { return config_; }
    const std::string& dir()           const { return dir_; }

    // Lazily open (and cache) the MapTileStore for a quadtree level. The same
    // reference is returned for repeated calls with the same level.
    Map::MapTileStore& tile_store(int level);

private:
    PlanetWorld(std::string dir, WorldConfig config, std::uint64_t entropy);

    std::string   dir_;
    WorldConfig   config_;
    std::uint64_t world_entropy_{0};
    std::map<int, std::unique_ptr<Map::MapTileStore>> tile_stores_;
};

} // namespace MeshWorld
