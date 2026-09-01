// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "generators/map/PlanetGenerator.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include "Map/BiomeClassifier.hpp"
#include "Map/BiomeRefinement.hpp"
#include "Map/Coastline.hpp"
#include "Map/FeatureNaming.hpp"
#include "Map/Hydrology.hpp"
#include "Map/MountainRanges.hpp"
#include "Map/Noise.hpp"
#include "Map/Volcanism.hpp"
#include "Naming.hpp"

namespace MeshWorld::Map {

namespace {
static constexpr double PI        = 3.14159265358979323846;
static constexpr int    GRID_SIZE = 64;
} // namespace

PlanetGenerator::PlanetGenerator(PlanetParams params) : params_(std::move(params)) {}

int PlanetGenerator::continent_count(std::uint64_t entropy) const {
    const int lo   = params_.continents_min;
    const int hi   = std::max(lo, params_.continents_max);
    const int span = hi - lo + 1;
    return lo + static_cast<int>(entropy % static_cast<std::uint64_t>(span));
}

MapTilePayload PlanetGenerator::generate(const TileCoord&      tile,
                                         const MapTilePayload* /*parent*/,
                                         std::uint64_t         entropy) const {
    MapTilePayload p;
    p.tile      = tile;
    p.entropy   = entropy;
    p.generator = "planet";
    // M132 — every C++-generated payload previously left `culture` at its
    // default-constructed "" (only the Lua path ever set it, via
    // MapBuilder::setMetadata()), so Naming::river()/lake()/mountain()
    // below would always silently fall back to "nordic". Derive a real,
    // tile-varying culture instead.
    p.culture = MeshWorld::Naming::culture(entropy);

    // M053 — seed continent centers.
    const int n = continent_count(entropy);
    p.features.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        const std::uint64_t h  = noise::hash2i(i, 0, entropy);
        const double        cx = static_cast<double>(noise::to_unit_float(h)) * params_.planet_size_m;
        const double        cy = static_cast<double>(
                                     noise::to_unit_float(h ^ 0x9e3779b97f4a7c15ULL)) *
                                 params_.planet_size_m;

        MapFeature continent;
        continent.type   = FeatureType::Continent;
        continent.points = {{cx, cy}};
        p.features.push_back(std::move(continent));
    }

    // M054 — per-continent base radius scaled so the expected non-overlapping
    // coverage is ~35 % of the planet square, independent of continent count.
    // Overlap and coastline noise reduce actual land to roughly 20–35 %.
    const double target_coverage = 0.35;
    const double base_r = std::sqrt(
        target_coverage * params_.planet_size_m * params_.planet_size_m
        / (PI * static_cast<double>(n)));
    std::vector<double> radii(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        const std::uint64_t h = noise::hash2i(i, 2, entropy);  // axis 2 = radius
        radii[static_cast<std::size_t>(i)] = base_r * (0.70 + 0.60 * noise::to_unit_float(h));
    }

    // Noise scales: coastline variation at ~1/4 of planet size; elevation at ~1/8.
    const double coast_scale   = std::max(params_.planet_size_m / 4.0, 1.0);
    const double terrain_scale = std::max(params_.planet_size_m / 8.0, 1.0);
    const double sea_m         = params_.sea_level_m;

    FieldGrid elev;
    elev.w = elev.h = GRID_SIZE;
    elev.data.assign(static_cast<std::size_t>(GRID_SIZE * GRID_SIZE), 0.0f);

    FieldGrid temp;
    temp.w = temp.h = GRID_SIZE;
    temp.data.assign(static_cast<std::size_t>(GRID_SIZE * GRID_SIZE), 0.0f);

    FieldGrid moist;
    moist.w = moist.h = GRID_SIZE;
    moist.data.assign(static_cast<std::size_t>(GRID_SIZE * GRID_SIZE), 0.0f);

    BiomeGrid bio;
    bio.w = bio.h = GRID_SIZE;
    bio.data.assign(static_cast<std::size_t>(GRID_SIZE * GRID_SIZE), std::uint8_t{0});

