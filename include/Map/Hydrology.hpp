// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <array>
#include <vector>

#include "Map/MapTilePayload.hpp"  // FieldGrid

namespace MeshWorld::Map {

// River network data model (MAP8, M121). This is the in-memory shape
// Hydrology::trace() (M122) produces; it is intentionally richer than
// Map::MapFeature (per-point flow/width, explicit confluence linkage) —
// naming/converting traced rivers into MapFeature::River features for
// storage is a later step (M132), not this one.

// One sampled point along a traced river, in world coordinates (meters).
// `flow` is an accumulated-drainage proxy (M124): grows monotonically from
// source to mouth as tributaries join; river width is derived from it.
struct RiverPoint {
    double x{0.0};
    double z{0.0};
    float  flow{0.0f};
};

// A river segment: one continuous polyline from a source (or a confluence
// with an upstream segment) down to either a confluence with a downstream
// segment or a terminal mouth (ocean/lake). `points` is ordered source ->
// mouth/confluence, i.e. downstream.
struct RiverSegment {
    std::vector<RiverPoint> points;
    // Index into HydrologyNetwork::rivers of the segment this one flows
    // into, or -1 if this segment terminates at a mouth (ocean/lake) itself.
    int  downstream_segment{-1};
    // True if this segment's final point empties into an ocean cell.
    // Mutually exclusive with lake_index >= 0 — a segment ends at the ocean,
    // a lake, or (only pre-M123) an unresolved dead end, never more than one.
    bool reaches_ocean{false};
    // Index into HydrologyNetwork::lakes of the lake this segment empties
    // into (M123), or -1 if it doesn't end at a lake (reaches_ocean instead,
    // or — should not happen once M123's basin-filling runs — an unresolved
    // dead end).
    int lake_index{-1};
    // M345 (MAP22) — true if this segment's trace stopped because its own
    // steepest-descent hit a cell on this tile's OUTER row/column with no
    // in-bounds neighbor lower than it, i.e. this tile's own limited field
    // of view ran out, not because the water is genuinely landlocked. A
    // THIRD terminal state, distinct from reaches_ocean/lake_index>=0 (both
    // false/-1 here) — trace() deliberately does NOT basin-fill this case
    // into a spurious Lake (the pre-M345 behavior), since the real terrain
    // just beyond this tile's own edge, invisible to this call, is far more
    // often still sloping downward than it is a coincidental matching local
    // minimum right at the seam. See Hydrology::exportCrossings().
    bool exits_tile{false};
};

// A lake: a closed drainage basin filled to its spill elevation (M123).
struct Lake {
    double x{0.0};
    double z{0.0};
    double surface_elevation_m{0.0};
    std::vector<std::array<double, 2>> shoreline;  // approximate boundary polygon, world coords
};

// The complete traced hydrology for one generation pass.
struct HydrologyNetwork {
    std::vector<RiverSegment> rivers;
    std::vector<Lake>         lakes;

    bool empty() const { return rivers.empty() && lakes.empty(); }
};

// Pure static; no state (mirrors BiomeClassifier's style).
class Hydrology {
public:
    // M122/M123 — traces a river from every source down to its terminus.
    //
    // A source is a land cell (elevation > sea_level_m) that is a local
    // maximum among its 8 neighbors (ties count as local maxima too, so a
    // flat plateau produces one source per plateau cell). From each source,
    // the trace repeatedly steps to the neighbor with the strictly lowest
    // elevation (steepest descent) until either:
    //   - it steps onto a cell at or below sea_level_m (a mouth: reaches_ocean
    //     = true), or
    //   - no neighbor is lower (a landlocked local minimum). M123: this pit
    //     is flood-filled (priority-flood / watershed algorithm) up to its
    //     true spill elevation — the lowest ridge point with a path out to
    //     the ocean, or the highest point reached if the basin is a fully
    //     enclosed endorheic sink with no ocean outlet at all within this
    //     grid — and recorded as a HydrologyNetwork::lakes entry; the
    //     segment's lake_index is set to it (reaches_ocean stays false).
    //     Multiple segments dead-ending at the same pit share one Lake.
    //
    // `world_x0/z0/x1/z1` are this elevation grid's world-space bounds,
    // used to convert cell (gx, gy) centers to RiverPoint/Lake world
    // coordinates the same way PlanetGenerator/ChildGenerator convert grid
    // cells to world positions: world_x0 + (gx + 0.5) * (world_x1 - world_x0) / w.
    //
    // M124 — confluence IS detected: since steepest descent is a pure
    // function of the elevation grid, two sources whose traces ever reach
    // the same interior (non-terminal) cell take an identical route from
    // there on. Whichever source's trace reaches that cell first (in
    // internal processing order — deterministic given the same grid, not
    // meaningful hydrologically) keeps going; the other stops there,
    // records the first one's index in `downstream_segment`, and does not
    // duplicate the shared downstream polyline. `RiverPoint::flow` is an
    // accumulated-drainage proxy: it grows with each step along a segment,
    // and a tributary's total flow at its confluence point is added to
    // every point downstream of the merge (recursively, through further
    // confluences) so flow keeps growing toward the mouth as tributaries
    // join. Elevation strictly decreases along a trace, so it always
    // terminates and never revisits a cell.
    static HydrologyNetwork trace(const FieldGrid& elevation, double sea_level_m,
                                   double world_x0, double world_z0,
                                   double world_x1, double world_z1);

