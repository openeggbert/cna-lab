// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "WorldConfig.hpp"
#include <filesystem>
#include <fstream>
#include <utility>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace MeshWorld {

WorldConfig::WorldConfig() = default;

bool WorldConfig::load_from_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;

    json j;
    try { f >> j; } catch (const json::exception&) { return false; }

    // T242 -- local, typically-gitignored overrides: "world.local.json"
    // alongside the base config file, merged on top via RFC 7396 JSON
    // Merge Patch (nested objects merge key-by-key; arrays/scalars in the
    // override replace the base wholesale, e.g. a "zones" override
    // replaces the whole zones array rather than appending to it). Absent
    // entirely is the common case, not an error. A malformed override
    // file fails the whole load the same way a malformed base file does,
    // rather than silently falling back to the base config -- the caller
    // asked for the override to apply.
    const auto local_path = std::filesystem::path(path).parent_path() / "world.local.json";
    std::ifstream lf(local_path);
    if (lf.is_open()) {
        json local_j;
        try { lf >> local_j; } catch (const json::exception&) { return false; }
        j.merge_patch(local_j);
    }

    version      = j.value("version",      version);
    name         = j.value("name",         name);
    seed         = j.value("seed",         seed);
    style        = j.value("style",        style);
    map_size_m   = j.value("map_size_m",   map_size_m);
    chunk_size_m = j.value("chunk_size_m", chunk_size_m);
    grid_w       = j.value("grid_w",       grid_w);
    grid_h       = j.value("grid_h",       grid_h);

    if (j.contains("zone_default"))
        zone_default = zone_from_string(j["zone_default"].get<std::string>());
    if (j.contains("region_default"))
        region_default = region_from_string(j["region_default"].get<std::string>());

    procedural           = j.value("procedural",           procedural);
    procedural_cell_size = j.value("procedural_cell_size", procedural_cell_size);
    use_world_composer   = j.value("use_world_composer",   use_world_composer);

    // Planetary map fields (MAP3) — all optional; absent keys keep the defaults.
    planet_size_m  = j.value("planet_size_m",  planet_size_m);
    continents_min = j.value("continents_min", continents_min);
    continents_max = j.value("continents_max", continents_max);
    sea_level_m    = j.value("sea_level_m",    sea_level_m);
    equator_temp_c = j.value("equator_temp_c", equator_temp_c);
    pole_temp_c    = j.value("pole_temp_c",    pole_temp_c);
    world_entropy  = j.value("world_entropy",  world_entropy);

    zones.clear();
    if (j.contains("zones") && j["zones"].is_array()) {
        for (const auto& z : j["zones"]) {
            ZoneOverride zo;
            zo.x_min = z.value("x_min", 0);
            zo.x_max = z.value("x_max", 0);
            zo.y_min = z.value("y_min", 0);
            zo.y_max = z.value("y_max", 0);
            zo.type          = zone_from_string(z.value("type", std::string("empty")));
            zo.region_default = region_from_string(
                z.value("region_default", std::string("open")));

            if (z.contains("regions") && z["regions"].is_array()) {
                for (const auto& r : z["regions"]) {
                    RegionOverride ro;
                    ro.x_min = r.value("x_min", zo.x_min);
                    ro.x_max = r.value("x_max", zo.x_max);
                    ro.y_min = r.value("y_min", zo.y_min);
                    ro.y_max = r.value("y_max", zo.y_max);
                    ro.type  = region_from_string(r.value("type", std::string("open")));
                    zo.regions.push_back(ro);
                }
            }
            zones.push_back(zo);
        }
    }

    landmarks.clear();
    if (j.contains("landmarks") && j["landmarks"].is_array()) {
        for (const auto& l : j["landmarks"]) {
            LandmarkPlacement lp;
            lp.chunk_x       = l.value("chunk_x", 0);
            lp.chunk_y       = l.value("chunk_y", 0);
            lp.definition_id = l.value("definition_id", std::string());
            lp.x             = l.value("x", 0.0f);
            lp.z             = l.value("z", 0.0f);
            lp.rotation_y    = l.value("rotation_y", 0.0f);
            landmarks.push_back(lp);
        }
    }

    road_termini.clear();
    if (j.contains("road_termini")) {
        if (!j["road_termini"].is_array()) return false;
        for (const auto& t : j["road_termini"]) {
            if (!t.is_object()) return false;
            RoadTerminus terminus;
            terminus.chunk_x = t.value("chunk_x", 0);
            terminus.chunk_y = t.value("chunk_y", 0);
            terminus.edge    = t.value("edge", std::string());
            const std::string kind = t.value("kind", std::string("terminus"));
            if (terminus.edge != "north" && terminus.edge != "south" &&
                terminus.edge != "east" && terminus.edge != "west") return false;
            if (kind == "boundary") terminus.kind = RoadTerminusKind::boundary;
            else if (kind != "terminus") return false;
            road_termini.push_back(std::move(terminus));
        }
    }
    return true;
}

bool WorldConfig::is_consistent() const {
    if (chunk_size_m <= 0) return false;
    if (grid_w != map_size_m / chunk_size_m) return false;
    if (grid_h != map_size_m / chunk_size_m) return false;

    // Planet fields (MAP3/M040). Continent count: default 5..12, allowed 4..20.
    if (planet_size_m <= 0.0) return false;
    if (continents_min < 4 || continents_max > 20) return false;
    if (continents_min > continents_max) return false;

    return true;
}

} // namespace MeshWorld
