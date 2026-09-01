// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "MapBuilder.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>

#include "Map/BiomeClassifier.hpp"
#include "Map/BiomeRefinement.hpp"
#include "Map/FeatureNaming.hpp"
#include "Map/Noise.hpp"
#include "Map/ZoneCandidate.hpp"

namespace MeshWorld {

namespace {

// M331 (MAP21) — decorrelates the cave-cavity noise channel from every
// other hash2i(...)-seeded channel in this codebase (river/lake/mountain
// naming use 601/602/603 as their own axis constants, see
// FeatureNaming.cpp; this just needs to be similarly distinct, not part of
// that same numbering scheme).
constexpr std::uint64_t kCaveCavitySeedSalt = 0x9CAF0000ULL;

// Real physical size (metres) of one Worley cell -- i.e. the rough diameter
// of a single cave pocket, chosen as a plausible real-world cave-system
// scale. Deliberately a fixed METRE value, not scaled by tile/level: caves
// are real, roughly constant-size physical features, unlike continents or
// mountain ranges (whose natural scale genuinely does track the tile
// they're generated at). One consequence: at coarse levels (a planet-root
// cell spans ~350 km) this noise is sampled at a resolution far finer than
// one cell, so it is effectively a single, mostly-uniform sample per cell
// there -- caves become vanishingly rare at planet/continent scale and only
// meaningfully reachable at metro/city/neighborhood-scale tiles, which is
// the physically sensible outcome, not something this function special-
// cases by level.
constexpr double kCaveNoiseScaleM = 800.0;

// Worley (cellular) F1 noise, not fBm: caves are naturally clustered pockets
// (a handful of "cavity centers" with sparse territory around them), which
// is exactly the cellular-distance shape Worley noise produces -- fBm would
// scatter cave-eligible cells uniformly at random ("salt and pepper")
// instead of into the kind of localized pockets a real cave SYSTEM forms.
// Returns a 0..1 score (1.0 = sits exactly on a cellular seed point, the
// "quality"/cave chance) fed into BiomeClassifier::classify()'s own
// cavity_noise parameter -- see CAVE_CAVITY_THRESHOLD there for why this is
// checked against a value close to 1.0 (caves must stay a small minority of
// mountain-tier cells).
double cavity_score(double world_x, double world_z, std::uint64_t entropy) {
    const float worley = Map::noise::worley_f1(world_x / kCaveNoiseScaleM,
                                                world_z / kCaveNoiseScaleM,
                                                entropy ^ kCaveCavitySeedSalt);
    return 1.0 - std::min(static_cast<double>(worley), 1.0);
}

int edge_index(const std::string& edge) {
    if (edge == "N") return 0;
    if (edge == "E") return 1;
    if (edge == "S") return 2;
    if (edge == "W") return 3;
    assert(false && "MapBuilder::setEdge: edge must be one of N/E/S/W");
    return 0;
}

// Linearly resamples src (length >= 1) to a new length dst_len, preserving
// the first/last values exactly. Used to fit a parent edge (or edge half,
// see parent_edge_half below) onto this tile's own grid resolution when the
// two don't match sample-for-sample.
std::vector<float> resample1d(const std::vector<float>& src, int dst_len) {
    if (src.empty() || dst_len <= 0) return {};
    if (src.size() == 1 || dst_len == 1)
        return std::vector<float>(static_cast<std::size_t>(dst_len), src.front());

    std::vector<float> out(static_cast<std::size_t>(dst_len));
    const double scale = static_cast<double>(src.size() - 1) / static_cast<double>(dst_len - 1);
    for (int i = 0; i < dst_len; ++i) {
        const double pos = static_cast<double>(i) * scale;
        const auto   i0  = static_cast<std::size_t>(pos);
        const auto   i1  = std::min(i0 + 1, src.size() - 1);
        const double t   = pos - static_cast<double>(i0);
        out[static_cast<std::size_t>(i)] = static_cast<float>(src[i0] * (1.0 - t) + src[i1] * t);
    }
    return out;
}

// The half of a parent edge array this tile's quadrant is adjacent to
// (half_index 0 = the lower-index half, 1 = the upper-index half) — mirrors
// ChildGenerator's own cx*32/cy*32 split of the parent field (M109).
std::vector<float> parent_edge_half(const std::vector<float>& src, int half_index) {
    if (src.empty()) return {};
    const std::size_t n   = src.size();
    const std::size_t mid = n / 2;
    const std::size_t lo  = half_index == 0 ? std::size_t{0} : mid;
    const std::size_t hi  = half_index == 0 ? mid : n - 1;
    return std::vector<float>(src.begin() + static_cast<std::ptrdiff_t>(lo),
                               src.begin() + static_cast<std::ptrdiff_t>(hi) + 1);
}

// M162 — a "crossing" here means a Street/Road feature POINT lying on one
// of the tile's own 4 boundary lines, NOT a segment passing through and
// beyond one (unlike Roads::exportCrossings()'s own segment/tile-edge
// intersection geometry, src/Map/RoadNetwork.cpp — that class's paths span
// continuous world coordinates across many tiles, unconstrained by any
// single tile's bounds). Every Street/Road feature collected through
// MapBuilder is validated by MapValidator to stay strictly within its OWN
// tile's bounds (half-open: min_x/min_z inclusive, max_x/max_z exclusive,
// see MapValidator.cpp's check_features_in_bounds()) — a script has no way
// to know what lies beyond its own tile, so its streets can only ever
// TOUCH an edge, never cross through it. kBoundaryEpsilonM tolerates
// floating-point roundoff in a script's own boundary-snapping arithmetic.
constexpr double kBoundaryEpsilonM = 0.01;

void check_point_on_boundaries(const std::array<double, 2>& p, const Map::WorldBounds& bounds,
                                std::array<Map::TileEdge, 4>& edges) {
    const double w = bounds.max_x - bounds.min_x;
    const double h = bounds.max_z - bounds.min_z;
    if (w <= 0.0 || h <= 0.0) return;

    const bool in_x_span = p[0] >= bounds.min_x - kBoundaryEpsilonM && p[0] <= bounds.max_x + kBoundaryEpsilonM;
    const bool in_z_span = p[1] >= bounds.min_z - kBoundaryEpsilonM && p[1] <= bounds.max_z + kBoundaryEpsilonM;

    if (in_x_span && std::abs(p[1] - bounds.min_z) <= kBoundaryEpsilonM) {  // N
        edges[0].crossings.push_back(
            {Map::EdgeCrossingType::Road, static_cast<float>((p[0] - bounds.min_x) / w)});
    }
    if (in_x_span && std::abs(p[1] - bounds.max_z) <= kBoundaryEpsilonM) {  // S
        edges[2].crossings.push_back(
            {Map::EdgeCrossingType::Road, static_cast<float>((p[0] - bounds.min_x) / w)});
    }
    if (in_z_span && std::abs(p[0] - bounds.min_x) <= kBoundaryEpsilonM) {  // W
        edges[3].crossings.push_back(
            {Map::EdgeCrossingType::Road, static_cast<float>((p[1] - bounds.min_z) / h)});
    }
    if (in_z_span && std::abs(p[0] - bounds.max_x) <= kBoundaryEpsilonM) {  // E
        edges[1].crossings.push_back(
            {Map::EdgeCrossingType::Road, static_cast<float>((p[1] - bounds.min_z) / h)});
    }
}

} // namespace

MapBuilder::MapBuilder(const Map::TileCoord& tile, std::uint64_t entropy, double sea_level_m,
                        const Map::MapTilePayload* parent)
    : sea_level_m_(sea_level_m), parent_(parent) {
    payload_.tile    = tile;
    payload_.entropy = entropy;
}

void MapBuilder::setBiomeField(int w, int h,
                                const std::vector<float>& elevation,
                                const std::vector<float>& temperature,
                                const std::vector<float>& moisture) {
    const auto count = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    assert(elevation.size() == count && temperature.size() == count && moisture.size() == count
           && "MapBuilder::setBiomeField: grids must all be w*h");

    payload_.elevation.w    = w;
    payload_.elevation.h    = h;
    payload_.elevation.data = elevation;

    payload_.temperature.w    = w;
    payload_.temperature.h    = h;
    payload_.temperature.data = temperature;

    payload_.moisture.w    = w;
    payload_.moisture.h    = h;
    payload_.moisture.data = moisture;

    // M331 (MAP21) -- real world-space bounds for the cave cavity-noise
    // score below (cavity_score() needs actual world coordinates, not just
    // a grid index -- it's genuine spatial noise, not a function of the
    // other 3 scalars). Every real generator (every C++ path and all 7
    // scripted Lua levels) funnels through this one setBiomeField() call,
    // so ZoneType::cave becomes reachable everywhere for free -- no script
    // or other generator needed any change.
    const auto   bounds = payload_.tile.world_bounds();
    const double cell_w = (bounds.max_x - bounds.min_x) / static_cast<double>(w);
    const double cell_h = (bounds.max_z - bounds.min_z) / static_cast<double>(h);

    // Derive biome per-cell, same as PlanetGenerator/ChildGenerator do inline.
    payload_.biome.w = w;
    payload_.biome.h = h;
    payload_.biome.data.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        const int    gx = static_cast<int>(i % static_cast<std::size_t>(w));
        const int    gy = static_cast<int>(i / static_cast<std::size_t>(w));
        const double wx = bounds.min_x + (gx + 0.5) * cell_w;
        const double wz = bounds.min_z + (gy + 0.5) * cell_h;
        const double cavity = cavity_score(wx, wz, payload_.entropy);

        const auto zone = Map::BiomeClassifier::classify(
            elevation[i], temperature[i], moisture[i], sea_level_m_, cavity);
        payload_.biome.data[i] = static_cast<std::uint8_t>(zone);
    }

