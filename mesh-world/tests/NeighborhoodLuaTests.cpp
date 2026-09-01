// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M154 — structural tests for generators/lua/map/neighborhood.lua (level
// 15), mirroring CityLuaTests.cpp's style (child-tile path, parent
// required). Unlike city.lua, this script deliberately has no zoning
// (markUrbanCells)/park/lake output -- see neighborhood.lua's own header --
// so there is no equivalent of CityLuaTest's urban-cell/park/lake tests.

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>

#include "LuaSandbox.hpp"
#include "Map/BiomeClassifier.hpp"
#include "Map/MapTilePayload.hpp"
#include "ZoneType.hpp"

using namespace MeshWorld;
using namespace MeshWorld::Map;

namespace {

constexpr int N = 64;

std::string read_neighborhood_lua() {
    std::ifstream ifs("generators/lua/map/neighborhood.lua");
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

// Uniform 1000 m everywhere: comfortably land (>0) and comfortably below the
// 2500 m mountain threshold, even after the child's own detail perturbation
// (detail_amp tops out well below 1000 m at this tile size) regardless of
// entropy — every cell is buildable.
MapTilePayload make_moderate_land_parent(std::uint64_t entropy) {
    MapTilePayload p;
    p.tile    = TileCoord{14, 0, 0};
    p.entropy = entropy;
    p.culture = "nordic";

    p.elevation.w = p.elevation.h = N;
    p.elevation.data.assign(static_cast<std::size_t>(N * N), 1000.0f);
    p.temperature.w = p.temperature.h = N;
    p.temperature.data.assign(static_cast<std::size_t>(N * N), 15.0f);
    p.moisture.w = p.moisture.h = N;
    p.moisture.data.assign(static_cast<std::size_t>(N * N), 0.4f);
    p.biome.w = p.biome.h = N;
    // M354 fix (2026-07-10): a real classify() result (meadow, at these
    // elevation/temperature/moisture values), not a bare 0 -- ZoneType::city
    // IS ordinal 0, so an all-zero biome grid used to silently mean "the
    // whole parent is already a city" once neighborhood.lua started reading
    // parent.biome, which would have made this fixture accidentally trigger
    // M354's own new inheritance behavior in every other test below.
    p.biome.data.assign(static_cast<std::size_t>(N * N),
                         static_cast<std::uint8_t>(BiomeClassifier::classify(1000.0, 15.0, 0.4, 0.0)));
    for (auto& e : p.edges) e.elevation.assign(static_cast<std::size_t>(N), 1000.0f);
    return p;
}

// Uniform -2000 m everywhere: comfortably below sea level even after detail
// perturbation, regardless of entropy — nothing is ever buildable.
MapTilePayload make_all_ocean_parent(std::uint64_t entropy) {
    MapTilePayload p;
    p.tile    = TileCoord{14, 0, 0};
    p.entropy = entropy;
    p.culture = "nordic";

    p.elevation.w = p.elevation.h = N;
    p.elevation.data.assign(static_cast<std::size_t>(N * N), -2000.0f);
    p.temperature.w = p.temperature.h = N;
    p.temperature.data.assign(static_cast<std::size_t>(N * N), 15.0f);
    p.moisture.w = p.moisture.h = N;
    p.moisture.data.assign(static_cast<std::size_t>(N * N), 0.4f);
    p.biome.w = p.biome.h = N;
    // M354 fix: real classify() result (ocean), see make_moderate_land_parent()'s
    // own comment above for why this can no longer be a bare 0.
    p.biome.data.assign(static_cast<std::size_t>(N * N),
                         static_cast<std::uint8_t>(BiomeClassifier::classify(-2000.0, 15.0, 0.4, 0.0)));
    for (auto& e : p.edges) e.elevation.assign(static_cast<std::size_t>(N), -2000.0f);
    return p;
}

// A diagonal elevation gradient (elev = BASE + (px - py) * K), mirroring
// CityLuaTests.cpp's own fixture of the same name/purpose: proves M155's
// align_grid_line() (duplicated from city.lua) actually bends this
// script's own finer street grid too, not just city.lua's.
MapTilePayload make_diagonal_gradient_parent(std::uint64_t entropy) {
    constexpr double BASE = 500.0;
    constexpr double K    = 100.0;
    MapTilePayload p;
    p.tile    = TileCoord{14, 0, 0};
    p.entropy = entropy;
    p.culture = "nordic";

    p.elevation.w = p.elevation.h = N;
    p.elevation.data.resize(static_cast<std::size_t>(N * N));
    for (int py = 0; py < N; ++py) {
        for (int px = 0; px < N; ++px) {
            p.elevation.data[static_cast<std::size_t>(py * N + px)] =
                static_cast<float>(BASE + (px - py) * K);
        }
    }
    p.temperature.w = p.temperature.h = N;
    p.temperature.data.assign(static_cast<std::size_t>(N * N), 15.0f);
    p.moisture.w = p.moisture.h = N;
    p.moisture.data.assign(static_cast<std::size_t>(N * N), 0.4f);
    p.biome.w = p.biome.h = N;
    p.biome.data.resize(static_cast<std::size_t>(N * N));
    // M354 fix: real per-cell classify() result matching this fixture's own
    // elevation gradient (see make_moderate_land_parent()'s comment above
    // for why this can no longer be a bare 0 -- this fixture's gradient
    // spans ocean up to near-mountain elevations, so a single constant
    // wouldn't even be a plausible stand-in the way it was for the other
    // two uniform-elevation fixtures).
    for (std::size_t i = 0; i < p.biome.data.size(); ++i) {
        p.biome.data[i] = static_cast<std::uint8_t>(
            BiomeClassifier::classify(p.elevation.data[i], 15.0, 0.4, 0.0));
    }

    for (auto& e : p.edges) e.elevation.resize(static_cast<std::size_t>(N));
    for (int k = 0; k < N; ++k) {
        p.edges[0].elevation[static_cast<std::size_t>(k)] = static_cast<float>(BASE + (k - 0) * K);
        p.edges[2].elevation[static_cast<std::size_t>(k)] = static_cast<float>(BASE + (k - (N - 1)) * K);
        p.edges[1].elevation[static_cast<std::size_t>(k)] = static_cast<float>(BASE + ((N - 1) - k) * K);
        p.edges[3].elevation[static_cast<std::size_t>(k)] = static_cast<float>(BASE + (0 - k) * K);
    }
    return p;
}

MapGenContext make_ctx(const MapTilePayload& parent, std::uint64_t entropy, const TileCoord& tile) {
    MapGenContext ctx;
    ctx.tile        = tile;
    ctx.entropy     = entropy;
    ctx.sea_level_m = 0.0;
    ctx.parent      = &parent;
    return ctx;
}

MapTilePayload run_neighborhood(const MapTilePayload& parent, std::uint64_t entropy,
                                 const TileCoord& tile, std::string* error_out = nullptr) {
    LuaSandbox sandbox;
    return sandbox.executeMap(read_neighborhood_lua(), make_ctx(parent, entropy, tile), error_out);
}

bool has_feature(const MapTilePayload& payload, FeatureType type) {
    for (const auto& f : payload.features)
        if (f.type == type) return true;
    return false;
}

int count_feature(const MapTilePayload& payload, FeatureType type) {
    int n = 0;
    for (const auto& f : payload.features)
        if (f.type == type) ++n;
    return n;
}

} // namespace

TEST(NeighborhoodLuaTest, FileReadsAndRunsWithoutError) {
    const std::string source = read_neighborhood_lua();
    ASSERT_FALSE(source.empty()) << "generators/lua/map/neighborhood.lua not found or empty";

    const MapTilePayload parent = make_moderate_land_parent(1);
    std::string error;
    const MapTilePayload payload = run_neighborhood(parent, 2, TileCoord{15, 1, 0}, &error);

    EXPECT_TRUE(error.empty()) << error;
    EXPECT_FALSE(payload.elevation.empty());
    EXPECT_EQ(payload.generator, "lua.map.child.level15.default");
    EXPECT_EQ(payload.culture, "nordic");
}

TEST(NeighborhoodLuaTest, FieldShapesMatch64x64) {
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_neighborhood(parent, 2, TileCoord{15, 1, 0});

    EXPECT_EQ(payload.elevation.w, N);
    EXPECT_EQ(payload.elevation.h, N);
    EXPECT_EQ(payload.temperature.w, N);
    EXPECT_EQ(payload.temperature.h, N);
    EXPECT_EQ(payload.moisture.w, N);
    EXPECT_EQ(payload.moisture.h, N);
    EXPECT_EQ(payload.biome.w, N);
    EXPECT_EQ(payload.biome.h, N);
}

TEST(NeighborhoodLuaTest, EdgeDescriptorsMatchElevationBoundary) {
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_neighborhood(parent, 2, TileCoord{15, 1, 0});

    ASSERT_EQ(static_cast<int>(payload.edges[0].elevation.size()), N);
    ASSERT_EQ(static_cast<int>(payload.edges[1].elevation.size()), N);
    ASSERT_EQ(static_cast<int>(payload.edges[2].elevation.size()), N);
    ASSERT_EQ(static_cast<int>(payload.edges[3].elevation.size()), N);

    for (int i = 0; i < N; ++i) {
        EXPECT_EQ(payload.edges[0].elevation[static_cast<std::size_t>(i)],
                  payload.elevation.at(i, 0))     << "N edge mismatch at i=" << i;
        EXPECT_EQ(payload.edges[2].elevation[static_cast<std::size_t>(i)],
                  payload.elevation.at(i, N - 1)) << "S edge mismatch at i=" << i;
        EXPECT_EQ(payload.edges[1].elevation[static_cast<std::size_t>(i)],
                  payload.elevation.at(N - 1, i)) << "E edge mismatch at i=" << i;
        EXPECT_EQ(payload.edges[3].elevation[static_cast<std::size_t>(i)],
                  payload.elevation.at(0, i))     << "W edge mismatch at i=" << i;
    }
}

// M155: each street line is bent by align_grid_line() into a fixed number
// of waypoints (SAMPLE_STEP_CELLS=4 over a 64-cell tile: 16 sampled steps +
// 1 trailing endpoint = 17), regardless of how much it actually bends.
constexpr std::size_t kNeighborhoodStreetWaypoints = 17u;

TEST(NeighborhoodLuaTest, PlacesAFinerStreetGridOverModerateLand) {
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_neighborhood(parent, 2, TileCoord{15, 1, 0});

    int street_count = 0;
    for (const auto& f : payload.features) {
        if (f.type == FeatureType::Street) {
            ++street_count;
            EXPECT_FALSE(f.name.empty());
            ASSERT_EQ(f.points.size(), kNeighborhoodStreetWaypoints);
        }
    }
    EXPECT_GT(street_count, 0);
}

// §5 #18 fix (M162 prerequisite): same guarantee as CityLuaTest's own version -- the
// first/last waypoint must land exactly on the tile's true boundary (half-open
// [min,max), so the max side is snapped just under the edge), not half a cell short.
TEST(NeighborhoodLuaTest, StreetEndpointsSnapToTileBoundary) {
    const TileCoord tile{15, 1, 0};
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_neighborhood(parent, 2, tile);
    const WorldBounds bounds = tile.world_bounds();
    constexpr double kEps = 0.01;

    int checked = 0;
    for (const auto& f : payload.features) {
        if (f.type != FeatureType::Street) continue;
        ASSERT_GE(f.points.size(), 2u);
        const auto& first = f.points.front();
        const auto& last  = f.points.back();

        const bool vertical = std::abs(first[1] - last[1]) > std::abs(first[0] - last[0]);
        if (vertical) {
            EXPECT_NEAR(first[1], bounds.min_z, kEps) << f.name;
            EXPECT_NEAR(last[1], bounds.max_z, kEps) << f.name;
            EXPECT_LT(last[1], bounds.max_z) << f.name << ": must stay inside the half-open bound";
        } else {
            EXPECT_NEAR(first[0], bounds.min_x, kEps) << f.name;
            EXPECT_NEAR(last[0], bounds.max_x, kEps) << f.name;
            EXPECT_LT(last[0], bounds.max_x) << f.name << ": must stay inside the half-open bound";
        }
        ++checked;
    }
    EXPECT_GT(checked, 0);
}

TEST(NeighborhoodLuaTest, StreetsBendToFollowATerrainGradient) {
    const MapTilePayload parent  = make_diagonal_gradient_parent(1);
    const MapTilePayload payload = run_neighborhood(parent, 2, TileCoord{15, 0, 0});

    // neighborhood.lua's own loop order (gx-loop before gy-loop) means the
    // first Street feature is always the vertical line at gx=0.
    const MapFeature* first_street = nullptr;
    for (const auto& f : payload.features) {
        if (f.type == FeatureType::Street) { first_street = &f; break; }
    }
    ASSERT_NE(first_street, nullptr);
    ASSERT_EQ(first_street->points.size(), kNeighborhoodStreetWaypoints);

    // Same proof as CityLuaTest's own version: against a strong diagonal
    // gradient, align_grid_line()'s greedy contour-following walk must
    // drift this vertical street's x away from its straight-line x as gy
    // grows.
    const double x_first = first_street->points.front()[0];
    const double x_last  = first_street->points.back()[0];
    EXPECT_GT(x_last, x_first);
}

TEST(NeighborhoodLuaTest, StreetGridIsFinerThanCitys) {
    // city.lua's grid line count over the same 64-cell tile (spacing 8);
    // neighborhood.lua's tighter spacing (4) must produce strictly more
    // street segments given the same all-buildable fixture.
    constexpr int city_spacing = 8;
    constexpr int neighborhood_spacing = 4;
    const int city_lines = 2 * ((N - 1) / city_spacing + 1);
    const int neighborhood_lines = 2 * ((N - 1) / neighborhood_spacing + 1);
    EXPECT_GT(neighborhood_lines, city_lines);

    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_neighborhood(parent, 2, TileCoord{15, 1, 0});
    EXPECT_EQ(count_feature(payload, FeatureType::Street), neighborhood_lines);
}

TEST(NeighborhoodLuaTest, NeverPlacesStreetsOverOcean) {
    const MapTilePayload parent = make_all_ocean_parent(1);
    for (std::uint64_t entropy = 1; entropy <= 10; ++entropy) {
        const MapTilePayload payload = run_neighborhood(parent, entropy, TileCoord{15, 0, 0});
        EXPECT_FALSE(has_feature(payload, FeatureType::Street)) << "entropy=" << entropy;
    }
}

TEST(NeighborhoodLuaTest, NeverZonesOrAddsParksOrLakesOverANonUrbanParent) {
    // M154's scope is streets only -- zoning (Map::ZoneCandidate) is M156's
    // job, parks/water are city.lua's (M153) job, not repeated at this
    // level. make_moderate_land_parent() is genuinely non-city (meadow, via
    // BiomeClassifier::classify() -- see its own comment), so M354's new
    // city-inheritance path (tested separately below) must not fire here.
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_neighborhood(parent, 2, TileCoord{15, 1, 0});

    for (const auto& b : payload.biome.data)
        EXPECT_NE(static_cast<ZoneType>(b), ZoneType::city);
    EXPECT_FALSE(has_feature(payload, FeatureType::Park));
    EXPECT_FALSE(has_feature(payload, FeatureType::Lake));
}

// M354 (MAP24, found 2026-07-10): a parent tile whose own biome grid is
// already ZoneType::city over its western half (e.g. written by city.lua's
// markUrbanCells() 3 levels up) must have that override survive down to
// this level -- previously neighborhood.lua had no way to see parent.biome
// at all, so this always silently reverted to a natural classify() result.
MapTilePayload make_half_urban_parent(std::uint64_t entropy) {
    MapTilePayload p = make_moderate_land_parent(entropy);
    // Parent grid columns 0..31 (of 64) -> ZoneType::city; columns 32..63
    // stay whatever make_moderate_land_parent() already set (meadow).
    for (int py = 0; py < N; ++py) {
        for (int px = 0; px < N / 2; ++px) {
            p.biome.data[static_cast<std::size_t>(py * N + px)] =
                static_cast<std::uint8_t>(ZoneType::city);
        }
    }
    return p;
}

// neighborhood.lua's own pgx formula (cx * 32.0 + gx * (32.0 / (W - 1))):
// with tile_x=0 (cx=0), gx=0 lands exactly on parent column 0 -- deep
// inside make_half_urban_parent()'s city half, no rounding ambiguity.
// (gx=W-1 lands exactly on parent column 32, the shared boundary between
// the city and meadow halves -- deliberately not asserted on here, same
// "boundary rounding is a wash" reasoning M112/M108 boundary-matching code
// elsewhere in this codebase already accepts.)
TEST(NeighborhoodLuaTest, InheritsCityFromParentBiomeInteriorOfUrbanQuadrant) {
    const MapTilePayload parent  = make_half_urban_parent(1);
    const MapTilePayload payload = run_neighborhood(parent, 2, TileCoord{15, 0, 0});

    constexpr int gx = 0, gy = 0;
    const auto zone = static_cast<ZoneType>(payload.biome.data[static_cast<std::size_t>(gy * N + gx)]);
    EXPECT_EQ(zone, ZoneType::city);
}

// tile_x=1 (cx=1), gx=W-1: pgx clamps to parent column 63 -- deep inside
// the meadow half, no rounding ambiguity.
TEST(NeighborhoodLuaTest, DoesNotInheritCityFromParentBiomeInteriorOfNonUrbanQuadrant) {
    const MapTilePayload parent  = make_half_urban_parent(1);
    const MapTilePayload payload = run_neighborhood(parent, 2, TileCoord{15, 1, 0});

    constexpr int gx = N - 1, gy = 0;
    const auto zone = static_cast<ZoneType>(payload.biome.data[static_cast<std::size_t>(gy * N + gx)]);
    EXPECT_NE(zone, ZoneType::city);
}

TEST(NeighborhoodLuaTest, InheritingCityDoesNotSuppressThisLevelsOwnStreetGrid) {
    // Inheriting city must not suppress this level's own scope (streets) --
    // buildability is still purely elevation-driven (is_buildable), city
    // inheritance and street placement are independent concerns here, same
    // as city.lua's own header note documents for markUrbanCells() vs. its
    // street grid.
    const MapTilePayload parent  = make_half_urban_parent(1);
    const MapTilePayload payload = run_neighborhood(parent, 2, TileCoord{15, 0, 0});

    EXPECT_TRUE(has_feature(payload, FeatureType::Street));
}

TEST(NeighborhoodLuaTest, DeterministicForSameEntropy) {
    const MapTilePayload parent = make_moderate_land_parent(1);
    const MapTilePayload a = run_neighborhood(parent, 999, TileCoord{15, 1, 0});
    const MapTilePayload b = run_neighborhood(parent, 999, TileCoord{15, 1, 0});

    ASSERT_EQ(a.elevation.data.size(), b.elevation.data.size());
    EXPECT_EQ(a.elevation.data, b.elevation.data);
    EXPECT_EQ(a.temperature.data, b.temperature.data);
    EXPECT_EQ(a.moisture.data, b.moisture.data);
    EXPECT_EQ(a.biome.data, b.biome.data);
    ASSERT_EQ(a.features.size(), b.features.size());
    for (std::size_t i = 0; i < a.features.size(); ++i) {
        EXPECT_EQ(a.features[i].name, b.features[i].name);
        EXPECT_EQ(a.features[i].type, b.features[i].type);
    }
}

TEST(NeighborhoodLuaTest, MissingParentReturnsEmptyPayloadNotCrash) {
    MapGenContext ctx;
    ctx.tile        = TileCoord{15, 0, 0};
    ctx.entropy     = 1;
    ctx.sea_level_m = 0.0;
    ctx.parent      = nullptr;

    LuaSandbox sandbox;
    std::string error;
    const MapTilePayload payload = sandbox.executeMap(read_neighborhood_lua(), ctx, &error);

    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(payload.elevation.empty());
}
