// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "Map/Hydrology.hpp"
#include "Map/MapTilePayload.hpp"
#include "Map/MountainRanges.hpp"
#include "Map/TileCoord.hpp"
#include "Map/Volcanism.hpp"

namespace MeshWorld {

// Collects Lua map-generator calls into a Map::MapTilePayload (MAP6, M088/M089).
// Mirrors Mc3SceneBuilder's role for chunk generators: a safe, ergonomic C++
// API that Lua binds to via sol2 (see LuaRuntime), rather than exposing
// MapTilePayload's raw struct fields directly to script code.
//
// Field grids: elevation/temperature/moisture are square W x H grids (see
// Map::FieldGrid), row-major (index = gy*w + gx), matching the existing
// convention used by PlanetGenerator/ChildGenerator. setBiomeField() takes
// all three at once (already computed by the calling generator, e.g. via
// ctx.noise() calls in a loop) and derives the BiomeGrid per-cell via
// BiomeClassifier — the same relationship PlanetGenerator/ChildGenerator
// already have between these fields, just exposed as one builder call.
class MapBuilder {
public:
    // tile/entropy seed the resulting payload's own fields
    // (MapTilePayload::tile/entropy). sea_level_m is needed for the biome
    // classification setBiomeField() performs. parent is null at level 0 (the
    // planet root has no parent); when non-null, setBiomeField() enforces
    // parent->edges as a fixed boundary condition (M108) on the two sides of
    // this tile that coincide with the parent's own outer boundary.
    MapBuilder(const Map::TileCoord& tile, std::uint64_t entropy, double sea_level_m,
               const Map::MapTilePayload* parent = nullptr);

    // --- fields ---
    // w*h must equal elevation.size() == temperature.size() == moisture.size().
    // M108: if a parent was given at construction, the boundary rows/columns
    // of the resulting elevation (and re-derived biome) are overwritten to
    // match parent->edges on the two sides this tile shares with the parent's
    // own boundary — a script's own elevation values there are discarded.
    // This guarantees parent/child continuity regardless of how the script
    // computed its field, mirroring the guarantee ChildGenerator's C++ path
    // gets for free from bilinear(parent) + fade-to-zero detail.
    void setBiomeField(int w, int h,
                       const std::vector<float>& elevation,
                       const std::vector<float>& temperature,
                       const std::vector<float>& moisture);

    // --- vector features (see Map::MapFeature) ---
    // Continent center marker (FeatureType::Continent), for the level-0
    // planet generator (M095, mirrors PlanetGenerator.cpp's continent
    // features, which the C++ generator sets without a builder helper).
    void addContinent(const std::string& name, double x, double z);
    void addRiver(const std::string& name, const std::vector<std::array<double, 2>>& path);
    void addMountainRange(const std::string& name, const std::vector<std::array<double, 2>>& ridge);
    // size_hint: "" or "city" -> FeatureType::City; "town" -> FeatureType::Town.
    void addCity(const std::string& name, double x, double z, const std::string& size_hint = "");
    void addBorder(const std::string& country, const std::vector<std::array<double, 2>>& polygon);
    // FeatureType::Road existed in the enum, unused by any generator until
    // country.lua (M146, MAP9) -- same situation addBorder/addMountainRange/
    // addCity were each in before their own first caller arrived.
    void addRoad(const std::string& name, const std::vector<std::array<double, 2>>& path);
    // FeatureType::Lake existed in the enum (used by FeatureNaming's C++-side
    // Hydrology export, M132) but had no Lua-facing builder call until
    // metro.lua (M147, MAP9) -- same "previously unused by any Lua
    // generator" situation addRoad was just in.
    void addLake(const std::string& name, const std::vector<std::array<double, 2>>& shoreline);
    // FeatureType::Street (MAP10, M153) -- a city-scale street, distinct
    // from FeatureType::Road's inter-settlement trunk roads (MAP9):
    // different scale/semantics, same shape (a named path), so a new type
    // rather than overloading Road's meaning across two very different
    // levels of the quadtree.
    void addStreet(const std::string& name, const std::vector<std::array<double, 2>>& path);
    // FeatureType::Park (MAP10, M153) -- a named green-space marker within
    // a city tile. Single center point only, same "v1: a point, not a
    // footprint polygon" choice metro.lua's (M147) city footprint already
    // made -- real park-boundary geometry is out of scope here.
    void addPark(const std::string& name, double x, double z);

    // --- zoning override (MAP10, M153) ---
    // BiomeClassifier::classify() (called inside setBiomeField()) only ever
    // classifies natural terrain -- it has no "city" output. Urbanizing a
    // cell is therefore an explicit override layered on top of
    // setBiomeField()'s own result, the same "override after the default"
    // shape setEdge() already established for TileEdge. `mask` must be the
    // same w*h size setBiomeField() was given (row-major, index = gy*w+gx,
    // matching every other field-grid convention in this class); every
    // non-zero entry marks that cell Map::ZoneType::city (`Map::BiomeGrid`
    // stores raw ZoneType ordinals, see its own doc comment). A no-op for
    // any index setBiomeField() was never given (mask longer than the
    // current biome grid, or called before setBiomeField()).
    void markUrbanCells(const std::vector<std::uint8_t>& mask);

