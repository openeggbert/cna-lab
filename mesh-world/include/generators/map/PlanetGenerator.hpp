// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cstdint>

#include "Map/MapGenerator.hpp"
#include "Map/PlanetConstants.hpp"

namespace MeshWorld::Map {

// Inputs the PlanetGenerator needs, kept separate from WorldConfig so the map
// subsystem does not depend on the chunk-world config struct. Translate from
// WorldConfig at the call site (PlanetWorld / map pipeline).
struct PlanetParams {
    // Defaults to the real planet size (matches WorldConfig's own default,
    // see WorldConfig.hpp) rather than 0.0 — a bare PlanetParams{} used to be
    // a footgun: TileCoord's own math (tile_size_m()) uses the fixed
    // Map::PLANET_SIZE_M constant regardless, so a 0.0 planet_size_m here
    // only broke ChildGenerator's/PlanetGenerator's internal math (every
    // child_size_m collapsing to 0), which produced a degenerate
    // (0,0)-(0,0) world extent passed to Hydrology::trace()/
    // MountainRanges::generate() — not a crash, but observed to hang and
    // grow memory unbounded (found via WorldStreamerTests.cpp's/
    // WorldRendererTests.cpp's own map-layer tests). Every real caller
    // already set this explicitly (planet_params_from_config() or manual
    // assignment); this default just makes the degenerate case impossible
    // to hit by accident.
    double planet_size_m{PLANET_SIZE_M};   // planet edge, meters (continent centers lie in [0, this))
    int    continents_min{5};    // inclusive continent-count range; the actual count is
    int    continents_max{12};   //   entropy-driven within [min, max]
    double sea_level_m{0.0};      // ocean threshold (used from M055)
    double equator_temp_c{30.0};  // climate band endpoints (used from M059)
    double pole_temp_c{-20.0};
};

// Level-0 planet generator (MAP4). Produces the planet-root MapTilePayload.
// M053 seeds the continent centers across the planet square; land/ocean,
// elevation, climate, and biomes are layered on in later tasks (M054-M063).
// Pure in (tile, entropy): same inputs -> identical payload (M070).
class PlanetGenerator : public MapGenerator {
public:
    explicit PlanetGenerator(PlanetParams params);

    MapTilePayload generate(const TileCoord&      tile,
                            const MapTilePayload* parent,
                            std::uint64_t         entropy) const override;

    // Deterministic continent count in [continents_min, continents_max].
    int continent_count(std::uint64_t entropy) const;

private:
    PlanetParams params_;
};

} // namespace MeshWorld::Map
