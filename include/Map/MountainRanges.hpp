// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cstdint>
#include <vector>

#include "Map/MapTilePayload.hpp"  // FieldGrid

namespace MeshWorld::Map {

// Mountain range data model (MAP8, M126/M127). Independent of Hydrology, but
// deliberately mirrors its shape (generate a network of polylines -> apply
// its effect into an existing FieldGrid, edge rows/columns untouched) so the
// two systems compose predictably in the generators that call them.

// One sampled point along a ridge, in world coordinates (meters).
// `elevation_m` is this point's own height above sea level (M127's
// peak/pass profile) -- it is added on top of whatever base terrain already
// exists there, not an absolute world elevation.
struct RidgePoint {
    double x{0.0};
    double z{0.0};
    double elevation_m{0.0};
};

// One contiguous ridge: a polyline of RidgePoints from one end of the range
// to the other.
struct MountainRange {
    std::vector<RidgePoint> ridge;
};

// The complete set of ranges generated for one pass.
struct MountainRangeNetwork {
    std::vector<MountainRange> ranges;

    bool empty() const { return ranges.empty(); }
};

// Pure static; no state (mirrors Hydrology/BiomeClassifier's style).
class MountainRanges {
public:
    // M126 — seeds `count` ridge polylines from tectonic-style seed points.
    // Entropy-driven: the same `entropy` and inputs always produce an
    // identical network (same determinism requirement as PlanetGenerator's
    // continent seeding, M070).
    //
    // Each ridge starts at a hash-jittered point within
    // [world_x0,world_x1) x [world_z0,world_z1) and is a fixed-length
    // directed random walk (heading drifts by a small random turn each
    // step) so ridges meander naturally instead of running dead straight.
    // M127's elevation profile is baked in per point: `elevation_m` varies
    // along the ridge via fBm noise, scaled into
    // [min_peak_elevation_m, max_peak_elevation_m], so peaks (local maxima
    // along the ridge) and passes (local minima) emerge from the same
    // profile rather than needing separate peak/pass bookkeeping.
    static MountainRangeNetwork generate(std::uint64_t entropy, int count,
                                          double world_x0, double world_z0,
                                          double world_x1, double world_z1,
                                          double min_peak_elevation_m,
                                          double max_peak_elevation_m);

    // M127 — this network's elevation contribution at world position
    // (x, z): the *nearest* ridge point's `elevation_m`, falling off
    // linearly to 0 at `falloff_width_m` away (0 beyond that). Ridges are
    // sampled as discrete points, not nearest-point-on-segment, the same
    // discrete-sample approximation Hydrology already makes for rivers.
    // Where two ranges are close enough to overlap, nearest-wins (not
    // blended) -- a reasonable v1, revisit only if a real gap shows up.
    static double sampleElevation(const MountainRangeNetwork& network, double x, double z,
                                   double falloff_width_m);

    // Adds sampleElevation()'s contribution on top of `elevation`, in
    // place, for every interior *land* cell (a cell already above
    // `sea_level_m` before this call) -- ranges uplift existing land, they
    // don't sprout new islands out of the ocean, and callers rely on land
    // cells staying strictly above sea level and ocean cells strictly below
    // it (the same invariant Hydrology::carve() preserves from the other
    // direction). Never touches the grid's edge rows/columns
    // (gx==0, gx==w-1, gy==0, gy==h-1) for the same reason Hydrology::
    // carve() doesn't: those are exactly the samples `TileEdge::elevation`
    // copies out for parent/child boundary matching (M108/M112/M117), and
    // each tile generates its ridges independently, so there's no
    // guarantee a neighboring tile's ranges line up at the shared edge.
    static void apply(FieldGrid& elevation, const MountainRangeNetwork& network,
                       double sea_level_m, double falloff_width_m,
                       double world_x0, double world_z0, double world_x1, double world_z1);
};

} // namespace MeshWorld::Map
