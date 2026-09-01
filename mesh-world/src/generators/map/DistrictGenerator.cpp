// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "generators/map/DistrictGenerator.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <optional>

#include "MapBuilder.hpp"
#include "Map/Noise.hpp"
#include "Naming.hpp"

namespace MeshWorld::Map {

namespace {

constexpr double PI = 3.14159265358979323846;

constexpr int    kGridSize        = 64;
constexpr int    kMaxSiteAttempts = 8;
// Tile bounds are half-open [min,max) (MapValidator rejects a point at
// exactly max_x/max_z) -- same epsilon country.lua's/district.lua's own
// border corners use.
constexpr double kBorderEpsM = 1e-6;

// Distinct hash2i() axis for this generator's own site-search rolls (see
// CityGenerator.cpp's own "every hash2i(...) caller needs a distinct axis"
// rule) -- picked clear of CityGenerator.cpp's 2001-2006 and
// ChildGenerator.cpp's kRoadNameAxis (1001).
constexpr std::int64_t kSiteAxis     = 3001;
constexpr std::int64_t kDistrictNameAxis = 3002;
constexpr std::int64_t kTownNameAxis     = 3003;

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

struct Quadrant {
    int gx0, gx1, gy0, gy1;
};

// One suitable (non-ocean, non-mountain) random site within the grid-cell
// rectangle [gx0,gx1) x [gy0,gy1), or std::nullopt if none found within
// kMaxSiteAttempts tries.
std::optional<std::array<double, 2>> find_site_in_quadrant(
    std::uint64_t entropy, int quadrant_index, const std::vector<float>& elevation,
    int W, const Quadrant& q, double child_x0, double child_y0, double child_size_m, int H) {
    for (int attempt = 0; attempt < kMaxSiteAttempts; ++attempt) {
        const std::int64_t idx = static_cast<std::int64_t>(quadrant_index) * kMaxSiteAttempts + attempt;
        const int gx = rand_int(entropy, idx * 2, kSiteAxis, q.gx0, q.gx1 - 1);
        const int gy = rand_int(entropy, idx * 2 + 1, kSiteAxis, q.gy0, q.gy1 - 1);
        const float elev_above_sea = elevation[static_cast<std::size_t>(gy) * W + gx];
        if (elev_above_sea > 0.0f && elev_above_sea < 2500.0f) {
            const double wx = child_x0 + (gx + 0.5) * child_size_m / W;
            const double wz = child_y0 + (gy + 0.5) * child_size_m / H;
            return std::array<double, 2>{wx, wz};
        }
    }
    return std::nullopt;
}

} // namespace

DistrictGenerator::DistrictGenerator(PlanetParams params) : params_(std::move(params)) {}

MapTilePayload DistrictGenerator::generate(const TileCoord&      tile,
                                            const MapTilePayload* parent,
                                            std::uint64_t         entropy) const {
    assert(parent != nullptr && "DistrictGenerator requires a parent payload");
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
        }
    }

    const std::string culture = parent->culture.empty() ? MeshWorld::Naming::culture(entropy) : parent->culture;

    MeshWorld::MapBuilder builder(tile, entropy, sea_m, parent);
    builder.setBiomeField(W, H, elevation, temperature, moisture);

    // 4 axis-aligned quadrants: NW, NE, SW, SE (see header note on why not a
    // real geometric partition).
    const int half = W / 2;
    const std::array<Quadrant, 4> quadrants = {{
        {0, half, 0, half},    // NW
        {half, W, 0, half},    // NE
        {0, half, half, H},    // SW
        {half, W, half, H},    // SE
    }};

    for (std::size_t i = 0; i < quadrants.size(); ++i) {
        const auto& q = quadrants[i];
        const auto site = find_site_in_quadrant(entropy, static_cast<int>(i), elevation, W, q,
                                                 child_x0, child_y0, child_size_m, H);
        if (!site) continue;

        const std::uint64_t name_seed = noise::hash2i(static_cast<std::int64_t>(i), kDistrictNameAxis, entropy);
        const std::string   district_name = MeshWorld::Naming::city(culture, name_seed) + " District";

        double wx0 = child_x0 + q.gx0 * child_size_m / W;
        double wx1 = child_x0 + q.gx1 * child_size_m / W;
        double wz0 = child_y0 + q.gy0 * child_size_m / H;
        double wz1 = child_y0 + q.gy1 * child_size_m / H;
        if (q.gx1 == W) wx1 -= kBorderEpsM;
        if (q.gy1 == H) wz1 -= kBorderEpsM;

        builder.addBorder(district_name, {
            {wx0, wz0}, {wx1, wz0}, {wx1, wz1}, {wx0, wz1},
        });

        const std::uint64_t town_seed = noise::hash2i(static_cast<std::int64_t>(i), kTownNameAxis, entropy);
        const std::string   town_name = MeshWorld::Naming::city(culture, town_seed);
        builder.addCity(town_name, (*site)[0], (*site)[1], "town");
    }

    builder.setMetadata("cpp.map.district", culture);

    return builder.payload();
}

} // namespace MeshWorld::Map