    // --- zone candidates (MAP10, M156) ---
    // `mask` holds one Map::ZoneCandidate ordinal per cell, same w*h
    // row-major shape as setBiomeField()'s field tables (must be called
    // after setBiomeField() -- sizes payload_.zone_candidates from the
    // current biome grid's w/h). Unlike markUrbanCells() (an override
    // layered on top of an existing natural classification), there is no
    // pre-existing default to layer onto here, so this call REPLACES the
    // whole grid each time: any index setBiomeField() was never given
    // stays Map::ZoneCandidate::none; `mask` longer than the current
    // biome grid is truncated, not out-of-bounds. See Map::ZoneCandidate's
    // own doc comment for why this is a Map::-native enum, not the legacy
    // chunk-system RegionType.
    void setZoneCandidates(const std::vector<std::uint8_t>& mask);

    // --- boundary export (constraint propagation to children, see map.md §7) ---
    // edge: "N"/"E"/"S"/"W". Exported for the child generator to read as a
    // fixed boundary condition (mirrors MapTilePayload::edges[]). setBiomeField()
    // already default-populates all 4 edges from this tile's own final
    // elevation (M115) — call setEdge() only to override a side with
    // something setBiomeField() couldn't infer (e.g. crossings, M110/M111).
    void setEdge(const std::string& edge, const std::vector<float>& elevation);

    // --- metadata ---
    void setMetadata(const std::string& generator_id, const std::string& culture);

    // --- edge crossings (MAP10, M162) ---
    // Scans every already-added Street/Road feature's POINTS for ones lying
    // on this tile's own 4 world-space boundary lines. Deliberately a
    // point-on-boundary check, not a segment/tile-edge intersection test
    // like Roads::exportCrossings() (src/Map/RoadNetwork.cpp) uses — that
    // class's paths span continuous world coordinates across many tiles,
    // but every Street/Road feature collected through THIS builder is
    // validated (MapValidator) to stay strictly within its own tile's
    // bounds, so it can only ever touch an edge, never cross through it.
    // Called automatically by LuaSandbox::executeMap() once the generator
    // script has finished (so every addStreet()/addRoad() call has already
    // happened) — not script-visible, the same "framework derives
    // incidental structural data" shape applyParentEdgeConstraints()/
    // deriveEdgesFromElevation() already established for elevation-derived
    // edges. Idempotent: clears and recomputes rather than appending, safe
    // to call more than once.
    //
    // KNOWN LIMITATION: as of M162, no real script's own streets actually
    // reach their tile's boundary -- city.lua's/neighborhood.lua's street
    // endpoints (align_grid_line(), M155) use a cell-center formula that
    // stops half a cell short of every edge, so this mechanism, while
    // correctly implemented and unit-tested, has no effect on any
    // currently-generated world yet. See NEXT.md for the full writeup.
    void deriveEdgeCrossings();

    // --- hydrology (MAP19, M313) ---
    // Thin wrappers over Map::Hydrology (M121-M125, see its own doc comments
    // for the actual tracing/carving algorithm) that supply this builder's
    // own sea_level_m_/payload_.tile.world_bounds() automatically, so a
    // script never has to re-derive or re-pass values this builder already
    // knows -- the same reasoning setBiomeField() already applies to
    // sea_level_m_ via BiomeClassifier::classify(). Operate on a caller-
    // supplied `elevation` grid, NOT payload_.elevation: rivers must be
    // traced/carved into a script's own in-progress elevation BEFORE that
    // grid is handed to setBiomeField() (carving changes elevation, which
    // would otherwise need biome to be reclassified again). Pure
    // pass-through to Hydrology; this builder does not retain the returned
    // network -- a script must feed it into appendHydrologyFeatures() itself
    // to have it recorded, mirroring how a traced-but-undescribed network is
    // otherwise silently discarded (see FeatureNaming.hpp's own doc comment).
    Map::HydrologyNetwork traceRivers(const Map::FieldGrid& elevation) const;
    void carveRivers(Map::FieldGrid& elevation, const Map::HydrologyNetwork& network) const;
    // Appends a MapFeature::River/Lake per Map::FeatureNaming::
    // appendHydrologyFeatures() (M132/M316) directly into this builder's
    // payload -- the Lua-facing completion of traceRivers()/carveRivers(),
    // since neither of those two produces named features on its own.
    void appendHydrologyFeatures(const Map::HydrologyNetwork& network, const std::string& culture,
                                  std::uint64_t entropy, int min_river_points = 5);