    applyParentEdgeConstraints();
    deriveEdgesFromElevation();
}

// M115 — default-populate the outgoing edge descriptors from this tile's own
// FINAL elevation (i.e. after applyParentEdgeConstraints() above), mirroring
// PlanetGenerator.cpp/ChildGenerator.cpp's own tail logic. Without this, a
// script that computes setEdge() from its own pre-constraint local array (as
// continent.lua originally did) exports edges inconsistent with what
// M108 actually stored in payload_.elevation. A script can still override any
// side afterwards via setEdge() (e.g. once crossings are added, M110/M111).
void MapBuilder::deriveEdgesFromElevation() {
    if (payload_.elevation.empty()) return;

    const int W = payload_.elevation.w;
    const int H = payload_.elevation.h;

    auto& en = payload_.edges[0]; en.elevation.resize(static_cast<std::size_t>(W));
    auto& ee = payload_.edges[1]; ee.elevation.resize(static_cast<std::size_t>(H));
    auto& es = payload_.edges[2]; es.elevation.resize(static_cast<std::size_t>(W));
    auto& ew = payload_.edges[3]; ew.elevation.resize(static_cast<std::size_t>(H));
    for (int i = 0; i < W; ++i) {
        en.elevation[static_cast<std::size_t>(i)] = payload_.elevation.at(i, 0);
        es.elevation[static_cast<std::size_t>(i)] = payload_.elevation.at(i, H - 1);
    }
    for (int i = 0; i < H; ++i) {
        ee.elevation[static_cast<std::size_t>(i)] = payload_.elevation.at(W - 1, i);
        ew.elevation[static_cast<std::size_t>(i)] = payload_.elevation.at(0, i);
    }
}

