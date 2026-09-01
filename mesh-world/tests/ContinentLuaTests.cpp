// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M115 — structural tests for generators/lua/map/continent.lua (level 3),
// mirroring PlanetLuaTests.cpp's style but exercising the child-tile path
// (a parent payload is required; level 3 always has one).

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

std::string read_continent_lua() {
    std::ifstream ifs("generators/lua/map/continent.lua");
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

// A level-2 parent whose elevation rises linearly from 0 m (gx=0) to 6000 m
// (gx=63) — guarantees a cx=1 child (columns 32..63) sees mostly-mountainous
// terrain (~3000-6000 m, above MOUNTAIN_ELEV_M=2500).
MapTilePayload make_gradient_parent(std::uint64_t entropy) {
    MapTilePayload p;
    p.tile    = TileCoord{2, 0, 0};
    p.entropy = entropy;
    p.culture = "nordic";

    p.elevation.w = p.elevation.h = N;
    p.elevation.data.resize(static_cast<std::size_t>(N * N));
    p.temperature.w = p.temperature.h = N;
    p.temperature.data.assign(static_cast<std::size_t>(N * N), 15.0f);
    p.moisture.w = p.moisture.h = N;
    p.moisture.data.assign(static_cast<std::size_t>(N * N), 0.4f);
    p.biome.w = p.biome.h = N;
    p.biome.data.assign(static_cast<std::size_t>(N * N), 0);

    for (int gy = 0; gy < N; ++gy)
        for (int gx = 0; gx < N; ++gx)
            p.elevation.data[static_cast<std::size_t>(gy * N + gx)] =
                static_cast<float>(gx) / static_cast<float>(N - 1) * 6000.0f;

    for (auto& e : p.edges) e.elevation.assign(static_cast<std::size_t>(N), 0.0f);
    return p;
}

// Uniform -2000 m everywhere: comfortably below sea level even after detail
// perturbation, regardless of entropy -- no cell anywhere is land.
MapTilePayload make_all_ocean_parent(std::uint64_t entropy) {
    MapTilePayload p;
    p.tile    = TileCoord{2, 0, 0};
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

MapTilePayload run_continent(const MapTilePayload& parent, std::uint64_t entropy,
                              const TileCoord& tile, std::string* error_out = nullptr) {
    LuaSandbox sandbox;
    return sandbox.executeMap(read_continent_lua(), make_ctx(parent, entropy, tile), error_out);
}

} // namespace

TEST(ContinentLuaTest, FileReadsAndRunsWithoutError) {
    const std::string source = read_continent_lua();
    ASSERT_FALSE(source.empty()) << "generators/lua/map/continent.lua not found or empty";

    const MapTilePayload parent = make_gradient_parent(1);
    std::string error;
    const MapTilePayload payload = run_continent(parent, 2, TileCoord{3, 1, 0}, &error);

    EXPECT_TRUE(error.empty()) << error;
    EXPECT_FALSE(payload.elevation.empty());
    EXPECT_EQ(payload.generator, "lua.map.child.level3.default");
    EXPECT_EQ(payload.culture, "nordic");
}

TEST(ContinentLuaTest, FieldShapesMatch64x64) {
    const MapTilePayload parent  = make_gradient_parent(1);
    const MapTilePayload payload = run_continent(parent, 2, TileCoord{3, 1, 0});

    EXPECT_EQ(payload.elevation.w, N);
    EXPECT_EQ(payload.elevation.h, N);
    EXPECT_EQ(payload.temperature.w, N);
    EXPECT_EQ(payload.temperature.h, N);
    EXPECT_EQ(payload.moisture.w, N);
    EXPECT_EQ(payload.moisture.h, N);
    EXPECT_EQ(payload.biome.w, N);
    EXPECT_EQ(payload.biome.h, N);
}

TEST(ContinentLuaTest, EdgeDescriptorsMatchElevationBoundary) {
    const MapTilePayload parent  = make_gradient_parent(1);
    const MapTilePayload payload = run_continent(parent, 2, TileCoord{3, 1, 0});

    ASSERT_EQ(static_cast<int>(payload.edges[0].elevation.size()), N);  // N
    ASSERT_EQ(static_cast<int>(payload.edges[1].elevation.size()), N);  // E
    ASSERT_EQ(static_cast<int>(payload.edges[2].elevation.size()), N);  // S
    ASSERT_EQ(static_cast<int>(payload.edges[3].elevation.size()), N);  // W

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

// M115's one piece of genuinely new functionality: a MountainRange feature
// appears when the refined elevation clears the mountain threshold.
TEST(ContinentLuaTest, EmitsMountainRangeWhenElevationExceedsThreshold) {
    const MapTilePayload parent = make_gradient_parent(1);
    // cx=1 (tile_x=1, odd) samples the parent's high-elevation half (columns 32..63).
    const MapTilePayload payload = run_continent(parent, 2, TileCoord{3, 1, 0});

    bool found_range = false;
    for (const auto& f : payload.features) {
        if (f.type == FeatureType::MountainRange) {
            found_range = true;
            EXPECT_FALSE(f.points.empty());
            EXPECT_FALSE(f.name.empty());
        }
    }
    EXPECT_TRUE(found_range) << "expected a MountainRange feature over mostly-high terrain";
}

// MAP19, M317: mountain ranges are now real generated-and-uplifted terrain
// (Map::MountainRanges via the M314 binding), not a per-cell threshold scan
// -- MountainRanges::apply() actively RAISES land cells, so this can no
// longer assert "no range over flat, low LAND" (that premise stopped being
// true the moment uplift became real: flat, low land is exactly what a
// range's uplift changes). The invariant that DOES still hold post-M317:
// apply()/appendMountainRangeFeatures() only ever touch/name land cells
// (see both functions' own doc comments), so an all-ocean tile can never
// get a MountainRange feature, regardless of how many ranges are seeded.
TEST(ContinentLuaTest, NoMountainRangeOverAllOceanTerrain) {
    const MapTilePayload parent = make_all_ocean_parent(1);
    for (std::uint64_t entropy = 1; entropy <= 10; ++entropy) {
        const MapTilePayload payload = run_continent(parent, entropy, TileCoord{3, 0, 0});
        for (const auto& f : payload.features)
            EXPECT_NE(f.type, FeatureType::MountainRange)
                << "entropy=" << entropy << ": no range should be named over all-ocean terrain";
    }
}

TEST(ContinentLuaTest, DeterministicForSameEntropy) {
    const MapTilePayload parent = make_gradient_parent(1);
    const MapTilePayload a = run_continent(parent, 999, TileCoord{3, 1, 0});
    const MapTilePayload b = run_continent(parent, 999, TileCoord{3, 1, 0});

    ASSERT_EQ(a.elevation.data.size(), b.elevation.data.size());
    EXPECT_EQ(a.elevation.data, b.elevation.data);
    EXPECT_EQ(a.temperature.data, b.temperature.data);
    EXPECT_EQ(a.moisture.data, b.moisture.data);
}

TEST(ContinentLuaTest, MissingParentReturnsEmptyPayloadNotCrash) {
    MapGenContext ctx;
    ctx.tile        = TileCoord{3, 0, 0};
    ctx.entropy     = 1;
    ctx.sea_level_m = 0.0;
    ctx.parent      = nullptr;  // defensive case; level 3 always has one in practice

    LuaSandbox sandbox;
    std::string error;
    const MapTilePayload payload = sandbox.executeMap(read_continent_lua(), ctx, &error);

    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(payload.elevation.empty());
}
