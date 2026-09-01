// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M153 — structural tests for generators/lua/map/city.lua (level 12),
// mirroring MetroLuaTests.cpp's/RegionLuaTests.cpp's style (child-tile
// path, parent required).

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>

#include "LuaSandbox.hpp"
#include "Map/MapTilePayload.hpp"
#include "Map/ZoneCandidate.hpp"
#include "ZoneType.hpp"

using namespace MeshWorld;
using namespace MeshWorld::Map;

namespace {

constexpr int N = 64;

std::string read_city_lua() {
    std::ifstream ifs("generators/lua/map/city.lua");
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

// Uniform 1000 m everywhere: comfortably land (>0) and comfortably below the
// 2500 m mountain threshold, AND comfortably outside the 50 m lake band,
// even after the child's own detail perturbation (level 12's detail_amp
// tops out well below 1000 m at this tile size) regardless of entropy —
// every site-find attempt succeeds, no lake is ever detected.
MapTilePayload make_moderate_land_parent(std::uint64_t entropy) {
    MapTilePayload p;
    p.tile    = TileCoord{11, 0, 0};
    p.entropy = entropy;
    p.culture = "nordic";

    p.elevation.w = p.elevation.h = N;
    p.elevation.data.assign(static_cast<std::size_t>(N * N), 1000.0f);
    p.temperature.w = p.temperature.h = N;
    p.temperature.data.assign(static_cast<std::size_t>(N * N), 15.0f);
    p.moisture.w = p.moisture.h = N;
    p.moisture.data.assign(static_cast<std::size_t>(N * N), 0.4f);
    p.biome.w = p.biome.h = N;
    p.biome.data.assign(static_cast<std::size_t>(N * N), 0);
    for (auto& e : p.edges) e.elevation.assign(static_cast<std::size_t>(N), 1000.0f);
    return p;
}

// Uniform -2000 m everywhere: comfortably below sea level even after detail
// perturbation, regardless of entropy — no site-find attempt can succeed,
// nothing is ever buildable.
MapTilePayload make_all_ocean_parent(std::uint64_t entropy) {
    MapTilePayload p;
    p.tile    = TileCoord{11, 0, 0};
    p.entropy = entropy;
    p.culture = "nordic";

    p.elevation.w = p.elevation.h = N;
    p.elevation.data.assign(static_cast<std::size_t>(N * N), -2000.0f);
    p.temperature.w = p.temperature.h = N;
    p.temperature.data.assign(static_cast<std::size_t>(N * N), 15.0f);
    p.moisture.w = p.moisture.h = N;
    p.moisture.data.assign(static_cast<std::size_t>(N * N), 0.4f);
    p.biome.w = p.biome.h = N;
    p.biome.data.assign(static_cast<std::size_t>(N * N), 0);
    for (auto& e : p.edges) e.elevation.assign(static_cast<std::size_t>(N), -2000.0f);
    return p;
}

// Uniform 25 m everywhere: comfortably inside the [0, 50] m lake band near
// every tile edge (where the fade term is ~0) — reliably produces lake
// points, same technique MetroLuaTests.cpp's own fixture already uses.
MapTilePayload make_near_sea_level_parent(std::uint64_t entropy) {
    MapTilePayload p;
    p.tile    = TileCoord{11, 0, 0};
    p.entropy = entropy;
    p.culture = "nordic";

    p.elevation.w = p.elevation.h = N;
    p.elevation.data.assign(static_cast<std::size_t>(N * N), 25.0f);
    p.temperature.w = p.temperature.h = N;
    p.temperature.data.assign(static_cast<std::size_t>(N * N), 15.0f);
    p.moisture.w = p.moisture.h = N;
    p.moisture.data.assign(static_cast<std::size_t>(N * N), 0.4f);
    p.biome.w = p.biome.h = N;
    p.biome.data.assign(static_cast<std::size_t>(N * N), 0);
    for (auto& e : p.edges) e.elevation.assign(static_cast<std::size_t>(N), 25.0f);
    return p;
}

// A diagonal elevation gradient (elev = BASE + (px - py) * K): to keep a
// vertical street's own elevation roughly constant as gy grows, M155's
// align_grid_line() must drift the street toward increasing gx (px) to
// offset gy's growth -- a genuine, predictable "align to terrain" case,
// unlike the uniform/near-uniform fixtures above (no real slope to align
// to). p.edges are filled with the same formula (not just a uniform
// value) so M108's boundary overwrite stays gradient-consistent too.
MapTilePayload make_diagonal_gradient_parent(std::uint64_t entropy) {
    constexpr double BASE = 500.0;
    constexpr double K    = 100.0;
    MapTilePayload p;
    p.tile    = TileCoord{11, 0, 0};
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
    p.biome.data.assign(static_cast<std::size_t>(N * N), 0);

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

MapTilePayload run_city(const MapTilePayload& parent, std::uint64_t entropy,
                        const TileCoord& tile, std::string* error_out = nullptr) {
    LuaSandbox sandbox;
    return sandbox.executeMap(read_city_lua(), make_ctx(parent, entropy, tile), error_out);
}

bool has_urban_cell(const MapTilePayload& payload) {
    for (const auto& b : payload.biome.data)
        if (static_cast<ZoneType>(b) == ZoneType::city) return true;
    return false;
}

bool has_feature(const MapTilePayload& payload, FeatureType type) {
    for (const auto& f : payload.features)
        if (f.type == type) return true;
    return false;
}

} // namespace

TEST(CityLuaTest, FileReadsAndRunsWithoutError) {
    const std::string source = read_city_lua();
    ASSERT_FALSE(source.empty()) << "generators/lua/map/city.lua not found or empty";

    const MapTilePayload parent = make_moderate_land_parent(1);
    std::string error;
    const MapTilePayload payload = run_city(parent, 2, TileCoord{12, 1, 0}, &error);

    EXPECT_TRUE(error.empty()) << error;
    EXPECT_FALSE(payload.elevation.empty());
    EXPECT_EQ(payload.generator, "lua.map.child.level12.default");
    EXPECT_EQ(payload.culture, "nordic");
}

TEST(CityLuaTest, FieldShapesMatch64x64) {
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_city(parent, 2, TileCoord{12, 1, 0});

    EXPECT_EQ(payload.elevation.w, N);
    EXPECT_EQ(payload.elevation.h, N);
    EXPECT_EQ(payload.temperature.w, N);
    EXPECT_EQ(payload.temperature.h, N);
    EXPECT_EQ(payload.moisture.w, N);
    EXPECT_EQ(payload.moisture.h, N);
    EXPECT_EQ(payload.biome.w, N);
    EXPECT_EQ(payload.biome.h, N);
}

TEST(CityLuaTest, EdgeDescriptorsMatchElevationBoundary) {
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_city(parent, 2, TileCoord{12, 1, 0});

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

TEST(CityLuaTest, MarksUrbanCellsOverModerateLand) {
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_city(parent, 2, TileCoord{12, 1, 0});
    EXPECT_TRUE(has_urban_cell(payload));
}

TEST(CityLuaTest, NeverMarksUrbanCellsOverOcean) {
    const MapTilePayload parent = make_all_ocean_parent(1);
    for (std::uint64_t entropy = 1; entropy <= 10; ++entropy) {
        const MapTilePayload payload = run_city(parent, entropy, TileCoord{12, 0, 0});
        EXPECT_FALSE(has_urban_cell(payload)) << "entropy=" << entropy;
        EXPECT_FALSE(has_feature(payload, FeatureType::Street)) << "entropy=" << entropy;
        EXPECT_FALSE(has_feature(payload, FeatureType::Park)) << "entropy=" << entropy;
        // M156: setZoneCandidates() is only called inside the same
        // any_buildable gate as markUrbanCells()/addStreet()/addPark().
        EXPECT_TRUE(payload.zone_candidates.empty()) << "entropy=" << entropy;
    }
}

TEST(CityLuaTest, ZoneCandidatesGridExistsAndVariesOverModerateLand) {
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_city(parent, 2, TileCoord{12, 1, 0});

    ASSERT_EQ(payload.zone_candidates.w, N);
    ASSERT_EQ(payload.zone_candidates.h, N);

    bool any_non_none = false;
    for (auto v : payload.zone_candidates.data)
        if (static_cast<ZoneCandidate>(v) != ZoneCandidate::none) any_non_none = true;
    EXPECT_TRUE(any_non_none) << "expected at least one urbanized block to get a real candidate";
}

TEST(CityLuaTest, ParkBlocksGetParkZoneCandidateAcrossSeveralEntropies) {
    const MapTilePayload parent = make_moderate_land_parent(1);
    bool found = false;
    for (std::uint64_t entropy = 1; entropy <= 20 && !found; ++entropy) {
        const MapTilePayload payload = run_city(parent, entropy, TileCoord{12, 1, 0});
        if (!has_feature(payload, FeatureType::Park)) continue;
        for (auto v : payload.zone_candidates.data) {
            if (static_cast<ZoneCandidate>(v) == ZoneCandidate::park) {
                found = true;
                break;
            }
        }
    }
    EXPECT_TRUE(found) << "expected at least one park-zoned block across 20 entropies";
}

// M155: each street line is bent by align_grid_line() into a fixed number
// of waypoints (SAMPLE_STEP_CELLS=8 over a 64-cell tile: 8 sampled steps +
// 1 trailing endpoint = 9), regardless of how much it actually bends.
constexpr std::size_t kCityStreetWaypoints = 9u;

TEST(CityLuaTest, PlacesAStreetGridOverModerateLand) {
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_city(parent, 2, TileCoord{12, 1, 0});

    int street_count = 0;
    for (const auto& f : payload.features) {
        if (f.type == FeatureType::Street) {
            ++street_count;
            EXPECT_FALSE(f.name.empty());
            ASSERT_EQ(f.points.size(), kCityStreetWaypoints);
        }
    }
    EXPECT_GT(street_count, 0);
}

// §5 #18 fix (M162 prerequisite): align_grid_line()'s first/last waypoint must land
// exactly on the tile's true boundary (half-open [min,max), so the max side is snapped
// just under the edge, not on it), not half a cell short -- otherwise
// MapBuilder::deriveEdgeCrossings()'s 0.01 m boundary match never fires.
TEST(CityLuaTest, StreetEndpointsSnapToTileBoundary) {
    const TileCoord tile{12, 1, 0};
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_city(parent, 2, tile);
    const WorldBounds bounds = tile.world_bounds();
    constexpr double kEps = 0.01;

    int checked = 0;
    for (const auto& f : payload.features) {
        if (f.type != FeatureType::Street) continue;
        ASSERT_GE(f.points.size(), 2u);
        const auto& first = f.points.front();
        const auto& last  = f.points.back();

        // Vertical streets (fixed x, sweeping z) hit N/S edges; horizontal streets
        // (fixed z, sweeping x) hit W/E edges -- tell them apart by which axis moved.
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

TEST(CityLuaTest, StreetsBendToFollowATerrainGradient) {
    const MapTilePayload parent  = make_diagonal_gradient_parent(1);
    const MapTilePayload payload = run_city(parent, 2, TileCoord{12, 0, 0});

    // city.lua's own loop order (gx-loop before gy-loop) means the first
    // Street feature is always the vertical line at gx=0.
    const MapFeature* first_street = nullptr;
    for (const auto& f : payload.features) {
        if (f.type == FeatureType::Street) { first_street = &f; break; }
    }
    ASSERT_NE(first_street, nullptr);
    ASSERT_EQ(first_street->points.size(), kCityStreetWaypoints);

    // Cross-tile seam fix (see city.lua's own header note): both endpoints
    // are now pinned to the nominal, unbent fixed_index, so they must land
    // on the exact same x -- that pin is what lets a neighboring sibling
    // tile's matching street meet this one exactly at the shared boundary
    // (proven separately by StreetsConnectAcrossSiblingTileBoundary below).
    const double x_first = first_street->points.front()[0];
    const double x_last  = first_street->points.back()[0];
    EXPECT_NEAR(x_last, x_first, 0.01);

    // Against a strong diagonal elevation gradient, the greedy contour-
    // following walk in align_grid_line() must still drift this vertical
    // street's x away from that pinned nominal line at an INTERIOR sample
    // -- proof M155 actually bends streets in the middle of the tile, not
    // a no-op that always reproduces a straight line.
    ASSERT_GT(first_street->points.size(), 2u);
    const double x_mid = first_street->points[first_street->points.size() / 2][0];
    EXPECT_GT(x_mid, x_first);
}

// Fix verification: align_grid_line()'s cross-tile boundary pinning must
// make two horizontally-adjacent sibling tiles' matching streets meet
// exactly at their shared edge, not kink apart the way an independent
// per-tile drift walk did before the fix.
TEST(CityLuaTest, StreetsConnectAcrossSiblingTileBoundary) {
    const MapTilePayload parent = make_diagonal_gradient_parent(1);
    const MapTilePayload west   = run_city(parent, 11, TileCoord{12, 0, 0});
    const MapTilePayload east   = run_city(parent, 47, TileCoord{12, 1, 0});

    auto horizontal_streets = [](const MapTilePayload& payload) {
        std::vector<const MapFeature*> out;
        for (const auto& f : payload.features) {
            if (f.type != FeatureType::Street) continue;
            const bool horizontal = std::abs(f.points.back()[0] - f.points.front()[0]) >
                                     std::abs(f.points.back()[1] - f.points.front()[1]);
            if (horizontal) out.push_back(&f);
        }
        return out;
    };

    const auto west_streets = horizontal_streets(west);
    const auto east_streets = horizontal_streets(east);
    ASSERT_FALSE(west_streets.empty());
    ASSERT_EQ(west_streets.size(), east_streets.size());

    const double shared_x = west.tile.world_bounds().max_x;
    EXPECT_NEAR(shared_x, east.tile.world_bounds().min_x, 0.01);

    for (std::size_t i = 0; i < west_streets.size(); ++i) {
        const auto& w_end   = west_streets[i]->points.back();
        const auto& e_start = east_streets[i]->points.front();
        EXPECT_NEAR(w_end[0], shared_x, 0.02) << "west street #" << i;
        EXPECT_NEAR(e_start[0], shared_x, 0.02) << "east street #" << i;
        EXPECT_NEAR(w_end[1], e_start[1], 0.01)
            << "west street #" << i << " (" << west_streets[i]->name << ") does not meet "
            << "east street #" << i << " (" << east_streets[i]->name << ") at the shared boundary";
    }
}

TEST(CityLuaTest, EventuallyPlacesAParkOverModerateLandAcrossSeveralEntropies) {
    const MapTilePayload parent = make_moderate_land_parent(1);
    bool found_park = false;
    for (std::uint64_t entropy = 1; entropy <= 20 && !found_park; ++entropy) {
        const MapTilePayload payload = run_city(parent, entropy, TileCoord{12, 1, 0});
        for (const auto& f : payload.features) {
            if (f.type == FeatureType::Park) {
                found_park = true;
                EXPECT_FALSE(f.name.empty());
                ASSERT_EQ(f.points.size(), 1u);
                break;
            }
        }
    }
    EXPECT_TRUE(found_park) << "expected at least one Park across 20 entropies over moderate land";
}

TEST(CityLuaTest, PlacesALakeOverNearSeaLevelTerrain) {
    const MapTilePayload parent  = make_near_sea_level_parent(1);
    const MapTilePayload payload = run_city(parent, 2, TileCoord{12, 1, 0});
    EXPECT_TRUE(has_feature(payload, FeatureType::Lake));
}

TEST(CityLuaTest, NeverPlacesALakeOverModerateLand) {
    const MapTilePayload parent = make_moderate_land_parent(1);
    for (std::uint64_t entropy = 1; entropy <= 10; ++entropy) {
        const MapTilePayload payload = run_city(parent, entropy, TileCoord{12, 0, 0});
        EXPECT_FALSE(has_feature(payload, FeatureType::Lake)) << "entropy=" << entropy;
    }
}

TEST(CityLuaTest, DeterministicForSameEntropy) {
    const MapTilePayload parent = make_moderate_land_parent(1);
    const MapTilePayload a = run_city(parent, 999, TileCoord{12, 1, 0});
    const MapTilePayload b = run_city(parent, 999, TileCoord{12, 1, 0});

    ASSERT_EQ(a.elevation.data.size(), b.elevation.data.size());
    EXPECT_EQ(a.elevation.data, b.elevation.data);
    EXPECT_EQ(a.temperature.data, b.temperature.data);
    EXPECT_EQ(a.moisture.data, b.moisture.data);
    EXPECT_EQ(a.biome.data, b.biome.data);
    EXPECT_EQ(a.zone_candidates.data, b.zone_candidates.data);
    ASSERT_EQ(a.features.size(), b.features.size());
    for (std::size_t i = 0; i < a.features.size(); ++i) {
        EXPECT_EQ(a.features[i].name, b.features[i].name);
        EXPECT_EQ(a.features[i].type, b.features[i].type);
    }
}

TEST(CityLuaTest, MissingParentReturnsEmptyPayloadNotCrash) {
    MapGenContext ctx;
    ctx.tile        = TileCoord{12, 0, 0};
    ctx.entropy     = 1;
    ctx.sea_level_m = 0.0;
    ctx.parent      = nullptr;

    LuaSandbox sandbox;
    std::string error;
    const MapTilePayload payload = sandbox.executeMap(read_city_lua(), ctx, &error);

    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(payload.elevation.empty());
}