// M108 — a child tile must consume its parent's edge descriptors as a fixed
// boundary condition. Overwrites the two boundary rows/columns of this
// tile's elevation (and re-derives biome there) that coincide with the
// parent's own N/E/S/W boundary; the other two boundaries are internal to
// the parent and are sibling-coherence territory (M112), not this task's.
void MapBuilder::applyParentEdgeConstraints() {
    if (parent_ == nullptr || payload_.elevation.empty()) return;

    const int W  = payload_.elevation.w;
    const int H  = payload_.elevation.h;
    const int cx = static_cast<int>(payload_.tile.x % 2);
    const int cy = static_cast<int>(payload_.tile.y % 2);

    const auto write_cell = [&](int gx, int gy, float elev) {
        const auto idx = static_cast<std::size_t>(gy) * static_cast<std::size_t>(W)
                        + static_cast<std::size_t>(gx);
        payload_.elevation.data[idx] = elev;
        if (!payload_.biome.empty()) {
            const double temp  = payload_.temperature.empty() ? 0.0 : payload_.temperature.data[idx];
            const double moist = payload_.moisture.empty()    ? 0.0 : payload_.moisture.data[idx];
            // M331: cavity_noise deliberately left at its 0.0 default here
            // -- this reclassifies the 2 boundary rows/columns M108's
            // parent-edge constraint just overwrote, which prioritizes
            // elevation continuity with the parent over cave-eligibility;
            // a cave that happened to want to straddle a tile boundary is
            // an edge case not worth the extra gx/gy/world-position
            // plumbing this narrow lambda doesn't otherwise need.
            payload_.biome.data[idx] = static_cast<std::uint8_t>(
                Map::BiomeClassifier::classify(elev, temp, moist, sea_level_m_));
        }
    };

    // N/S: exactly one applies, selected by which half of the parent this
    // tile occupies (cy==0 -> parent's N edge; cy==1 -> parent's S edge). The
    // N/S edge array is indexed along X (the parent's full width), and this
    // tile only spans HALF of that width -- which half is picked by cx, not
    // cy (cy already chose N vs S; it says nothing about the X sub-range).
    // Found 2026-07-12: this used cy for both the edge AND the half, so any
    // tile with cx != cy (2 of the 4 quadrant combinations) silently got the
    // WRONG half of its own parent's N/S edge here -- a real, previously
    // undetected sibling-boundary-mismatch bug (visible as a terrain seam),
    // caught by MapPipelineTest.SiblingSharedEdgeOrderIndependent.
    const auto& row_src = cy == 0 ? parent_->edges[0].elevation : parent_->edges[2].elevation;
    const auto  row     = resample1d(parent_edge_half(row_src, cx == 0 ? 0 : 1), W);
    const int   row_y   = cy == 0 ? 0 : H - 1;
    for (int gx = 0; gx < W && gx < static_cast<int>(row.size()); ++gx)
        write_cell(gx, row_y, row[static_cast<std::size_t>(gx)]);

    // E/W: exactly one applies (cx==0 -> parent's W edge; cx==1 -> parent's E
    // edge). Symmetric to the above: the E/W edge array is indexed along Z
    // (the parent's full height), so the half this tile occupies is picked
    // by cy, not cx.
    const auto& col_src = cx == 0 ? parent_->edges[3].elevation : parent_->edges[1].elevation;
    const auto  col     = resample1d(parent_edge_half(col_src, cy == 0 ? 0 : 1), H);
    const int   col_x   = cx == 0 ? 0 : W - 1;
    for (int gy = 0; gy < H && gy < static_cast<int>(col.size()); ++gy)
        write_cell(col_x, gy, col[static_cast<std::size_t>(gy)]);
}