    // M125 — carves river valleys into `elevation`, in place, following a
    // `network` traced over that same grid with the same `sea_level_m` and
    // world bounds. Depth and width (radius) both grow with a river point's
    // `flow`; where multiple points' falloffs overlap the same cell, the
    // largest reduction wins (not summed), so overlapping headwaters near a
    // confluence don't over-carve. Carving never lowers a cell below
    // `sea_level_m` (rivers cut valleys into land, not the ocean floor —
    // that refinement is biome/coastline territory, MAP8's later tasks).
    //
    // Never touches the grid's edge rows/columns (gx==0, gx==w-1, gy==0,
    // gy==h-1): those are exactly the samples `TileEdge::elevation` copies
    // out for parent/child boundary matching (M108/M112/M117), and each
    // tile traces hydrology independently, so a river near one tile's edge
    // has no guarantee of lining up with whatever the neighboring tile
    // traces on its side. Making carving interior-only preserves that
    // continuity invariant unconditionally; true cross-tile river
    // continuity is separate, later work (MAP7's blocked M110/M111, gated
    // on `TileEdge::crossings`).
    static void carve(FieldGrid& elevation, const HydrologyNetwork& network,
                       double sea_level_m,
                       double world_x0, double world_z0,
                       double world_x1, double world_z1);

    // M345 (MAP22) — appends an EdgeCrossingType::River EdgeCrossing to
    // `edges` (N=0/E=1/S=2/W=3, matching MapTilePayload::edges's own
    // documented index convention) for every segment in `network` with
    // exits_tile == true, at the grid cell its trace stopped on. Mirrors
    // Roads::exportCrossings()'s role (and its own explicit "no generator
    // populates river crossings... a separate, still-open, unscoped gap"
    // note) but grid-index-based rather than continuous-path-based: every
    // RiverPoint sits at a cell CENTER (world_x0 + (gx+0.5)*cell_w, never
    // exactly on the tile's true boundary LINE the way a road's own path
    // points can be), so Roads' own segment/boundary-line intersection
    // technique would never register a crossing for a river at all — this
    // instead directly uses the exiting cell's own grid row/column.
    // `grid_w`/`grid_h` and `world_x0/z0/x1/z1` must match the elevation
    // grid `network` was traced over (the same inverse of RiverPoint's own
    // world_x0 + (gx+0.5)*cell_w forward transform trace() itself uses) so
    // a segment's final point can be mapped back to the exact grid cell its
    // trace stopped on. A corner-cell exit (on both a horizontal AND
    // vertical boundary row/column at once) registers on both matching
    // edges — a deliberate, accepted imprecision for a genuinely rare case
    // (same class of "small edge-case imprecision accepted for simplicity"
    // this codebase already accepts elsewhere, e.g. CaveLayout's
    // full-width-passage v1). Purely additive: only appends to
    // `edges[i].crossings`, never clears it first, so it composes with
    // Roads::exportCrossings() regardless of call order (same guarantee
    // that function's own doc comment makes). No-op if `network` is empty
    // or grid_w/grid_h/world bounds are degenerate.
    static void exportCrossings(const HydrologyNetwork& network, int grid_w, int grid_h,
                                 double world_x0, double world_z0,
                                 double world_x1, double world_z1,
                                 std::array<TileEdge, 4>& edges);
};

} // namespace MeshWorld::Map
