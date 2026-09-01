// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include "Map/Hydrology.hpp"       // HydrologyNetwork
#include "Map/MapTilePayload.hpp"  // FieldGrid, BiomeGrid
#include "Map/Volcanism.hpp"       // VolcanicField

namespace MeshWorld::Map {

// Neighbor-aware biome refinement passes (MAP8, M128+) that `BiomeClassifier`
// cannot do per-cell alone (it only sees one cell's own elevation/temperature/
// moisture). Pure static; no state (mirrors Hydrology/MountainRanges' style).
class BiomeRefinement {
public:
    // M128 — reclassifies a land cell to ZoneType::beach when it is within
    // `radius_cells` (Chebyshev/8-connected distance, matching the
    // 8-neighbor convention `Hydrology::trace()` already uses) of an ocean
    // cell, provided the land cell's own elevation is no more than
    // `max_beach_elevation_m` above sea level. That elevation cap keeps a
    // steep cliff plunging straight into the ocean as whatever it already
    // was (e.g. mountain), instead of turning it into a sandy beach just
    // because it happens to touch the coastline.
    //
    // Ocean cells are never modified (only land cells become beach). Unlike
    // Hydrology::carve()/MountainRanges::apply(), this operates on the
    // *whole* grid, including edge rows/columns: elevation has a real
    // parent/child boundary-matching invariant to protect (M108/M112/M117),
    // but biome deliberately does not -- MapPipelineTests.cpp documents
    // that biome is reclassified independently per tile with no
    // cross-boundary continuity guarantee, and no generator populates
    // `TileEdge::biome` (M107) today. Skipping edges here would only lose
    // real coastline coverage for no compatibility benefit.
    static void applyCoastalBeach(BiomeGrid& biome, const FieldGrid& elevation,
                                   double sea_level_m,
                                   int radius_cells = 1,
                                   double max_beach_elevation_m = 50.0);

    // M130 — demotes a cell already classified as ZoneType::swamp back to
    // ZoneType::meadow when its immediate 8-neighborhood isn't flat: local
    // relief (max minus min elevation among the cell and its neighbors)
    // greater than `max_local_relief_m` means it's a wet slope, not a
    // floodplain. Pairs with `BiomeClassifier::classify()`'s M130 lowland
    // elevation cap (the "lowlands" half of "flat wet lowlands"; this is
    // the "flat" half, which needs neighbor awareness classify() can't do
    // per-cell). Cells not currently classified as swamp are left alone.
    static void applySwampFlatnessCheck(BiomeGrid& biome, const FieldGrid& elevation,
                                         double max_local_relief_m = 150.0);

    // M259 — reclassifies a badlands/mesa/rocky_desert cell to
    // ZoneType::canyon when its own 3x3 local relief (max minus min
    // elevation among itself and its neighbors, same technique
    // applySwampFlatnessCheck() already uses) is at least
    // `steep_relief_m`. A canyon is a deep, steep-walled gorge cut into
    // otherwise-flat dry rock, not its own climate band -- a pure per-cell
    // elevation/temperature/moisture function can't express "locally
    // steep", which is why this needs the same neighbor awareness
    // BiomeClassifier::classify() can't do alone.
    //
    // Cells not currently classified badlands/mesa/rocky_desert are left
    // alone; canyon itself is never one of those three, so a single
    // forward pass is safe (no run-away canyon-spreads-into-canyon
    // cascade).
    static void applyCanyonCarving(BiomeGrid& biome, const FieldGrid& elevation,
                                    double steep_relief_m = 80.0);

    // M274/M275 — refines the coastal band (any non-ocean-family cell
    // within `radius_cells` of an ocean-family cell, same 8-connected
    // neighbor test applyCoastalBeach() already uses) into
    // ZoneType::tidal_flat or ZoneType::sea_cliff based on local relief
    // (same 3x3 min/max technique applySwampFlatnessCheck() uses, but over
    // LAND neighbors only -- ocean-family neighbors are excluded from the
    // min/max scan, otherwise every coastal cell would read as "steep"
    // purely from the elevation drop into the adjacent seafloor, regardless
    // of how flat the actual shoreline terrain is):
    //   - relief >= `steep_relief_m`, OR elevation more than
    //     `max_coastal_elevation_m` above sea level -> sea_cliff (the
    //     "steep cliff plunging into the ocean" case applyCoastalBeach()'s
    //     own doc comment already calls out as deliberately excluded from
    //     becoming a sandy beach);
    //   - otherwise, relief < `flat_relief_m` -> tidal_flat (an unusually
    //     flat shore -- a periodically-submerged mudflat, not an ordinary
    //     sloped beach);
    //   - otherwise left unchanged (moderate relief within the beach
    //     elevation band -- ordinary beach territory).
    //
    // Reclassification depends only on elevation/relief, never the cell's
    // own current biome, so this is safe to run in either order relative
    // to applyCoastalBeach() -- though the natural pipeline position is
    // right after it (see ChildGenerator.cpp/PlanetGenerator.cpp), so a
    // freshly-set beach cell can still be refined further into
    // tidal_flat/sea_cliff.
    static void applyCoastalReliefRefinement(BiomeGrid& biome, const FieldGrid& elevation,
                                              double sea_level_m,
                                              int radius_cells = 1,
                                              double max_coastal_elevation_m = 50.0,
                                              double flat_relief_m = 15.0,
                                              double steep_relief_m = 80.0);