void MapBuilder::addContinent(const std::string& name, double x, double z) {
    Map::MapFeature f;
    f.type   = Map::FeatureType::Continent;
    f.name   = name;
    f.points = {{x, z}};
    payload_.features.push_back(std::move(f));
}

void MapBuilder::addRiver(const std::string& name, const std::vector<std::array<double, 2>>& path) {
    Map::MapFeature f;
    f.type   = Map::FeatureType::River;
    f.name   = name;
    f.points = path;
    payload_.features.push_back(std::move(f));
}

void MapBuilder::addMountainRange(const std::string& name,
                                   const std::vector<std::array<double, 2>>& ridge) {
    Map::MapFeature f;
    f.type   = Map::FeatureType::MountainRange;
    f.name   = name;
    f.points = ridge;
    payload_.features.push_back(std::move(f));
}

void MapBuilder::addCity(const std::string& name, double x, double z,
                          const std::string& size_hint) {
    Map::MapFeature f;
    f.type   = (size_hint == "town") ? Map::FeatureType::Town : Map::FeatureType::City;
    f.name   = name;
    f.points = {{x, z}};
    payload_.features.push_back(std::move(f));
}

void MapBuilder::addBorder(const std::string& country,
                            const std::vector<std::array<double, 2>>& polygon) {
    Map::MapFeature f;
    f.type   = Map::FeatureType::Border;
    f.name   = country;
    f.points = polygon;
    payload_.features.push_back(std::move(f));
}

