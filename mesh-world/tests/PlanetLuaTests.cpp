// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M095/M101 — structural-parity tests for generators/lua/map/planet.lua
// against src/generators/map/PlanetGenerator.cpp. Not byte-identical (the
// Lua port uses ctx.random()/ctx.noise() instead of the C++ generator's own
// hashing), but the same invariants tests/PlanetGeneratorTests.cpp checks
// for the C++ version must hold here too: continent count in [5,12] (the
// WorldConfig defaults this script hardcodes, see the script's header
// comment), field shapes, land/ocean ratio, elevation/biome/temperature
// consistency, edge descriptors, and determinism given entropy.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <sstream>

#include "LuaSandbox.hpp"
#include "Map/MapTilePayload.hpp"
#include "ZoneType.hpp"

using namespace MeshWorld;
using namespace MeshWorld::Map;

namespace {

// M236-M275 (MAP16, 2026-07-10): classify() now produces 6 distinct
// underwater outcomes (see ZoneType.hpp's is_ocean_family()), not always
// plain ZoneType::ocean -- wrap it here instead of comparing a single
// hardcoded ordinal.
static bool is_ocean_ordinal(std::uint8_t v) {
    return is_ocean_family(static_cast<ZoneType>(v));
}

std::string read_planet_lua() {
    std::ifstream ifs("generators/lua/map/planet.lua");
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

MapGenContext make_ctx(std::uint64_t entropy) {
    MapGenContext ctx;
    ctx.tile        = TileCoord{0, 0, 0};
    ctx.entropy     = entropy;
    ctx.sea_level_m = 0.0;
    ctx.parent      = nullptr;
    return ctx;
}

MapTilePayload run_planet(const std::string& source, std::uint64_t entropy,
                           std::string* error_out = nullptr) {
    LuaSandbox sandbox;
    return sandbox.executeMap(source, make_ctx(entropy), error_out);
}

} // namespace

TEST(PlanetLuaTest, FileReadsAndRunsWithoutError) {
    const std::string source = read_planet_lua();
    ASSERT_FALSE(source.empty()) << "generators/lua/map/planet.lua not found or empty";

    std::string error;
    const MapTilePayload payload = run_planet(source, 42, &error);
    EXPECT_TRUE(error.empty()) << "Unexpected error: " << error;
    EXPECT_EQ(payload.generator, "lua.map.planet.default");
    EXPECT_TRUE(payload.culture == "nordic" || payload.culture == "romance"
                || payload.culture == "desert") << payload.culture;
}

// MAP19, M317: payload.features now also holds MountainRange/River/Lake
// entries from the real Hydrology/MountainRanges bindings wired into
// planet.lua, so continent count must filter by FeatureType::Continent
// rather than assuming every feature is a continent (true before M317).
std::size_t count_continents(const MapTilePayload& payload) {
    return static_cast<std::size_t>(
        std::count_if(payload.features.begin(), payload.features.end(),
                      [](const MapFeature& f) { return f.type == FeatureType::Continent; }));
}

TEST(PlanetLuaTest, ContinentCountWithinConfiguredRange) {
    // Full planet generation is comparatively expensive under Lua (tens of
    // thousands of ctx.noise() sol2 calls per run) — 25 draws is enough to
    // check range membership without materially slowing the test suite.
    const std::string source = read_planet_lua();
    for (std::uint64_t e = 0; e < 25; ++e) {
        std::string error;
        const MapTilePayload payload = run_planet(source, e, &error);
        ASSERT_TRUE(error.empty()) << "entropy=" << e << " error=" << error;
        const std::size_t continents = count_continents(payload);
        EXPECT_GE(continents, 5u)  << "entropy=" << e;
        EXPECT_LE(continents, 12u) << "entropy=" << e;
    }
}

TEST(PlanetLuaTest, ContinentCountSpansTheRange) {
    // 80 draws over 8 possible values (5..12): probability of missing either
    // extreme is ~(7/8)^80 =~ 3e-5 per extreme, negligible flake risk.
    const std::string source = read_planet_lua();
    std::size_t seen_min = 99, seen_max = 0;
    for (std::uint64_t e = 0; e < 80; ++e) {
        const MapTilePayload payload = run_planet(source, e);
        const std::size_t   continents = count_continents(payload);
        seen_min = std::min(seen_min, continents);
        seen_max = std::max(seen_max, continents);
    }
    EXPECT_EQ(seen_min, 5u);
    EXPECT_EQ(seen_max, 12u);
}

TEST(PlanetLuaTest, FieldShapesMatchCppGenerator) {
    const MapTilePayload payload = run_planet(read_planet_lua(), 7);
    // src/generators/map/PlanetGenerator.cpp uses GRID_SIZE = 64.
    EXPECT_EQ(payload.elevation.w, 64);
    EXPECT_EQ(payload.elevation.h, 64);
    EXPECT_EQ(payload.temperature.w, payload.elevation.w);
    EXPECT_EQ(payload.temperature.h, payload.elevation.h);
    EXPECT_EQ(payload.biome.w, payload.elevation.w);
    EXPECT_EQ(payload.biome.h, payload.elevation.h);
}

TEST(PlanetLuaTest, LandOceanRatioWithinSaneBounds) {
    const std::string source = read_planet_lua();
    for (std::uint64_t e : {100ULL, 999ULL, 42000ULL, 0xdeadbeefULL, 777777ULL}) {
        const MapTilePayload payload = run_planet(source, e);
        ASSERT_FALSE(payload.biome.empty()) << "entropy=" << e;
        int land = 0;
        const int total = payload.biome.w * payload.biome.h;
        for (auto v : payload.biome.data)
            if (!is_ocean_ordinal(v)) ++land;
        const double ratio = static_cast<double>(land) / static_cast<double>(total);
        EXPECT_GE(ratio, 0.15) << "entropy=" << e << " land_ratio=" << ratio;
        EXPECT_LE(ratio, 0.55) << "entropy=" << e << " land_ratio=" << ratio;
    }
}

TEST(PlanetLuaTest, ElevationFieldConsistentWithLandMask) {
    const MapTilePayload payload = run_planet(read_planet_lua(), 12345);
    ASSERT_FALSE(payload.elevation.empty());

    const int w = payload.elevation.w;
    const int h = payload.elevation.h;
    for (int gy = 0; gy < h; ++gy) {
        for (int gx = 0; gx < w; ++gx) {
            const float elev     = payload.elevation.at(gx, gy);
            const bool  is_ocean = is_ocean_ordinal(payload.biome.at(gx, gy));
            if (!is_ocean)
                EXPECT_GT(elev, 0.0f) << "land cell (" << gx << "," << gy << ") below sea level";
            else
                EXPECT_LT(elev, 0.0f) << "ocean cell (" << gx << "," << gy << ") above sea level";
        }
    }
}

TEST(PlanetLuaTest, BiomeGridUsesZoneTypeOrdinalsWithBothLandAndOcean) {
    const MapTilePayload payload = run_planet(read_planet_lua(), 777);
    ASSERT_FALSE(payload.biome.empty());

    const std::uint8_t max_valid = static_cast<std::uint8_t>(ZoneType::empty);
    bool saw_ocean = false, saw_non_ocean = false;
    for (auto v : payload.biome.data) {
        EXPECT_LE(v, max_valid) << "biome ordinal out of ZoneType range";
        if (is_ocean_ordinal(v)) saw_ocean = true;
        else                     saw_non_ocean = true;
    }
    EXPECT_TRUE(saw_ocean);
    EXPECT_TRUE(saw_non_ocean);
}

TEST(PlanetLuaTest, TemperatureDecreasesPoleward) {
    const MapTilePayload payload = run_planet(read_planet_lua(), 99);
    const int W = payload.temperature.w;
    const int H = payload.temperature.h;
    ASSERT_GT(H, 0);

    auto avg_rows = [&](int y0, int y1) {
        double sum = 0.0;
        int    cnt = 0;
        for (int gy = y0; gy < y1; ++gy)
            for (int gx = 0; gx < W; ++gx, ++cnt)
                sum += payload.temperature.at(gx, gy);
        return sum / cnt;
    };
    const int mid = H / 2;
    const double avg_equator = avg_rows(mid - 2, mid + 2);
    const double avg_pole    = (avg_rows(0, 4) + avg_rows(H - 4, H)) * 0.5;
    EXPECT_GT(avg_equator, avg_pole);
}

TEST(PlanetLuaTest, TemperatureNeverExceedsEquatorTemp) {
    const MapTilePayload payload = run_planet(read_planet_lua(), 55);
    // WorldConfig default equator_temp_c = 30.0 (hardcoded in planet.lua too).
    for (auto t : payload.temperature.data)
        EXPECT_LE(t, 30.0f + 0.01f);
}

// M060 parity — moisture is real fBm noise (in [0,1], varying), not the flat
// 0.0 placeholder this script used before the C++ side got a real field.
TEST(PlanetLuaTest, MoistureFieldPopulatedAndVaries) {
    const MapTilePayload payload = run_planet(read_planet_lua(), 777);
    ASSERT_FALSE(payload.moisture.empty());
    EXPECT_EQ(payload.moisture.w, payload.elevation.w);
    EXPECT_EQ(payload.moisture.h, payload.elevation.h);

    bool all_same = true;
    const float first = payload.moisture.data.front();
    for (auto m : payload.moisture.data) {
        EXPECT_GE(m, 0.0f);
        EXPECT_LE(m, 1.0f);
        if (m != first) all_same = false;
    }
    EXPECT_FALSE(all_same) << "moisture should vary, not be a flat placeholder";
}

TEST(PlanetLuaTest, EdgeDescriptorsMatchElevationBoundary) {
    const MapTilePayload payload = run_planet(read_planet_lua(), 55555);
    const int W = payload.elevation.w;
    const int H = payload.elevation.h;

    ASSERT_EQ(static_cast<int>(payload.edges[0].elevation.size()), W);  // N
    ASSERT_EQ(static_cast<int>(payload.edges[1].elevation.size()), H);  // E
    ASSERT_EQ(static_cast<int>(payload.edges[2].elevation.size()), W);  // S
    ASSERT_EQ(static_cast<int>(payload.edges[3].elevation.size()), H);  // W

    for (int i = 0; i < W; ++i) {
        EXPECT_EQ(payload.edges[0].elevation[static_cast<std::size_t>(i)],
                  payload.elevation.at(i, 0))     << "N edge mismatch at i=" << i;
        EXPECT_EQ(payload.edges[2].elevation[static_cast<std::size_t>(i)],
                  payload.elevation.at(i, H - 1)) << "S edge mismatch at i=" << i;
    }
    for (int i = 0; i < H; ++i) {
        EXPECT_EQ(payload.edges[1].elevation[static_cast<std::size_t>(i)],
                  payload.elevation.at(W - 1, i)) << "E edge mismatch at i=" << i;
        EXPECT_EQ(payload.edges[3].elevation[static_cast<std::size_t>(i)],
                  payload.elevation.at(0, i))     << "W edge mismatch at i=" << i;
    }
}

TEST(PlanetLuaTest, DeterministicForSameEntropy) {
    const std::string source = read_planet_lua();
    const MapTilePayload a = run_planet(source, 12345);
    const MapTilePayload b = run_planet(source, 12345);

    ASSERT_EQ(a.elevation.data.size(), b.elevation.data.size());
    EXPECT_EQ(a.elevation.data, b.elevation.data);
    EXPECT_EQ(a.temperature.data, b.temperature.data);
    ASSERT_EQ(a.features.size(), b.features.size());
    for (std::size_t i = 0; i < a.features.size(); ++i) {
        EXPECT_EQ(a.features[i].name, b.features[i].name);
        EXPECT_EQ(a.features[i].points[0][0], b.features[i].points[0][0]);
        EXPECT_EQ(a.features[i].points[0][1], b.features[i].points[0][1]);
    }
    EXPECT_EQ(a.culture, b.culture);
}

TEST(PlanetLuaTest, DifferentEntropyDiffersMaterially) {
    const std::string source = read_planet_lua();
    const MapTilePayload a = run_planet(source, 1);
    const MapTilePayload b = run_planet(source, 2);

    EXPECT_NE(a.elevation.data, b.elevation.data);
}