    for (int gy = 0; gy < GRID_SIZE; ++gy) {
        for (int gx = 0; gx < GRID_SIZE; ++gx) {
            const double wx = (gx + 0.5) * params_.planet_size_m / GRID_SIZE;
            const double wy = (gy + 0.5) * params_.planet_size_m / GRID_SIZE;

            // M054 — continent influence: max over all continents.
            // influence > 0 means the cell lies inside the noisy continent boundary.
            double max_inf = -1.0;
            for (int i = 0; i < n; ++i) {
                const auto  ci  = static_cast<std::size_t>(i);
                const double cx = p.features[ci].points[0][0];
                const double cy = p.features[ci].points[0][1];
                const double dx = wx - cx;
                const double dy = wy - cy;
                const double dist = std::sqrt(dx * dx + dy * dy);

                // Each continent gets its own coast-noise seed.
                const std::uint64_t coast_seed =
                    entropy ^ (static_cast<std::uint64_t>(i + 1) * 0xdeadbeef17263544ULL);
                const float coast_fbm = noise::fbm(wx / coast_scale, wy / coast_scale, coast_seed);
                // Effective radius varies ±20 % around the base radius.
                const double eff_r = radii[ci] * (1.0 + 0.4 * (coast_fbm - 0.5));
                const double inf   = 1.0 - dist / std::max(eff_r, 1.0);
                if (inf > max_inf) max_inf = inf;
            }

            // M055 — land/ocean mask: land where any continent claims the cell.
            const bool is_land = max_inf > 0.0;
            bio.data[static_cast<std::size_t>(gy * GRID_SIZE + gx)] =
                is_land ? std::uint8_t{1} : std::uint8_t{0};

            // M057 — coarse elevation field (continental uplift + fBm detail).
            const float terrain_fbm =
                noise::fbm(wx / terrain_scale, wy / terrain_scale, entropy + 1ULL);
            double elevation = 0.0;
            if (is_land) {
                const double uplift = std::min(max_inf, 1.0) * 1500.0;
                elevation = sea_m + 1.0 + uplift + (terrain_fbm - 0.5) * 2000.0;
                elevation = std::max(elevation, sea_m + 1.0);
            } else {
                const double depth = std::min(-max_inf, 1.0) * 4000.0;
                elevation = sea_m - 1.0 - depth + (terrain_fbm - 0.5) * 200.0;
                elevation = std::min(elevation, sea_m - 1.0);
            }
            elev.data[static_cast<std::size_t>(gy * GRID_SIZE + gx)] =
                static_cast<float>(elevation);

            // M059 — latitude temperature bands + elevation lapse rate.
            // "Latitude" = normalised Y distance from planet centre (0 = equator, 1 = pole).
            // Standard atmospheric lapse rate ≈ 6.5 °C per 1000 m above sea level.
            const double lat_factor  = std::abs((gy + 0.5) / GRID_SIZE - 0.5) * 2.0;
            const double base_temp   = params_.equator_temp_c
                                       + (params_.pole_temp_c - params_.equator_temp_c) * lat_factor;
            const double elev_above  = std::max(elevation - sea_m, 0.0);
            const double temperature = base_temp - 6.5 * elev_above / 1000.0;
            temp.data[static_cast<std::size_t>(gy * GRID_SIZE + gx)] =
                static_cast<float>(temperature);

            // M060 — moisture field: broad-scale fBm noise (own seed offset,
            // same coast_scale as the coastline/continent noise above — both
            // are landscape-scale features). Recomputed from world position
            // every level, same treatment as temperature (not inherited from
            // parent — see MapPipelineTests.cpp's FullChainBoundaryContinuity
            // ...Level0ToMax comment for why that's fine for these two fields).
            const float moisture = std::clamp(
                noise::fbm(wx / coast_scale, wy / coast_scale, entropy + 3ULL), 0.0f, 1.0f);
            moist.data[static_cast<std::size_t>(gy * GRID_SIZE + gx)] = moisture;

            // M061 — classify biome from (elevation, temperature, moisture).
            const auto zone = BiomeClassifier::classify(elevation, temperature, moisture, sea_m);
            bio.data[static_cast<std::size_t>(gy * GRID_SIZE + gx)] =
                static_cast<std::uint8_t>(zone);
        }
    }

