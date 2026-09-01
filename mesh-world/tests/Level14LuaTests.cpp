// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M299 (MAP18) — structural tests for generators/lua/map/level14.lua
// (level 14), mirroring NeighborhoodLuaTests.cpp's style, including its
// M354 city-inheritance coverage plus this level's own "block layout"
// ZoneCandidate assignment (see level14.lua's own header).

#include <gtest/gtest.h>

#include <cstdint>
#include <fstream>
#include <sstream>

#include "LuaSandbox.hpp"
#include "Map/BiomeClassifier.hpp"
#include "Map/MapTilePayload.hpp"
#include "Map/ZoneCandidate.hpp"
#include "ZoneType.hpp"

using namespace MeshWorld;
using namespace MeshWorld::Map;

namespace {

constexpr int N = 64;

std::string read_level14_lua() {
    std::ifstream ifs("generators/lua/map/level14.lua");
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

MapTilePayload make_moderate_land_parent(std::uint64_t entropy) {
    MapTilePayload p;
    p.tile    = TileCoord{13, 0, 0};
    p.entropy = entropy;
    p.culture = "nordic";

    p.elevation.w = p.elevation.h = N;
    p.elevation.data.assign(static_cast<std::size_t>(N * N), 1000.0f);
    p.temperature.w = p.temperature.h = N;
    p.temperature.data.assign(static_cast<std::size_t>(N * N), 15.0f);
    p.moisture.w = p.moisture.h = N;
    p.moisture.data.assign(static_cast<std::size_t>(N * N), 0.4f);
    p.biome.w = p.biome.h = N;
    p.biome.data.assign(static_cast<std::size_t>(N * N),
                         static_cast<std::uint8_t>(BiomeClassifier::classify(1000.0, 15.0, 0.4, 0.0)));
    for (auto& e : p.edges) e.elevation.assign(static_cast<std::size_t>(N), 1000.0f);
    return p;
}

MapTilePayload make_half_urban_parent(std::uint64_t entropy) {
    MapTilePayload p = make_moderate_land_parent(entropy);
    for (int py = 0; py < N; ++py)
        for (int px = 0; px < N / 2; ++px)
            p.biome.data[static_cast<std::size_t>(py * N + px)] =
                static_cast<std::uint8_t>(ZoneType::city);
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

MapTilePayload run_level14(const MapTilePayload& parent, std::uint64_t entropy,
                            const TileCoord& tile, std::string* error_out = nullptr) {
    LuaSandbox sandbox;
    return sandbox.executeMap(read_level14_lua(), make_ctx(parent, entropy, tile), error_out);
}

} // namespace

TEST(Level14LuaTest, FileReadsAndRunsWithoutError) {
    const std::string source = read_level14_lua();
    ASSERT_FALSE(source.empty()) << "generators/lua/map/level14.lua not found or empty";

    const MapTilePayload parent = make_moderate_land_parent(1);
    std::string error;
    const MapTilePayload payload = run_level14(parent, 2, TileCoord{14, 1, 0}, &error);

    EXPECT_TRUE(error.empty()) << error;
    EXPECT_FALSE(payload.elevation.empty());
    EXPECT_EQ(payload.generator, "lua.map.child.level14.default");
    EXPECT_EQ(payload.culture, "nordic");
}

TEST(Level14LuaTest, FieldShapesMatch64x64) {
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_level14(parent, 2, TileCoord{14, 1, 0});
    EXPECT_EQ(payload.elevation.w, N);
    EXPECT_EQ(payload.biome.w, N);
    EXPECT_EQ(payload.biome.h, N);
}

TEST(Level14LuaTest, EdgeDescriptorsMatchElevationBoundary) {
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_level14(parent, 2, TileCoord{14, 1, 0});
    ASSERT_EQ(static_cast<int>(payload.edges[0].elevation.size()), N);
    for (int i = 0; i < N; ++i)
        EXPECT_EQ(payload.edges[0].elevation[static_cast<std::size_t>(i)],
                  payload.elevation.at(i, 0)) << "N edge mismatch at i=" << i;
}

TEST(Level14LuaTest, NeverInventsCityOverANonUrbanParent) {
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_level14(parent, 2, TileCoord{14, 1, 0});
    for (const auto& b : payload.biome.data)
        EXPECT_NE(static_cast<ZoneType>(b), ZoneType::city);
}

// M354: this level's own defining requirement -- an inherited city zone
// must survive down from the parent (level13.lua's own output one level
// up, which itself inherited from city.lua at level 12).
TEST(Level14LuaTest, InheritsCityFromParentBiomeInteriorOfUrbanHalf) {
    const MapTilePayload parent  = make_half_urban_parent(1);
    const MapTilePayload payload = run_level14(parent, 2, TileCoord{14, 0, 0});  // cx=0: samples parent cols [0,32]

    constexpr int gx = 0, gy = 0;  // parent column 0 exactly -- no rounding ambiguity
    EXPECT_EQ(static_cast<ZoneType>(payload.biome.data[static_cast<std::size_t>(gy * N + gx)]),
              ZoneType::city);
}

TEST(Level14LuaTest, DoesNotInheritCityOutsideTheUrbanParentHalf) {
    const MapTilePayload parent  = make_half_urban_parent(1);
    const MapTilePayload payload = run_level14(parent, 2, TileCoord{14, 1, 0});  // cx=1: samples parent cols [32,64]

    constexpr int gx = N - 1, gy = 0;  // parent column 63 -- no rounding ambiguity
    EXPECT_NE(static_cast<ZoneType>(payload.biome.data[static_cast<std::size_t>(gy * N + gx)]),
              ZoneType::city);
}

TEST(Level14LuaTest, DeterministicForSameEntropy) {
    const MapTilePayload parent = make_moderate_land_parent(1);
    const MapTilePayload a = run_level14(parent, 999, TileCoord{14, 1, 0});
    const MapTilePayload b = run_level14(parent, 999, TileCoord{14, 1, 0});
    EXPECT_EQ(a.elevation.data, b.elevation.data);
    EXPECT_EQ(a.biome.data, b.biome.data);
}

TEST(Level14LuaTest, MissingParentReturnsEmptyPayloadNotCrash) {
    MapGenContext ctx;
    ctx.tile        = TileCoord{14, 0, 0};
    ctx.entropy     = 1;
    ctx.sea_level_m = 0.0;
    ctx.parent      = nullptr;

    LuaSandbox sandbox;
    std::string error;
    const MapTilePayload payload = sandbox.executeMap(read_level14_lua(), ctx, &error);

    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(payload.elevation.empty());
}

// --- "Block layout": ZoneCandidate assignment (level14.lua's own header note #2) ---

TEST(Level14LuaTest, NoZoneCandidatesOverANonUrbanParent) {
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_level14(parent, 2, TileCoord{14, 1, 0});
    EXPECT_TRUE(payload.zone_candidates.empty());
}

TEST(Level14LuaTest, UrbanBlocksGetSmallHouseBlockCandidateInInheritedCityArea) {
    const MapTilePayload parent  = make_half_urban_parent(1);
    const MapTilePayload payload = run_level14(parent, 2, TileCoord{14, 0, 0});  // cx=0: urban half
    ASSERT_FALSE(payload.zone_candidates.empty());

    constexpr int gx = 0, gy = 0;  // interior of the inherited-urban half
    EXPECT_EQ(static_cast<Map::ZoneCandidate>(payload.zone_candidates.at(gx, gy)),
              Map::ZoneCandidate::small_house_block);
}

// A clean half-split parent isn't enough here: cx=0's own child tile only
// samples parent columns [0,32] (half the parent's width), so it lands
// almost entirely inside whichever half it's centered on -- not a genuine
// mix of urban/non-urban BLOCKS within one child tile. Splitting the
// parent at column 16 instead (urban < 16) means cx=0's [0,32]-wide sample
// range straddles that boundary partway through, giving a fully-urban
// block (gx 0-7, samples parent ~0-3.6) and a fully-non-urban block
// (gx 56-63, samples parent ~28.4-32) inside the SAME generate() call.
MapTilePayload make_quarter_urban_parent(std::uint64_t entropy) {
    MapTilePayload p = make_moderate_land_parent(entropy);
    for (int py = 0; py < N; ++py)
        for (int px = 0; px < N / 4; ++px)
            p.biome.data[static_cast<std::size_t>(py * N + px)] =
                static_cast<std::uint8_t>(ZoneType::city);
    return p;
}

TEST(Level14LuaTest, NonUrbanBlocksStayNoneEvenWhenOtherBlocksInTheSameTileAreUrban) {
    const MapTilePayload parent  = make_quarter_urban_parent(1);
    const MapTilePayload payload = run_level14(parent, 2, TileCoord{14, 0, 0});
    ASSERT_FALSE(payload.zone_candidates.empty());

    EXPECT_EQ(static_cast<Map::ZoneCandidate>(payload.zone_candidates.at(0, 0)),
              Map::ZoneCandidate::small_house_block)
        << "block [0-7]: entirely inside the urban quarter";
    EXPECT_EQ(static_cast<Map::ZoneCandidate>(payload.zone_candidates.at(N - 1, 0)),
              Map::ZoneCandidate::none)
        << "block [56-63]: entirely past the urban quarter, in the same tile";
}
