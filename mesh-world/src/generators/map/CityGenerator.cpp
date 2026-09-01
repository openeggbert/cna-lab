// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "generators/map/CityGenerator.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <limits>

#include "MapBuilder.hpp"
#include "Map/Noise.hpp"
#include "Map/ZoneCandidate.hpp"
#include "Naming.hpp"

namespace MeshWorld::Map {

namespace {

constexpr double PI = 3.14159265358979323846;

constexpr int    kGridSize          = 64;
constexpr int    kStreetSpacingCells = 8;   // one grid line every N cells, both axes
constexpr int    kSampleStepCells    = 8;   // waypoint spacing along a bent street line
constexpr int    kMaxDriftCells      = 2;   // max lateral bend from the straight index
constexpr double kEdgeSnapEpsM       = 0.001;
constexpr int    kParksMin           = 0;
constexpr int    kParksMax           = 2;
constexpr int    kParkRadiusCells    = 3;   // Chebyshev radius excluded from urbanization
constexpr int    kMaxSiteAttempts    = 8;
constexpr double kLakeElevMarginM    = 50.0;

// Distinct hash2i() axes for this generator's own entropy-driven rolls (see
// Settlements.cpp/Countries.cpp/FeatureNaming.cpp's own "every hash2i(...)
// caller needs a distinct axis" rule) -- picked well clear of
// ChildGenerator.cpp's kRoadNameAxis (1001) and the 401/601/801/802/900+/
// 1001 axes used elsewhere in Map::.
constexpr std::int64_t kParkCountAxis = 2001;
constexpr std::int64_t kParkSiteAxis  = 2002;
constexpr std::int64_t kZoneRollAxis  = 2003;
constexpr std::int64_t kParkNameAxis  = 2004;
constexpr std::int64_t kStreetNameAxisV = 2005;  // vertical (x=const) street lines
constexpr std::int64_t kStreetNameAxisH = 2006;  // horizontal (y=const) street lines

int rand_int(std::uint64_t entropy, std::int64_t index, std::int64_t axis, int lo, int hi) {
    const std::uint64_t h = noise::hash2i(index, axis, entropy);
    return lo + static_cast<int>(noise::to_unit_float(h) * static_cast<float>(hi - lo + 1));
}

float bilinear(const FieldGrid& g, double fx, double fy) {
    fx = std::max(0.0, std::min(static_cast<double>(g.w - 1), fx));
    fy = std::max(0.0, std::min(static_cast<double>(g.h - 1), fy));
    const int x0 = static_cast<int>(fx);
    const int y0 = static_cast<int>(fy);
    const int x1 = std::min(x0 + 1, g.w - 1);
    const int y1 = std::min(y0 + 1, g.h - 1);
    const double tx = fx - x0, ty = fy - y0;
    const double v00 = g.at(x0, y0), v10 = g.at(x1, y0);
    const double v01 = g.at(x0, y1), v11 = g.at(x1, y1);
    return static_cast<float>(v00 + (v10 - v00) * tx + (v01 - v00) * ty
                              + (v11 - v10 - v01 + v00) * tx * ty);
}

// Native port of city.lua's align_grid_line(): bends a straight axis-aligned
// grid line to loosely follow terrain (a street prefers a constant-elevation
// contour over cutting straight through a slope). vertical=true bends an
// x=const line (cross axis is gx) as gy sweeps 0..H-1; vertical=false bends a
// y=const line (cross axis is gy) as gx sweeps 0..W-1. The two tile-boundary
// samples are pinned to the nominal, unbent fixed_index (not wherever the
// walk drifted to) so a neighboring sibling tile's own matching street --
// same fixed_index, since kStreetSpacingCells positions land on identical
// local indices across every level-12 tile -- meets it exactly at the shared
// boundary, mirroring city.lua's own cross-tile seam fix.
std::vector<std::array<double, 2>> align_grid_line(const std::vector<float>& elevation, int W, int H,
                                                     bool vertical, int fixed_index,
                                                     double child_x0, double child_y0,
                                                     double child_size_m) {
    const int along_len = vertical ? H : W;
    const int cross_len = vertical ? W : H;

    const auto elev_at = [&](int along, int cross) -> float {
        const int gx = vertical ? cross : along;
        const int gy = vertical ? along : cross;
        return elevation[static_cast<std::size_t>(gy) * W + gx];
    };
    const auto world_point = [&](int along, int cross, const double* along_boundary) -> std::array<double, 2> {
        const int gx = vertical ? cross : along;
        const int gy = vertical ? along : cross;
        double px = child_x0 + (gx + 0.5) * child_size_m / W;
        double pz = child_y0 + (gy + 0.5) * child_size_m / H;
        if (along_boundary != nullptr) {
            if (vertical) pz = *along_boundary; else px = *along_boundary;
        }
        return {px, pz};
    };

    const double along_min = vertical ? child_y0 : child_x0;
    const double along_max = along_min + child_size_m - kEdgeSnapEpsM;

    const int lo = std::max(0, fixed_index - kMaxDriftCells);
    const int hi = std::min(cross_len - 1, fixed_index + kMaxDriftCells);

    std::vector<std::array<double, 2>> points;
    int    prev_cross = fixed_index;
    float  prev_elev  = elev_at(0, fixed_index);
    int    last_along = 0;

    for (int along = 0; along < along_len; along += kSampleStepCells) {
        int   best_cross = prev_cross;
        float best_diff  = std::numeric_limits<float>::max();
        for (int d = -1; d <= 1; ++d) {
            const int cand = prev_cross + d;
            if (cand >= lo && cand <= hi) {
                const float diff = std::abs(elev_at(along, cand) - prev_elev);
                if (diff < best_diff) { best_diff = diff; best_cross = cand; }
            }
        }
        prev_cross = best_cross;
        prev_elev  = elev_at(along, best_cross);
        last_along = along;

        const double* boundary = nullptr;
        double boundary_value = 0.0;
        int emit_cross = best_cross;
        if (along == 0) {
            boundary_value = along_min;
            boundary = &boundary_value;
            emit_cross = fixed_index;
        }
        if (along == along_len - 1) {
            boundary_value = along_max;
            boundary = &boundary_value;
            emit_cross = fixed_index;
        }
        points.push_back(world_point(along, emit_cross, boundary));
    }
    if (last_along != along_len - 1) {
        const double boundary_value = along_max;
        points.push_back(world_point(along_len - 1, fixed_index, &boundary_value));
    }
    return points;
}

} // namespace

CityGenerator::CityGenerator(PlanetParams params) : params_(std::move(params)) {}

MapTilePayload CityGenerator::generate(const TileCoord&      tile,
                                        const MapTilePayload* parent,
                                        std::uint64_t         entropy) const {
    assert(parent != nullptr && "CityGenerator requires a parent payload");
    assert(!parent->elevation.empty() && "parent elevation grid must be populated");

    constexpr int W = kGridSize, H = kGridSize;
    const int cx = static_cast<int>(tile.x % 2LL);
    const int cy = static_cast<int>(tile.y % 2LL);

    const double child_size_m = params_.planet_size_m / std::pow(2.0, static_cast<double>(tile.level));
    const double child_x0     = static_cast<double>(tile.x) * child_size_m;
    const double child_y0     = static_cast<double>(tile.y) * child_size_m;
    const double sea_m        = params_.sea_level_m;

    const double terrain_scale = std::max(child_size_m / 4.0, 1.0);
    const double detail_amp    = std::min(child_size_m / 100.0, 500.0);

    std::vector<float> elevation(static_cast<std::size_t>(W * H));
    std::vector<float> temperature(static_cast<std::size_t>(W * H));
    std::vector<float> moisture(static_cast<std::size_t>(W * H));
    std::vector<std::uint8_t> is_buildable(static_cast<std::size_t>(W * H), 0);
    std::vector<std::array<double, 2>> lake_points;
    bool any_buildable = false;

    for (int gy = 0; gy < H; ++gy) {
        for (int gx = 0; gx < W; ++gx) {
            const std::size_t idx = static_cast<std::size_t>(gy) * W + gx;

            const double pgx_f = cx * 32.0 + gx * (32.0 / (W - 1));
            const double pgy_f = cy * 32.0 + gy * (32.0 / (H - 1));
            const float  base_elev = bilinear(parent->elevation, pgx_f, pgy_f);

            const double fx   = std::sin(PI * gx / (W - 1));
            const double fy   = std::sin(PI * gy / (H - 1));
            const double fade = fx * fx * fy * fy;

            const double wx = child_x0 + (gx + 0.5) * child_size_m / W;
            const double wy = child_y0 + (gy + 0.5) * child_size_m / H;

            const float  detail_fbm = noise::fbm(wx / terrain_scale, wy / terrain_scale, entropy + 2ULL);
            const double elev = base_elev + fade * (detail_fbm - 0.5) * detail_amp;
            elevation[idx] = static_cast<float>(elev);

            const double lat_factor = std::abs(wy / params_.planet_size_m - 0.5) * 2.0;
            const double base_temp  = params_.equator_temp_c
                                     + (params_.pole_temp_c - params_.equator_temp_c) * lat_factor;
            const double elev_above = std::max(elev - sea_m, 0.0);
            temperature[idx] = static_cast<float>(base_temp - 6.5 * elev_above / 1000.0);

            moisture[idx] = std::clamp(noise::fbm(wx / terrain_scale, wy / terrain_scale, entropy + 3ULL),
                                        0.0f, 1.0f);

            const double elev_above_sea = elev - sea_m;
            const bool   buildable = elev_above_sea > 0.0 && elev_above_sea < 2500.0;
            is_buildable[idx] = buildable ? 1 : 0;
            if (buildable) any_buildable = true;

            if (elev_above_sea >= 0.0 && elev_above_sea <= kLakeElevMarginM)
                lake_points.push_back({wx, wy});
        }
    }

    const std::string culture = parent->culture.empty() ? MeshWorld::Naming::culture(entropy) : parent->culture;

    MeshWorld::MapBuilder builder(tile, entropy, sea_m, parent);
    builder.setBiomeField(W, H, elevation, temperature, moisture);

    // MAP19/M259/M274/M275-equivalent refinement, BEFORE zoning below so
    // urbanization always wins over natural refinement (mirrors city.lua's
    // own precedence).
    builder.applyCoastalBeach();
    builder.applySwampFlatnessCheck();
    builder.applyCanyonCarving();
    builder.applyCoastalReliefRefinement();

    if (any_buildable) {
        struct ParkSite { int gx, gy; };
        std::vector<ParkSite> park_sites;
        const int n_parks = rand_int(entropy, 0, kParkCountAxis, kParksMin, kParksMax);
        for (int i = 0; i < n_parks; ++i) {
            for (int attempt = 0; attempt < kMaxSiteAttempts; ++attempt) {
                const std::int64_t site_index = static_cast<std::int64_t>(i) * kMaxSiteAttempts + attempt;
                const int gx = rand_int(entropy, site_index * 2, kParkSiteAxis, 0, W - 1);
                const int gy = rand_int(entropy, site_index * 2 + 1, kParkSiteAxis, 0, H - 1);
                if (is_buildable[static_cast<std::size_t>(gy) * W + gx]) {
                    park_sites.push_back({gx, gy});
                    break;
                }
            }
        }

        std::vector<std::uint8_t> urban_mask(static_cast<std::size_t>(W * H), 0);
        for (int gy = 0; gy < H; ++gy) {
            for (int gx = 0; gx < W; ++gx) {
                const std::size_t idx = static_cast<std::size_t>(gy) * W + gx;
                bool urban = is_buildable[idx] != 0;
                if (urban) {
                    for (const auto& p : park_sites) {
                        if (std::abs(gx - p.gx) <= kParkRadiusCells && std::abs(gy - p.gy) <= kParkRadiusCells) {
                            urban = false;
                            break;
                        }
                    }
                }
                urban_mask[idx] = urban ? 1 : 0;
            }
        }
        builder.markUrbanCells(urban_mask);

        for (std::size_t i = 0; i < park_sites.size(); ++i) {
            const auto& p = park_sites[i];
            const double wx = child_x0 + (p.gx + 0.5) * child_size_m / W;
            const double wz = child_y0 + (p.gy + 0.5) * child_size_m / H;
            const std::uint64_t seed = noise::hash2i(static_cast<std::int64_t>(i), kParkNameAxis, entropy);
            const std::string   park_name = MeshWorld::Naming::city(culture, seed) + " Park";
            builder.addPark(park_name, wx, wz);
        }

        for (int gx = 0; gx < W; gx += kStreetSpacingCells) {
            const std::uint64_t seed = noise::hash2i(gx, kStreetNameAxisV, entropy);
            const std::string   street_name = MeshWorld::Naming::street(culture, seed);
            auto path = align_grid_line(elevation, W, H, /*vertical=*/true, gx, child_x0, child_y0, child_size_m);
            builder.addStreet(street_name, path);
        }
        for (int gy = 0; gy < H; gy += kStreetSpacingCells) {
            const std::uint64_t seed = noise::hash2i(gy, kStreetNameAxisH, entropy);
            const std::string   street_name = MeshWorld::Naming::street(culture, seed);
            auto path = align_grid_line(elevation, W, H, /*vertical=*/false, gy, child_x0, child_y0, child_size_m);
            builder.addStreet(street_name, path);
        }

        // Zone candidates (Map::ZoneCandidate): one per BLOCK, where a block
        // is purely an index-space rectangle between nominal (pre-bend)
        // street grid lines -- deliberately decoupled from align_grid_line()'s
        // actual bent polylines, same as city.lua's own M156 logic.
        std::vector<std::uint8_t> zone_candidates(static_cast<std::size_t>(W * H),
                                                    static_cast<std::uint8_t>(ZoneCandidate::none));
        const int blocks_y = (H + kStreetSpacingCells - 1) / kStreetSpacingCells;
        const int blocks_x = (W + kStreetSpacingCells - 1) / kStreetSpacingCells;
        for (int by = 0; by < blocks_y; ++by) {
            const int gy0 = by * kStreetSpacingCells;
            const int gy1 = std::min(gy0 + kStreetSpacingCells, H) - 1;
            for (int bx = 0; bx < blocks_x; ++bx) {
                const int gx0 = bx * kStreetSpacingCells;
                const int gx1 = std::min(gx0 + kStreetSpacingCells, W) - 1;

                bool block_urban = false;
                for (int gy = gy0; gy <= gy1; ++gy)
                    for (int gx = gx0; gx <= gx1; ++gx)
                        if (urban_mask[static_cast<std::size_t>(gy) * W + gx] == 1) block_urban = true;

                bool block_park = false;
                for (const auto& p : park_sites)
                    if (p.gx >= gx0 && p.gx <= gx1 && p.gy >= gy0 && p.gy <= gy1) block_park = true;

                ZoneCandidate candidate;
                if (block_park) {
                    candidate = ZoneCandidate::park;
                } else if (!block_urban) {
                    candidate = ZoneCandidate::none;
                } else {
                    const std::int64_t block_index = static_cast<std::int64_t>(by) * blocks_x + bx;
                    const int roll = rand_int(entropy, block_index, kZoneRollAxis, 0, 99);
                    // Weighted roll, most common to rarest (mirrors city.lua's own thresholds).
                    if (roll < 5)       candidate = ZoneCandidate::square;
                    else if (roll < 15) candidate = ZoneCandidate::shop_street;
                    else if (roll < 30) candidate = ZoneCandidate::apartment_block;
                    else                candidate = ZoneCandidate::small_house_block;
                }

                for (int gy = gy0; gy <= gy1; ++gy)
                    for (int gx = gx0; gx <= gx1; ++gx)
                        zone_candidates[static_cast<std::size_t>(gy) * W + gx] = static_cast<std::uint8_t>(candidate);
            }
        }
        builder.setZoneCandidates(zone_candidates);
    }

    if (!lake_points.empty()) {
        const std::string lake_name = MeshWorld::Naming::lake(culture, entropy);
        builder.addLake(lake_name, lake_points);
    }

    builder.setMetadata("cpp.map.city", culture);

    return builder.payload();
}

} // namespace MeshWorld::Map