    // M126/M127 — seed tectonic ridge lines and raise elevation along them
    // (interior cells only, same edge-protection rule as Hydrology::carve()
    // below). Applied before hydrology so rivers trace across the
    // now-mountainous terrain, matching real-world causality (uplift, then
    // erosion). Range count is entropy-driven, decorrelated from
    // continent_count's own entropy usage via a bit shift.
    const int mountain_range_count = 2 + static_cast<int>((entropy >> 8) % 4ULL);
    const MountainRangeNetwork mountain_net =
        MountainRanges::generate(entropy + 4ULL, mountain_range_count,
                                  0.0, 0.0, params_.planet_size_m, params_.planet_size_m,
                                  /*min_peak_elevation_m=*/1500.0, /*max_peak_elevation_m=*/4000.0);
    MountainRanges::apply(elev, mountain_net, sea_m, /*falloff_width_m=*/terrain_scale,
                           0.0, 0.0, params_.planet_size_m, params_.planet_size_m);

    // M265-268 (2026-07-11) — same "seed + uplift before hydrology" slot as
    // mountain ranges above, same sparse "at most 1 hotspot" count as
    // ChildGenerator.cpp's identical call, decorrelated via a fresh
    // bit-shift axis.
    const int volcanic_hotspot_count = (static_cast<int>((entropy >> 24) % 5ULL) == 0) ? 1 : 0;
    const VolcanicField volcanic_field =
        Volcanism::generate(entropy + 8ULL, volcanic_hotspot_count,
                             0.0, 0.0, params_.planet_size_m, params_.planet_size_m,
                             /*min_peak_elevation_m=*/1500.0, /*max_peak_elevation_m=*/4000.0);
    Volcanism::apply(elev, volcanic_field, sea_m, 0.0, 0.0, params_.planet_size_m,
                      params_.planet_size_m);

    // M125 — trace rivers on this level's own elevation field and carve
    // valleys into it (interior cells only; never touches the edge rows/
    // columns TileEdge samples below, so this can't break parent/child
    // boundary matching). Biome/temperature above were already classified
    // from the pre-carve, pre-uplift elevation — reclassifying near carved
    // valleys or raised peaks beyond M128/M130's passes below is handled
    // there, not here or in M126/M127.
    const HydrologyNetwork hydro_net =
        Hydrology::trace(elev, sea_m, 0.0, 0.0, params_.planet_size_m, params_.planet_size_m);
    Hydrology::carve(elev, hydro_net, sea_m, 0.0, 0.0, params_.planet_size_m, params_.planet_size_m);

    // M128 — reclassify land within the coastal band (adjacent to ocean,
    // not too high above sea level) to beach. Runs last, against the final
    // elevation, since land/ocean membership itself is invariant across
    // uplift/carving (both only ever modify elevation *within* the land or
    // ocean side of sea level, never move a cell across it) -- so this
    // could equally have run right after the classification loop above,
    // but living next to the other post-processing passes keeps the
    // pipeline's refinement stage in one place.
    BiomeRefinement::applyCoastalBeach(bio, elev, sea_m);

    // M130 — demote any swamp cell sitting on locally steep terrain (not a
    // real floodplain) to meadow, against the same final elevation (post-
    // carve/uplift, since either could have changed the local relief
    // around a swamp cell).
    BiomeRefinement::applySwampFlatnessCheck(bio, elev);