    // --- mountain ranges (MAP19, M314) ---
    // Same "supply sea_level_m_/world_bounds() automatically" wrapper shape
    // as the hydrology block above, over Map::MountainRanges (M126/M127).
    // generateMountainRanges() is entropy-seeded and independent of any
    // elevation grid; applyMountainRanges() then uplifts a caller-supplied
    // `elevation` grid in place, same "operate on the script's own
    // in-progress grid, before setBiomeField()" reasoning as carveRivers().
    Map::MountainRangeNetwork generateMountainRanges(std::uint64_t entropy, int count,
                                                       double min_peak_elevation_m,
                                                       double max_peak_elevation_m) const;
    void applyMountainRanges(Map::FieldGrid& elevation, const Map::MountainRangeNetwork& network,
                              double falloff_width_m) const;
    // Appends a MapFeature::MountainRange per Map::FeatureNaming::
    // appendMountainRangeFeatures() (M132/M316) directly into this builder's
    // payload. `elevation` is only used for appendMountainRangeFeatures()'s
    // own land-cell check (see its doc comment) -- pass the same grid
    // applyMountainRanges() was given (post-uplift or pre-uplift both work,
    // since the check only needs land vs. ocean, and uplift never crosses
    // that boundary).
    void appendMountainRangeFeatures(const Map::MountainRangeNetwork& network,
                                      const Map::FieldGrid& elevation, const std::string& culture,
                                      std::uint64_t entropy);

    // --- volcanism (MAP16, M265-268) ---
    // Same "supply sea_level_m_/world_bounds() automatically" wrapper shape
    // as the mountain-range block above, over Map::Volcanism. No
    // appendVolcanicFeatures()-style equivalent (unlike mountain
    // ranges/hydrology) -- a hotspot's presence is expressed entirely as
    // biome data (applyVolcanicBiomes() below) plus its elevation
    // contribution, not a separate named MapFeature; nothing in
    // FeatureNaming names individual volcanoes.
    Map::VolcanicField generateVolcanicHotspots(std::uint64_t entropy, int count,
                                                  double min_peak_elevation_m,
                                                  double max_peak_elevation_m) const;
    void applyVolcanism(Map::FieldGrid& elevation, const Map::VolcanicField& field) const;

    // --- biome refinement (MAP19, M315) ---
    // Thin wrappers over Map::BiomeRefinement (M128/M130). Unlike the
    // hydrology/mountain wrappers above, these operate on THIS builder's own
    // payload_.elevation/payload_.biome in place, rather than a
    // caller-supplied grid -- BiomeRefinement is explicitly a *post*-
    // classification neighbor-aware pass (see its own doc comment), so it
    // only makes sense to run after setBiomeField() has already populated
    // both grids. A no-op if called before setBiomeField() (payload_.biome
    // is still empty, same "not yet populated" tolerance
    // markUrbanCells()/setZoneCandidates() already have).
    void applyCoastalBeach(int radius_cells = 1, double max_beach_elevation_m = 50.0);
    void applySwampFlatnessCheck(double max_local_relief_m = 150.0);
    // M259/M274/M275 (MAP16, deferred-biome unlock) -- same "post-
    // classification, no-op before setBiomeField()" contract as the two
    // wrappers above.
    void applyCanyonCarving(double steep_relief_m = 80.0);
    void applyCoastalReliefRefinement(int radius_cells = 1, double max_coastal_elevation_m = 50.0,
                                       double flat_relief_m = 15.0, double steep_relief_m = 80.0);
    // M247 (2026-07-11) -- same "post-classification" contract, but also
    // needs a traced river `network` (see traceRivers() above); supplies
    // this builder's own biome grid dimensions + world_bounds()
    // automatically (same "don't make a script re-derive what this
    // builder already knows" reasoning appendHydrologyFeatures() uses).
    void applyRiparianForest(const Map::HydrologyNetwork& network, int radius_cells = 1);
    // M265-268 (2026-07-11) -- same "post-classification" contract; also
    // needs a `field` of generated hotspots (see generateVolcanicHotspots()
    // above), same "the caller must trace/generate it first, this only
    // consumes it" pattern applyRiparianForest() established for rivers.
    void applyVolcanicBiomes(const Map::VolcanicField& field, int coastal_radius_cells = 1,
                              double inner_fraction = 0.4);

    // --- finalize ---
    // Returns the accumulated payload. Idempotent — safe to call more than
    // once; it does not consume/move-from the builder's state.
    const Map::MapTilePayload& payload() const { return payload_; }

private:
    void applyParentEdgeConstraints();
    void deriveEdgesFromElevation();

    Map::MapTilePayload        payload_;
    double                     sea_level_m_;
    const Map::MapTilePayload* parent_;
};

} // namespace MeshWorld
