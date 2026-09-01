// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cstdint>
#include <vector>

#include "Map/MapTilePayload.hpp"  // FieldGrid

namespace MeshWorld::Map {

// Volcanic hotspot data model (MAP16, M265-268). Structurally mirrors
// MountainRanges (generate a network -> apply its elevation contribution
// into an existing FieldGrid, edge rows/columns untouched) but with
// point/radial geometry instead of line/ridge geometry: a real volcano is
// a localized, roughly circular feature, not a meandering polyline the
// way a mountain range is.

// One volcanic hotspot, in world coordinates (meters). `peak_elevation_m`
// is this hotspot's own contribution at its center, added on top of
// whatever base terrain already exists there (same "additive, not
// absolute" convention RidgePoint::elevation_m uses). `radius_m` is how
// far its influence (both elevation uplift and, separately, biome
// reclassification) reaches before falling to zero. `active` is a
// snapshot state (currently erupting/geothermally active vs. dormant/
// extinct) -- real volcanic terrain is mostly dormant at any given
// moment, only a minority is presently active.
struct VolcanicHotspot {
    double x{0.0};
    double z{0.0};
    double peak_elevation_m{0.0};
    double radius_m{0.0};
    bool   active{false};
};

// The complete set of hotspots generated for one pass.
struct VolcanicField {
    std::vector<VolcanicHotspot> hotspots;

    bool empty() const { return hotspots.empty(); }
};

// Pure static; no state (mirrors MountainRanges/Hydrology/BiomeClassifier's
// style).
class Volcanism {
public:
    // M265-268 — seeds `count` hotspots from tectonic-style seed points
    // (same "hash-jittered point, entropy-driven, fully reproducible given
    // the same inputs" convention MountainRanges::generate() uses).
    // `peak_elevation_m` is scaled into [min_peak_elevation_m,
    // max_peak_elevation_m]; `radius_m` is scaled relative to the tile's
    // own size (matching MountainRanges::generate()'s own `step_m`
    // derivation) so a hotspot reads as a sensibly-sized feature at any
    // zoom level, from a planet-scale tile down to a metro-scale one.
    // `active` is a ~30% probability draw (`kActiveProbability` in the
    // .cpp) -- deliberately NOT elevation- or terrain-aware: same v1
    // simplicity MountainRanges::generate() itself accepts (a future
    // refinement could bias hotspot placement toward existing mountain
    // ranges, but that's separate, larger-scoped work, not implied here).
    static VolcanicField generate(std::uint64_t entropy, int count,
                                   double world_x0, double world_z0,
                                   double world_x1, double world_z1,
                                   double min_peak_elevation_m,
                                   double max_peak_elevation_m);

    // This field's elevation contribution at world position (x, z): the
    // largest single-hotspot contribution among every hotspot whose
    // radius reaches this point (linear falloff to 0 at each hotspot's
    // own `radius_m`) -- "largest wins, not summed," the same rule
    // Hydrology::carve() already uses for overlapping river falloffs, so
    // two nearby hotspots don't stack into an implausibly tall combined
    // peak.
    static double sampleElevation(const VolcanicField& field, double x, double z);

    // Adds sampleElevation()'s contribution on top of `elevation`, in
    // place, for every interior *land* cell -- same "never turn ocean
    // into land" invariant MountainRanges::apply() enforces (a volcanic
    // hotspot reshapes an existing landmass; it doesn't sprout a new
    // island out of open ocean in this v1 -- see applyVolcanicBiomes()'s
    // own doc comment for how `volcanic_island` is still reachable
    // without that). Edge rows/columns untouched, same
    // parent/child-boundary-matching reasoning MountainRanges::apply()
    // documents.
    static void apply(FieldGrid& elevation, const VolcanicField& field,
                       double sea_level_m,
                       double world_x0, double world_z0, double world_x1, double world_z1);
};

} // namespace MeshWorld::Map
