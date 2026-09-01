// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// R105 — structural tests for CityGenerator, the native C++ port of
// generators/lua/map/city.lua (level 12). Mirrors CityLuaTests.cpp's own
// fixture/assertion style (moderate land / all ocean / near sea level /
// diagonal gradient), adapted to call CityGenerator::generate() directly
// instead of running the Lua sandbox — proving the C++ path produces the
// same KIND of output (urban cells, zone candidates, streets, parks,
// lakes), not necessarily byte-identical to the Lua version (independent
// implementations, different RNG mechanism).

#include <gtest/gtest.h>

#include <cstdint>

#include "Map/MapTilePayload.hpp"
#include "Map/ZoneCandidate.hpp"
#include "ZoneType.hpp"
#include "generators/map/CityGenerator.hpp"

using namespace MeshWorld;
using namespace MeshWorld::Map;

namespace {

constexpr int N = 64;

PlanetParams make_params() {
    PlanetParams p;
    p.planet_size_m  = 22585000.0;
    p.continents_min = 5;
    p.continents_max = 12;
    p.sea_level_m    = 0.0;
    p.equator_temp_c = 30.0;
    p.pole_temp_c    = -20.0;
    return p;
}

// Uniform 1000 m everywhere: comfortably land (>0) and comfortably below the
// 2500 m mountain threshold, AND comfortably outside the 50 m lake band —
// every site-find attempt succeeds, no lake is ever detected.
MapTilePayload make_moderate_land_parent() {
    MapTilePayload p;
    p.elevation.w = p.elevation.h = N;
    p.elevation.data.assign(static_cast<std::size_t>(N * N), 1000.0f);
    p.culture = "nordic";
    return p;
}

// Uniform -2000 m everywhere: no site-find attempt can ever succeed.
MapTilePayload make_all_ocean_parent() {
    MapTilePayload p;
    p.elevation.w = p.elevation.h = N;
    p.elevation.data.assign(static_cast<std::size_t>(N * N), -2000.0f);
    p.culture = "nordic";
    return p;
}

// Uniform 25 m everywhere: comfortably inside the [0, 50] m lake band.
MapTilePayload make_near_sea_level_parent() {
    MapTilePayload p;
    p.elevation.w = p.elevation.h = N;
    p.elevation.data.assign(static_cast<std::size_t>(N * N), 25.0f);
    p.culture = "nordic";
    return p;
}

// A diagonal elevation gradient, same shape CityLuaTests.cpp's own fixture
// uses, so a street's greedy contour-following walk has real terrain to
// bend against (a uniform fixture never exercises that code path).
MapTilePayload make_diagonal_gradient_parent() {
    constexpr double BASE = 500.0;
    constexpr double K    = 100.0;
    MapTilePayload p;
    p.elevation.w = p.elevation.h = N;
    p.elevation.data.resize(static_cast<std::size_t>(N * N));
    for (int py = 0; py < N; ++py) {
        for (int px = 0; px < N; ++px) {
            p.elevation.data[static_cast<std::size_t>(py * N + px)] =
                static_cast<float>(BASE + (px - py) * K);
        }
    }
    p.culture = "nordic";
    return p;
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

TEST(CityGeneratorTest, FieldShapesMatch64x64) {
    const MapTilePayload parent = make_moderate_land_parent();
    const CityGenerator  gen(make_params());
    const MapTilePayload payload = gen.generate(TileCoord{12, 1, 0}, &parent, 2);

    EXPECT_EQ(payload.elevation.w, N);
    EXPECT_EQ(payload.elevation.h, N);
    EXPECT_EQ(payload.temperature.w, N);
    EXPECT_EQ(payload.temperature.h, N);
    EXPECT_EQ(payload.moisture.w, N);
    EXPECT_EQ(payload.moisture.h, N);
    EXPECT_EQ(payload.biome.w, N);
    EXPECT_EQ(payload.biome.h, N);
    EXPECT_EQ(payload.generator, "cpp.map.city");
    EXPECT_EQ(payload.culture, "nordic");
}

TEST(CityGeneratorTest, MarksUrbanCellsOverModerateLand) {
    const MapTilePayload parent = make_moderate_land_parent();
    const CityGenerator  gen(make_params());
    const MapTilePayload payload = gen.generate(TileCoord{12, 1, 0}, &parent, 2);
    EXPECT_TRUE(has_urban_cell(payload));
}

TEST(CityGeneratorTest, NeverMarksUrbanCellsOverOcean) {
    const MapTilePayload parent = make_all_ocean_parent();
    const CityGenerator  gen(make_params());
    for (std::uint64_t entropy = 1; entropy <= 10; ++entropy) {
        const MapTilePayload payload = gen.generate(TileCoord{12, 0, 0}, &parent, entropy);
        EXPECT_FALSE(has_urban_cell(payload)) << "entropy=" << entropy;
        EXPECT_FALSE(has_feature(payload, FeatureType::Street)) << "entropy=" << entropy;
        EXPECT_FALSE(has_feature(payload, FeatureType::Park)) << "entropy=" << entropy;
        EXPECT_TRUE(payload.zone_candidates.empty()) << "entropy=" << entropy;
    }
}

TEST(CityGeneratorTest, ZoneCandidatesGridExistsAndVariesOverModerateLand) {
    const MapTilePayload parent = make_moderate_land_parent();
    const CityGenerator  gen(make_params());
    const MapTilePayload payload = gen.generate(TileCoord{12, 1, 0}, &parent, 2);

    ASSERT_EQ(payload.zone_candidates.w, N);
    ASSERT_EQ(payload.zone_candidates.h, N);

    bool any_non_none = false;
    for (auto v : payload.zone_candidates.data)
        if (static_cast<ZoneCandidate>(v) != ZoneCandidate::none) any_non_none = true;
    EXPECT_TRUE(any_non_none) << "expected at least one urbanized block to get a real candidate";
}

TEST(CityGeneratorTest, ParkBlocksGetParkZoneCandidateAcrossSeveralEntropies) {
    const MapTilePayload parent = make_moderate_land_parent();
    const CityGenerator  gen(make_params());
    bool found = false;
    for (std::uint64_t entropy = 1; entropy <= 20 && !found; ++entropy) {
        const MapTilePayload payload = gen.generate(TileCoord{12, 1, 0}, &parent, entropy);
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

// align_grid_line() samples every kSampleStepCells=8 steps over a 64-cell
// tile: 8 sampled steps + 1 trailing endpoint = 9 waypoints, regardless of
// how much a line actually bends.
constexpr std::size_t kCityStreetWaypoints = 9u;

TEST(CityGeneratorTest, PlacesAStreetGridOverModerateLand) {
    const MapTilePayload parent = make_moderate_land_parent();
    const CityGenerator  gen(make_params());
    const MapTilePayload payload = gen.generate(TileCoord{12, 1, 0}, &parent, 2);

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

TEST(CityGeneratorTest, StreetEndpointsSnapToTileBoundary) {
    const TileCoord tile{12, 1, 0};
    const MapTilePayload parent = make_moderate_land_parent();
    const CityGenerator  gen(make_params());
    const MapTilePayload payload = gen.generate(tile, &parent, 2);
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
            EXPECT_LT(last[1], bounds.max_z) << f.name;
        } else {
            EXPECT_NEAR(first[0], bounds.min_x, kEps) << f.name;
            EXPECT_NEAR(last[0], bounds.max_x, kEps) << f.name;
            EXPECT_LT(last[0], bounds.max_x) << f.name;
        }
        ++checked;
    }
    EXPECT_GT(checked, 0);
}

TEST(CityGeneratorTest, StreetsBendToFollowATerrainGradient) {
    const MapTilePayload parent = make_diagonal_gradient_parent();
    const CityGenerator  gen(make_params());
    const MapTilePayload payload = gen.generate(TileCoord{12, 0, 0}, &parent, 2);

    // Loop order (gx-loop before gy-loop) means the first Street feature is
    // always the vertical line at gx=0.
    const MapFeature* first_street = nullptr;
    for (const auto& f : payload.features) {
        if (f.type == FeatureType::Street) { first_street = &f; break; }
    }
    ASSERT_NE(first_street, nullptr);
    ASSERT_EQ(first_street->points.size(), kCityStreetWaypoints);

    // Both endpoints are pinned to the nominal, unbent fixed_index.
    const double x_first = first_street->points.front()[0];
    const double x_last  = first_street->points.back()[0];
    EXPECT_NEAR(x_last, x_first, 0.01);

    // Against a strong diagonal gradient, the greedy walk must still drift
    // this vertical street's x away from the pinned line at an interior
    // sample — proof it actually bends, not a no-op straight line.
    ASSERT_GT(first_street->points.size(), 2u);
    const double x_mid = first_street->points[first_street->points.size() / 2][0];
    EXPECT_GT(x_mid, x_first);
}

TEST(CityGeneratorTest, StreetsConnectAcrossSiblingTileBoundary) {
    const MapTilePayload parent = make_diagonal_gradient_parent();
    const CityGenerator  gen(make_params());
    const MapTilePayload west = gen.generate(TileCoord{12, 0, 0}, &parent, 11);
    const MapTilePayload east = gen.generate(TileCoord{12, 1, 0}, &parent, 47);

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
            << "west street #" << i << " does not meet east street #" << i << " at the shared boundary";
    }
}

TEST(CityGeneratorTest, EventuallyPlacesAParkOverModerateLandAcrossSeveralEntropies) {
    const MapTilePayload parent = make_moderate_land_parent();
    const CityGenerator  gen(make_params());
    bool found_park = false;
    for (std::uint64_t entropy = 1; entropy <= 20 && !found_park; ++entropy) {
        const MapTilePayload payload = gen.generate(TileCoord{12, 1, 0}, &parent, entropy);
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

TEST(CityGeneratorTest, PlacesALakeOverNearSeaLevelTerrain) {
    const MapTilePayload parent = make_near_sea_level_parent();
    const CityGenerator  gen(make_params());
    const MapTilePayload payload = gen.generate(TileCoord{12, 1, 0}, &parent, 2);
    EXPECT_TRUE(has_feature(payload, FeatureType::Lake));
}

TEST(CityGeneratorTest, NeverPlacesALakeOverModerateLand) {
    const MapTilePayload parent = make_moderate_land_parent();
    const CityGenerator  gen(make_params());
    for (std::uint64_t entropy = 1; entropy <= 10; ++entropy) {
        const MapTilePayload payload = gen.generate(TileCoord{12, 0, 0}, &parent, entropy);
        EXPECT_FALSE(has_feature(payload, FeatureType::Lake)) << "entropy=" << entropy;
    }
}

TEST(CityGeneratorTest, DeterministicForSameEntropy) {
    const MapTilePayload parent = make_moderate_land_parent();
    const CityGenerator  gen(make_params());
    const MapTilePayload a = gen.generate(TileCoord{12, 1, 0}, &parent, 999);
    const MapTilePayload b = gen.generate(TileCoord{12, 1, 0}, &parent, 999);

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

TEST(CityGeneratorTest, InheritsParentCultureWhenSet) {
    const MapTilePayload parent = make_moderate_land_parent();
    const CityGenerator  gen(make_params());
    const MapTilePayload payload = gen.generate(TileCoord{12, 1, 0}, &parent, 2);
    EXPECT_EQ(payload.culture, "nordic");
}

TEST(CityGeneratorTest, PicksACultureWhenParentHasNone) {
    MapTilePayload parent = make_moderate_land_parent();
    parent.culture.clear();
    const CityGenerator  gen(make_params());
    const MapTilePayload payload = gen.generate(TileCoord{12, 1, 0}, &parent, 2);
    EXPECT_FALSE(payload.culture.empty());
}
