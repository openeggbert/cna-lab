// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "PlanetWorld.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace MeshWorld {
namespace {

using json = nlohmann::json;

std::string world_json_path(const std::string& dir) {
    return dir + "/world.json";
}

// Persist the planet-relevant config fields + entropy. Planet worlds are
// generated (not zone-authored), so the legacy zone overrides are not written.
void write_world_json(const std::string& path, const WorldConfig& c) {
    json j;
    j["version"]        = c.version;
    j["name"]           = c.name;
    j["world_entropy"]  = c.world_entropy;
    j["planet_size_m"]  = c.planet_size_m;
    j["continents_min"] = c.continents_min;
    j["continents_max"] = c.continents_max;
    j["sea_level_m"]    = c.sea_level_m;
    j["equator_temp_c"] = c.equator_temp_c;
    j["pole_temp_c"]    = c.pole_temp_c;

    std::ofstream f(path);
    if (!f) throw std::runtime_error("PlanetWorld: cannot write " + path);
    f << j.dump(2) << '\n';
}

} // namespace

PlanetWorld::PlanetWorld(std::string dir, WorldConfig config, std::uint64_t entropy)
    : dir_(std::move(dir)), config_(std::move(config)), world_entropy_(entropy) {}

PlanetWorld PlanetWorld::create_new(const std::string& dir, WorldConfig config) {
    std::filesystem::create_directories(dir);

    const auto entropy = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    config.world_entropy = entropy;

    write_world_json(world_json_path(dir), config);
    return PlanetWorld(dir, std::move(config), entropy);
}

PlanetWorld PlanetWorld::open_existing(const std::string& dir) {
    const std::string path = world_json_path(dir);

    WorldConfig config;
    if (!config.load_from_file(path))
        throw std::runtime_error("PlanetWorld: cannot open world.json at " + path);
    if (config.world_entropy == 0)
        throw std::runtime_error(
            "PlanetWorld: world.json carries no world_entropy (not a planet world): " + path);

    const std::uint64_t entropy = config.world_entropy;
    return PlanetWorld(dir, std::move(config), entropy);
}

Map::MapTileStore& PlanetWorld::tile_store(int level) {
    auto it = tile_stores_.find(level);
    if (it == tile_stores_.end()) {
        it = tile_stores_
                 .emplace(level, std::make_unique<Map::MapTileStore>(dir_, level))
                 .first;
    }
    return *it->second;
}

} // namespace MeshWorld
