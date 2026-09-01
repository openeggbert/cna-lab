// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M116 — structural tests for generators/lua/map/region.lua (level 7),
// mirroring ContinentLuaTests.cpp's style (child-tile path, parent required).

#include <gtest/gtest.h>

#include <cstdint>
#include <fstream>
#include <sstream>

#include "LuaSandbox.hpp"
#include "Map/MapTilePayload.hpp"
#include "ZoneType.hpp"

using namespace MeshWorld;
using namespace MeshWorld::Map;

namespace {

constexpr int N = 64;

std::string read_region_lua() {
    std::ifstream ifs("generators/lua/map/region.lua");
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

// Uniform 1000 m everywhere: comfortably land (>0) and comfortably below the
// 2500 m mountain threshold even after the child's own ±(<=500 m) detail
// perturbation, regardless of entropy — every town-site attempt succeeds.
MapTilePayload make_moderate_land_parent(std::uint64_t entropy) {
    MapTilePayload p;
    p.tile    = TileCoord{6, 0, 0};
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
// perturbation, regardless of entropy — no town-site attempt can succeed.
MapTilePayload make_all_ocean_parent(std::uint64_t entropy) {
    MapTilePayload p;
    p.tile    = TileCoord{6, 0, 0};
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

MapGenContext make_ctx(const MapTilePayload& parent, std::uint64_t entropy, const TileCoord& tile) {
    MapGenContext ctx;
    ctx.tile        = tile;
    ctx.entropy     = entropy;
    ctx.sea_level_m = 0.0;
    ctx.parent      = &parent;
    return ctx;
}

MapTilePayload run_region(const MapTilePayload& parent, std::uint64_t entropy,
                          const TileCoord& tile, std::string* error_out = nullptr) {
    LuaSandbox sandbox;
    return sandbox.executeMap(read_region_lua(), make_ctx(parent, entropy, tile), error_out);
}

} // namespace

TEST(RegionLuaTest, FileReadsAndRunsWithoutError) {
    const std::string source = read_region_lua();
    ASSERT_FALSE(source.empty()) << "generators/lua/map/region.lua not found or empty";

    const MapTilePayload parent = make_moderate_land_parent(1);
    std::string error;
    const MapTilePayload payload = run_region(parent, 2, TileCoord{7, 1, 0}, &error);

    EXPECT_TRUE(error.empty()) << error;
    EXPECT_FALSE(payload.elevation.empty());
    EXPECT_EQ(payload.generator, "lua.map.child.level7.default");
    EXPECT_EQ(payload.culture, "nordic");
}

TEST(RegionLuaTest, FieldShapesMatch64x64) {
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_region(parent, 2, TileCoord{7, 1, 0});

    EXPECT_EQ(payload.elevation.w, N);
    EXPECT_EQ(payload.elevation.h, N);
    EXPECT_EQ(payload.temperature.w, N);
    EXPECT_EQ(payload.temperature.h, N);
    EXPECT_EQ(payload.moisture.w, N);
    EXPECT_EQ(payload.moisture.h, N);
    EXPECT_EQ(payload.biome.w, N);
    EXPECT_EQ(payload.biome.h, N);
}

TEST(RegionLuaTest, EdgeDescriptorsMatchElevationBoundary) {
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_region(parent, 2, TileCoord{7, 1, 0});

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

// M116's one piece of genuinely new functionality: town placement over
// suitable (non-ocean, non-mountain) terrain, eventually, across several
// entropies (town count itself is randomized 0..2 per tile).
TEST(RegionLuaTest, EventuallyPlacesATownOverModerateLandAcrossSeveralEntropies) {
    const MapTilePayload parent = make_moderate_land_parent(1);
    bool found_town = false;
    for (std::uint64_t entropy = 1; entropy <= 20 && !found_town; ++entropy) {
        const MapTilePayload payload = run_region(parent, entropy, TileCoord{7, 1, 0});
        for (const auto& f : payload.features) {
            if (f.type == FeatureType::Town) {
                found_town = true;
                EXPECT_FALSE(f.name.empty());
                ASSERT_EQ(f.points.size(), 1u);
                break;
            }
        }
    }
    EXPECT_TRUE(found_town) << "expected at least one Town across 20 entropies over moderate land";
}

TEST(RegionLuaTest, NeverPlacesATownOverOcean) {
    const MapTilePayload parent = make_all_ocean_parent(1);
    for (std::uint64_t entropy = 1; entropy <= 10; ++entropy) {
        const MapTilePayload payload = run_region(parent, entropy, TileCoord{7, 0, 0});
        for (const auto& f : payload.features)
            EXPECT_NE(f.type, FeatureType::Town) << "entropy=" << entropy;
    }
}

TEST(RegionLuaTest, DeterministicForSameEntropy) {
    const MapTilePayload parent = make_moderate_land_parent(1);
    const MapTilePayload a = run_region(parent, 999, TileCoord{7, 1, 0});
    const MapTilePayload b = run_region(parent, 999, TileCoord{7, 1, 0});

    ASSERT_EQ(a.elevation.data.size(), b.elevation.data.size());
    EXPECT_EQ(a.elevation.data, b.elevation.data);
    EXPECT_EQ(a.temperature.data, b.temperature.data);
    EXPECT_EQ(a.moisture.data, b.moisture.data);
    ASSERT_EQ(a.features.size(), b.features.size());
    for (std::size_t i = 0; i < a.features.size(); ++i) {
        EXPECT_EQ(a.features[i].name, b.features[i].name);
        EXPECT_EQ(a.features[i].type, b.features[i].type);
    }
}

TEST(RegionLuaTest, MissingParentReturnsEmptyPayloadNotCrash) {
    MapGenContext ctx;
    ctx.tile        = TileCoord{7, 0, 0};
    ctx.entropy     = 1;
    ctx.sea_level_m = 0.0;
    ctx.parent      = nullptr;

    LuaSandbox sandbox;
    std::string error;
    const MapTilePayload payload = sandbox.executeMap(read_region_lua(), ctx, &error);

    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(payload.elevation.empty());
}
