// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <array>
#include <vector>

#include "Map/MapTilePayload.hpp"  // FieldGrid

namespace MeshWorld::Map {

// M342 (MAP22) -- traces the ocean/land boundary from an elevation grid as
// one or more smoothed world-space polylines. Addresses this task's own
// audit finding: `FeatureType::Coastline` has existed as an enum value
// (and had its own `feature_rgb_color()` entry in `PlanetMapLogic.cpp`)
// since MAP8/MAP9, but nothing anywhere ever constructed a `MapFeature`
// with that type -- the exact "exists in name only" gap `ZoneType::cave`
// had before MAP21 (M331). "Ocean" uses the SAME `elevation_m < sea_level_m`
// criterion `BiomeClassifier::classify()`/`Countries.cpp`'s own
// `near_water()`/`Settlements.cpp`'s own `suitability()` already use, not a
// hardcoded list of ocean-family `ZoneType`s -- one threshold, reused
// everywhere this codebase asks "is this cell ocean".
//
// Pure static; no state (mirrors Hydrology/MountainRanges/BiomeRefinement/
// Settlements/Countries' style).
class Coastline {
public:
    // Traces every ocean/land boundary in `elevation` into its own
    // world-space polyline, closed (first point repeated as last) --
    // unlike Countries.cpp's own `trace_owner_border()` (which keeps only
    // the single largest loop, a documented V1 simplification for exactly
    // one owner's territory), a coastline can have many disjoint
    // loops (islands, bays, lakes-that-count-as-ocean) and ALL of them
    // matter here, not just the biggest. Treats out-of-bounds cells as
    // ocean, so a landmass running off the tile edge still traces a
    // well-defined closed loop within this tile (the actual coastline
    // continues into the neighboring tile, out of this function's scope --
    // same "no lookahead into a neighbor's real content" limitation this
    // codebase already accepts elsewhere, e.g. CaveLayout.hpp).
    //
    // `smoothing_iterations` rounds of Chaikin's corner-cutting algorithm
    // (each round replaces every edge with two points at 1/4 and 3/4 along
    // it) are applied to every loop afterward, turning the raw raster's
    // blocky 90-degree staircase into a smooth curve -- this task's own
    // actual point. 0 returns the raw, unsmoothed trace (useful to test the
    // smoothing step's own effect in isolation). Returns {} if `elevation`
    // is empty or has no ocean/land boundary at all (uniformly ocean or
    // uniformly land -- the common case for most tiles at most zoom
    // levels, e.g. a deep continental interior or open ocean).
    static std::vector<std::vector<std::array<double, 2>>> trace(
        const FieldGrid& elevation, double sea_level_m,
        double world_x0, double world_z0, double world_x1, double world_z1,
        int smoothing_iterations = 2);
};

} // namespace MeshWorld::Map