    // M259/M274/M275 (2026-07-11) — canyon carving + coastal relief
    // refinement (tidal_flat/sea_cliff), same "runs after the earlier
    // passes" placement as ChildGenerator.cpp's identical call.
    BiomeRefinement::applyCanyonCarving(bio, elev);
    BiomeRefinement::applyCoastalReliefRefinement(bio, elev, sea_m);

    // M247 (2026-07-11) — riparian_forest, same placement as
    // ChildGenerator.cpp's identical call.
    BiomeRefinement::applyRiparianForest(bio, hydro_net, GRID_SIZE, GRID_SIZE, 0.0, 0.0,
                                          params_.planet_size_m, params_.planet_size_m);

    // M265-268 (2026-07-11) — volcanic/geothermal/ash_plain/volcanic_island,
    // same "runs last among the refinement passes" placement as
    // ChildGenerator.cpp's identical call.
    BiomeRefinement::applyVolcanicBiomes(bio, volcanic_field, GRID_SIZE, GRID_SIZE, 0.0, 0.0,
                                          params_.planet_size_m, params_.planet_size_m);

    // M132 — name the traced rivers/lakes/ranges and add them as
    // MapFeature entries; without this, hydro_net/mountain_net are
    // discarded the moment this function returns (they were only ever
    // used above to mutate elev in place).
    FeatureNaming::appendHydrologyFeatures(p.features, hydro_net, p.culture, entropy);
    FeatureNaming::appendMountainRangeFeatures(p.features, mountain_net, elev, sea_m, p.culture, entropy,
                                                0.0, 0.0, params_.planet_size_m, params_.planet_size_m);

    // M342 (MAP22) — same coastline tracing ChildGenerator.cpp's own
    // identical call adds; see that file's comment for the full "exists in
    // name only" backstory. Purely additive.
    for (auto& points : Coastline::trace(elev, sea_m, 0.0, 0.0,
                                          params_.planet_size_m, params_.planet_size_m)) {
        MapFeature f;
        f.type   = FeatureType::Coastline;
        f.points = std::move(points);
        p.features.push_back(std::move(f));
    }

    p.elevation    = std::move(elev);
    p.temperature  = std::move(temp);
    p.moisture     = std::move(moist);
    p.biome        = std::move(bio);

    // M066 — level-0 edge descriptors for child constraint propagation.
    // Edge indices: 0=N (top row), 1=E (right col), 2=S (bottom row), 3=W (left col).
    // Each edge carries GRID_SIZE elevation samples in left-to-right / top-to-bottom order.
    auto& en = p.edges[0]; en.elevation.resize(static_cast<std::size_t>(GRID_SIZE));
    auto& ee = p.edges[1]; ee.elevation.resize(static_cast<std::size_t>(GRID_SIZE));
    auto& es = p.edges[2]; es.elevation.resize(static_cast<std::size_t>(GRID_SIZE));
    auto& ew = p.edges[3]; ew.elevation.resize(static_cast<std::size_t>(GRID_SIZE));
    for (int i = 0; i < GRID_SIZE; ++i) {
        en.elevation[static_cast<std::size_t>(i)] = p.elevation.at(i, 0);               // N: top row
        ee.elevation[static_cast<std::size_t>(i)] = p.elevation.at(GRID_SIZE - 1, i);   // E: right col
        es.elevation[static_cast<std::size_t>(i)] = p.elevation.at(i, GRID_SIZE - 1);   // S: bottom row
        ew.elevation[static_cast<std::size_t>(i)] = p.elevation.at(0, i);               // W: left col
    }

    // M345 (MAP22) — same River EdgeCrossing export ChildGenerator.cpp's
    // own identical call adds; see that file's comment for the full
    // "existed, unit-tested, never wired in" backstory. No Roads::build()
    // at level 0 (no settlements placed here), so no Road crossings to
    // export at this level.
    Hydrology::exportCrossings(hydro_net, GRID_SIZE, GRID_SIZE, 0.0, 0.0,
                                params_.planet_size_m, params_.planet_size_m, p.edges);

    return p;
}

} // namespace MeshWorld::Map