    // M247 — reclassifies a cell currently in the grassland/dry-climate
    // family (savanna, steppe, prairie, chaparral, shrubland) to
    // ZoneType::riparian_forest when it lies within `radius_cells` grid
    // cells of any traced river point in `network` (converted to a
    // real-world distance using the grid's own cell size -- unlike
    // applyCoastalBeach()'s ocean-family neighbor scan, a traced river
    // point is a continuous world-space coordinate, not itself a grid
    // cell, so this can't reuse that exact 8-neighbor test). Models a real
    // riparian/gallery forest: a narrow forest corridor a river's extra
    // moisture supports even where the surrounding climate would otherwise
    // only produce grassland/scrubland.
    //
    // Deliberately scoped to just that one climate cluster for v1 -- NOT
    // the desert/arid family too (e.g. a Nile-style river cutting through
    // true desert), which is a real, defensible ecology but a broader,
    // separate scope decision than this task implies. `grid_w`/`grid_h`
    // and `world_x0/z0/x1/z1` must match the elevation grid `network` was
    // traced over (same convention Hydrology::exportCrossings() already
    // requires). No-op if `network` has no rivers, or the grid/bounds are
    // degenerate.
    static void applyRiparianForest(BiomeGrid& biome, const HydrologyNetwork& network,
                                     int grid_w, int grid_h,
                                     double world_x0, double world_z0,
                                     double world_x1, double world_z1,
                                     int radius_cells = 1);

    // M265-268 — reclassifies every non-ocean-family cell within a
    // hotspot's own `radius_m` (see Volcanism::generate()), REGARDLESS of
    // its current biome: a volcanic hotspot's direct impact zone (lava,
    // ash, geothermal vents) plausibly overrides whatever climate-based
    // biome was there, the same "broad override" reach
    // applyCoastalReliefRefinement() already has for the coastal band.
    // Where multiple hotspots' radii overlap a cell, the NEAREST one wins
    // (same convention Volcanism::sampleElevation() uses for elevation).
    // For the nearest qualifying hotspot:
    //   - dormant -> ZoneType::ash_plain (the settled ash blanket around
    //     an extinct/dormant volcano; distance-within-radius doesn't
    //     matter further, the whole influence zone reads as ash-blanketed);
    //   - active AND the cell is also within `coastal_radius_cells` of an
    //     ocean-family cell (same neighbor test applyCoastalBeach() uses)
    //     -> ZoneType::volcanic_island (an island shaped by an active
    //     volcano -- reachable without Volcanism::apply() ever having to
    //     violate the "never turn ocean into land" invariant, since this
    //     only requires the LAND side of an existing coastline to be both
    //     volcanically active and coast-adjacent, not a brand new
    //     landmass);
    //   - active, not coastal, within `inner_fraction` (0.4) of the
    //     hotspot's own radius -> ZoneType::volcanic (the crater/lava
    //     zone);
    //   - active, not coastal, further out but still in range ->
    //     ZoneType::geothermal (the hot-springs/fumarole periphery).
    //
    // Deliberately runs LAST among the refinement passes (see
    // ChildGenerator.cpp/PlanetGenerator.cpp) -- a volcanic hotspot is the
    // most dramatic natural override of this whole group, so it's given
    // the final say over whatever canyon/tidal_flat/sea_cliff/
    // riparian_forest classification an earlier pass produced for the same
    // cell, right before the M354 city-inheritance override (which still
    // wins over even this, unchanged).
    static void applyVolcanicBiomes(BiomeGrid& biome, const VolcanicField& field,
                                     int grid_w, int grid_h,
                                     double world_x0, double world_z0,
                                     double world_x1, double world_z1,
                                     int coastal_radius_cells = 1,
                                     double inner_fraction = 0.4);
};

} // namespace MeshWorld::Map
