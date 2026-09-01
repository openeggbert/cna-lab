// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "MapValidator.hpp"

#include <cstdint>
#include <string>

#include "ZoneType.hpp"

namespace MeshWorld {

namespace {

void check_field_grid(const Map::FieldGrid& grid, const char* name, ValidationResult& result) {
    if (grid.empty()) {
        result.add_error(std::string(name) + " field is empty");
        return;
    }
    if (grid.data.size() != static_cast<std::size_t>(grid.w) * static_cast<std::size_t>(grid.h)) {
        result.add_error(std::string(name) + " field data size (" +
                         std::to_string(grid.data.size()) + ") does not match w*h (" +
                         std::to_string(grid.w) + "*" + std::to_string(grid.h) + ")");
    }
}

void check_biome_grid(const Map::BiomeGrid& grid, ValidationResult& result) {
    if (grid.empty()) {
        result.add_error("biome field is empty");
        return;
    }
    if (grid.data.size() != static_cast<std::size_t>(grid.w) * static_cast<std::size_t>(grid.h)) {
        result.add_error("biome field data size (" + std::to_string(grid.data.size()) +
                         ") does not match w*h (" + std::to_string(grid.w) + "*" +
                         std::to_string(grid.h) + ")");
        return;
    }
    constexpr auto max_valid = static_cast<std::uint8_t>(ZoneType::empty);
    for (auto v : grid.data) {
        if (v > max_valid) {
            result.add_error("biome ordinal " + std::to_string(v) + " is out of ZoneType range");
            return; // one report is enough; don't spam one error per cell
        }
    }
}

void check_features_in_bounds(const Map::MapTilePayload& payload, ValidationResult& result) {
    const Map::WorldBounds b = payload.tile.world_bounds();
    for (const auto& f : payload.features) {
        for (const auto& p : f.points) {
            const double x = p[0];
            const double z = p[1];
            if (x < b.min_x || x >= b.max_x || z < b.min_z || z >= b.max_z) {
                result.add_error((f.name.empty() ? std::string("(unnamed feature)") : f.name) +
                                 ": point (" + std::to_string(x) + "," + std::to_string(z) +
                                 ") outside tile bounds [" + std::to_string(b.min_x) + "," +
                                 std::to_string(b.max_x) + ")x[" + std::to_string(b.min_z) + "," +
                                 std::to_string(b.max_z) + ")");
            }
        }
    }
}

void check_edges(const Map::MapTilePayload& payload, ValidationResult& result) {
    if (payload.elevation.empty()) return; // already reported by check_field_grid
    static constexpr const char* NAMES[4] = {"N", "E", "S", "W"};
    const int expected[4] = {payload.elevation.w, payload.elevation.h,
                             payload.elevation.w, payload.elevation.h};
    for (int i = 0; i < 4; ++i) {
        const auto& edge = payload.edges[static_cast<std::size_t>(i)];
        if (edge.elevation.empty()) continue; // edges are optional (e.g. not every generator sets all four)
        if (static_cast<int>(edge.elevation.size()) != expected[i]) {
            result.add_error(std::string(NAMES[i]) + " edge has " +
                             std::to_string(edge.elevation.size()) + " samples, expected " +
                             std::to_string(expected[i]));
        }
    }
}

} // namespace

ValidationResult MapValidator::validate(const Map::MapTilePayload& payload) const {
    ValidationResult result;

    check_field_grid(payload.elevation, "elevation", result);
    check_field_grid(payload.temperature, "temperature", result);
    check_field_grid(payload.moisture, "moisture", result);
    check_biome_grid(payload.biome, result);
    check_features_in_bounds(payload, result);
    check_edges(payload, result);

    return result;
}

} // namespace MeshWorld