void MapBuilder::addRoad(const std::string& name, const std::vector<std::array<double, 2>>& path) {
    Map::MapFeature f;
    f.type   = Map::FeatureType::Road;
    f.name   = name;
    f.points = path;
    payload_.features.push_back(std::move(f));
}

void MapBuilder::addLake(const std::string& name, const std::vector<std::array<double, 2>>& shoreline) {
    Map::MapFeature f;
    f.type   = Map::FeatureType::Lake;
    f.name   = name;
    f.points = shoreline;
    payload_.features.push_back(std::move(f));
}

void MapBuilder::addStreet(const std::string& name, const std::vector<std::array<double, 2>>& path) {
    Map::MapFeature f;
    f.type   = Map::FeatureType::Street;
    f.name   = name;
    f.points = path;
    payload_.features.push_back(std::move(f));
}

void MapBuilder::addPark(const std::string& name, double x, double z) {
    Map::MapFeature f;
    f.type   = Map::FeatureType::Park;
    f.name   = name;
    f.points = {{x, z}};
    payload_.features.push_back(std::move(f));
}

void MapBuilder::markUrbanCells(const std::vector<std::uint8_t>& mask) {
    const std::size_t n = std::min(mask.size(), payload_.biome.data.size());
    for (std::size_t i = 0; i < n; ++i)
        if (mask[i] != 0) payload_.biome.data[i] = static_cast<std::uint8_t>(ZoneType::city);
}

void MapBuilder::setZoneCandidates(const std::vector<std::uint8_t>& mask) {
    if (payload_.biome.empty()) return;
    payload_.zone_candidates.w = payload_.biome.w;
    payload_.zone_candidates.h = payload_.biome.h;
    payload_.zone_candidates.data.assign(payload_.biome.data.size(),
                                         static_cast<std::uint8_t>(Map::ZoneCandidate::none));
    const std::size_t n = std::min(mask.size(), payload_.zone_candidates.data.size());
    for (std::size_t i = 0; i < n; ++i) payload_.zone_candidates.data[i] = mask[i];
}

void MapBuilder::setEdge(const std::string& edge, const std::vector<float>& elevation) {
    payload_.edges[static_cast<std::size_t>(edge_index(edge))].elevation = elevation;
}

void MapBuilder::setMetadata(const std::string& generator_id, const std::string& culture) {
    payload_.generator = generator_id;
    payload_.culture   = culture;
}

Map::HydrologyNetwork MapBuilder::traceRivers(const Map::FieldGrid& elevation) const {
    const auto bounds = payload_.tile.world_bounds();
    return Map::Hydrology::trace(elevation, sea_level_m_, bounds.min_x, bounds.min_z, bounds.max_x,
                                  bounds.max_z);
}

void MapBuilder::carveRivers(Map::FieldGrid& elevation, const Map::HydrologyNetwork& network) const {
    const auto bounds = payload_.tile.world_bounds();
    Map::Hydrology::carve(elevation, network, sea_level_m_, bounds.min_x, bounds.min_z, bounds.max_x,
                           bounds.max_z);
}

void MapBuilder::appendHydrologyFeatures(const Map::HydrologyNetwork& network,
                                          const std::string& culture, std::uint64_t entropy,
                                          int min_river_points) {
    Map::FeatureNaming::appendHydrologyFeatures(payload_.features, network, culture, entropy,
                                                 min_river_points);
}

Map::MountainRangeNetwork MapBuilder::generateMountainRanges(std::uint64_t entropy, int count,
                                                               double min_peak_elevation_m,
                                                               double max_peak_elevation_m) const {
    const auto bounds = payload_.tile.world_bounds();
    return Map::MountainRanges::generate(entropy, count, bounds.min_x, bounds.min_z, bounds.max_x,
                                          bounds.max_z, min_peak_elevation_m, max_peak_elevation_m);
}

void MapBuilder::applyMountainRanges(Map::FieldGrid& elevation, const Map::MountainRangeNetwork& network,
                                      double falloff_width_m) const {
    const auto bounds = payload_.tile.world_bounds();
    Map::MountainRanges::apply(elevation, network, sea_level_m_, falloff_width_m, bounds.min_x,
                                bounds.min_z, bounds.max_x, bounds.max_z);
}

