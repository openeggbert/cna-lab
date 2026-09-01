// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Map/Hydrology.hpp"
#include "Map/MapTilePayload.hpp"
#include "Map/MountainRanges.hpp"

namespace MeshWorld::Map {

// M132 — converts the algorithmic output of Hydrology/MountainRanges
// (M121-M127) into named MapFeature entries, via MeshWorld::Naming. Neither
// Hydrology::trace()/carve() nor MountainRanges::generate()/apply() produce
// MapFeature entries themselves -- they only ever mutate an elevation
// FieldGrid in place, so without this, a traced network is discarded the
// moment the generator that traced it returns. Pure static; no state
// (mirrors Hydrology/MountainRanges/BiomeRefinement's style).
class FeatureNaming {
public:
    // Appends a MapFeature::River for every river segment with at least
    // `min_river_points` points (filters out tiny headwater stubs that
    // would otherwise clutter the payload with hundreds of
    // barely-a-trickle named "rivers" -- a real river needs to actually go
    // somewhere), and a MapFeature::Lake for every lake, regardless of
    // size. `entropy` seeds per-feature naming (combined with each
    // feature's index so multiple rivers/lakes in one tile get distinct
    // names); `culture` is passed straight through to Naming::river()/
    // Naming::lake().
    static void appendHydrologyFeatures(std::vector<MapFeature>& features,
                                         const HydrologyNetwork& network,
                                         const std::string& culture,
                                         std::uint64_t entropy,
                                         int min_river_points = 5);

    // Appends a MapFeature::MountainRange for every range whose ridge has
    // at least 2 points falling within [world_x0,world_x1) x
    // [world_z0,world_z1) -- MountainRanges::generate()'s ridge is an
    // unclamped random walk (by design, see its own doc comment) that can
    // wander outside the seed bounds, and MapValidator::
    // check_features_in_bounds() rejects any feature point outside the
    // tile's own world_bounds(). Only the in-bounds subset of each ridge's
    // points is kept, so a range crossing multiple tiles gets its own
    // partial polyline in each one it actually passes through; a range
    // that doesn't reach this tile at all (or barely clips a single point)
    // is skipped here entirely.
    //
    // Also requires at least one in-bounds ridge point to sit over land
    // (`elevation` at that point's cell > `sea_level_m`): `generate()`'s
    // ridge geometry is entropy-driven and independent of elevation, so a
    // range can exist purely geometrically over a tile that is 100% ocean
    // -- `MountainRanges::apply()` would have skipped every cell there
    // (land-only, see its own doc comment), so naming it anyway would
    // label a "mountain range" with no actual uplifted terrain behind it.
    static void appendMountainRangeFeatures(std::vector<MapFeature>& features,
                                             const MountainRangeNetwork& network,
                                             const FieldGrid& elevation, double sea_level_m,
                                             const std::string& culture,
                                             std::uint64_t entropy,
                                             double world_x0, double world_z0,
                                             double world_x1, double world_z1);
};

} // namespace MeshWorld::Map