void MapBuilder::appendMountainRangeFeatures(const Map::MountainRangeNetwork& network,
                                              const Map::FieldGrid& elevation,
                                              const std::string& culture, std::uint64_t entropy) {
    const auto bounds = payload_.tile.world_bounds();
    Map::FeatureNaming::appendMountainRangeFeatures(payload_.features, network, elevation,
                                                      sea_level_m_, culture, entropy, bounds.min_x,
                                                      bounds.min_z, bounds.max_x, bounds.max_z);
}

Map::VolcanicField MapBuilder::generateVolcanicHotspots(std::uint64_t entropy, int count,
                                                          double min_peak_elevation_m,
                                                          double max_peak_elevation_m) const {
    const auto bounds = payload_.tile.world_bounds();
    return Map::Volcanism::generate(entropy, count, bounds.min_x, bounds.min_z, bounds.max_x,
                                     bounds.max_z, min_peak_elevation_m, max_peak_elevation_m);
}

void MapBuilder::applyVolcanism(Map::FieldGrid& elevation, const Map::VolcanicField& field) const {
    const auto bounds = payload_.tile.world_bounds();
    Map::Volcanism::apply(elevation, field, sea_level_m_, bounds.min_x, bounds.min_z, bounds.max_x,
                           bounds.max_z);
}

void MapBuilder::applyCoastalBeach(int radius_cells, double max_beach_elevation_m) {
    if (payload_.biome.empty()) return;
    Map::BiomeRefinement::applyCoastalBeach(payload_.biome, payload_.elevation, sea_level_m_,
                                             radius_cells, max_beach_elevation_m);
}

void MapBuilder::applySwampFlatnessCheck(double max_local_relief_m) {
    if (payload_.biome.empty()) return;
    Map::BiomeRefinement::applySwampFlatnessCheck(payload_.biome, payload_.elevation,
                                                    max_local_relief_m);
}

void MapBuilder::applyCanyonCarving(double steep_relief_m) {
    if (payload_.biome.empty()) return;
    Map::BiomeRefinement::applyCanyonCarving(payload_.biome, payload_.elevation, steep_relief_m);
}

void MapBuilder::applyCoastalReliefRefinement(int radius_cells, double max_coastal_elevation_m,
                                               double flat_relief_m, double steep_relief_m) {
    if (payload_.biome.empty()) return;
    Map::BiomeRefinement::applyCoastalReliefRefinement(payload_.biome, payload_.elevation,
                                                         sea_level_m_, radius_cells,
                                                         max_coastal_elevation_m, flat_relief_m,
                                                         steep_relief_m);
}

void MapBuilder::applyRiparianForest(const Map::HydrologyNetwork& network, int radius_cells) {
    if (payload_.biome.empty()) return;
    const auto bounds = payload_.tile.world_bounds();
    Map::BiomeRefinement::applyRiparianForest(payload_.biome, network, payload_.biome.w,
                                               payload_.biome.h, bounds.min_x, bounds.min_z,
                                               bounds.max_x, bounds.max_z, radius_cells);
}

void MapBuilder::applyVolcanicBiomes(const Map::VolcanicField& field, int coastal_radius_cells,
                                      double inner_fraction) {
    if (payload_.biome.empty()) return;
    const auto bounds = payload_.tile.world_bounds();
    Map::BiomeRefinement::applyVolcanicBiomes(payload_.biome, field, payload_.biome.w,
                                               payload_.biome.h, bounds.min_x, bounds.min_z,
                                               bounds.max_x, bounds.max_z, coastal_radius_cells,
                                               inner_fraction);
}

void MapBuilder::deriveEdgeCrossings() {
    for (auto& e : payload_.edges) e.crossings.clear();  // idempotent: recompute, don't append

    const auto bounds = payload_.tile.world_bounds();
    for (const auto& f : payload_.features) {
        if (f.type != Map::FeatureType::Street && f.type != Map::FeatureType::Road) continue;
        for (const auto& p : f.points) check_point_on_boundaries(p, bounds, payload_.edges);
    }
}

} // namespace MeshWorld
