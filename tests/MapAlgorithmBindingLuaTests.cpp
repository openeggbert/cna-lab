// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP19, M313/M314/M315/M318 — structural + parity tests for the new
// map:traceRivers/carveRivers/appendHydrologyFeatures/generateMountainRanges/
// applyMountainRanges/appendMountainRangeFeatures/applyCoastalBeach/
// applySwampFlatnessCheck Lua bindings (LuaRuntime.cpp's register_map_api()).
//
// Unlike CountryLuaTests.cpp/etc, this does not read a generators/lua/map/
// level script -- it exercises the binding SURFACE itself via inline Lua
// source, independent of any one level's own use of it (M317's per-level
// wiring gets its own coverage in each level's existing test file).
//
// M318's actual parity requirement: PartyTest below builds the identical
// elevation/temperature/moisture grid on both sides, drives the identical
// sequence of calls -- once through MapBuilder directly in C++, once through
// the Lua bindings via LuaSandbox::executeMap() -- and asserts the two
// resulting payloads are field-for-field identical. Grid values are all
// small-integer-derived (modulo arithmetic only, no transcendental
// functions) so double(Lua)->float(FieldGrid) round-tripping can never
// introduce a rounding mismatch between the two sides.

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "LuaSandbox.hpp"
#include "MapBuilder.hpp"
#include "Map/BiomeRefinement.hpp"
#include "Map/FeatureNaming.hpp"
#include "Map/Hydrology.hpp"
#include "Map/MapTilePayload.hpp"
#include "Map/MountainRanges.hpp"

using namespace MeshWorld;
using namespace MeshWorld::Map;

namespace {

constexpr int    N       = 8;
constexpr double SEA_LVL = 0.0;

// Small-integer modulo formulas: exactly representable in both a Lua double
// and a C++ float, so the two sides can never diverge on FP rounding alone.
float elev_at(int gx, int gy) { return static_cast<float>(((gx * 13 + gy * 7) % 17) * 100.0 - 500.0); }
float temp_at(int gx, int gy) { return static_cast<float>(5.0 + (gx % 4) * 3.0); }
float moist_at(int gx, int gy) { return static_cast<float>(0.3 + ((gx + gy) % 5) * 0.15); }

std::vector<float> build_grid(float (*f)(int, int)) {
    std::vector<float> out(static_cast<std::size_t>(N * N));
    for (int gy = 0; gy < N; ++gy)
        for (int gx = 0; gx < N; ++gx) out[static_cast<std::size_t>(gy * N + gx)] = f(gx, gy);
    return out;
}

// Runs the full mountain-uplift -> river-carve -> classify -> refine ->
// name sequence directly through MapBuilder (no Lua involved) -- the
// "expected" side of the parity comparison.
MapTilePayload run_cpp_direct(std::uint64_t entropy) {
    MapBuilder builder(TileCoord{0, 0, 0}, entropy, SEA_LVL);

    FieldGrid elevation{N, N, build_grid(elev_at)};
    const std::vector<float> temperature = build_grid(temp_at);
    const std::vector<float> moisture    = build_grid(moist_at);

    const MountainRangeNetwork mtn = builder.generateMountainRanges(entropy, 2, 200.0, 1800.0);
    builder.applyMountainRanges(elevation, mtn, 400.0);

    const HydrologyNetwork hyd = builder.traceRivers(elevation);
    builder.carveRivers(elevation, hyd);

    builder.setBiomeField(N, N, elevation.data, temperature, moisture);

    builder.appendHydrologyFeatures(hyd, "nordic", entropy, 2);
    builder.appendMountainRangeFeatures(mtn, elevation, "nordic", entropy);

    builder.applyCoastalBeach(1, 100.0);
    builder.applySwampFlatnessCheck(200.0);

    return builder.payload();
}

// Mirrors run_cpp_direct()'s exact call sequence, but entirely through the
// Lua binding surface via LuaSandbox::executeMap() -- the "actual" side.
std::string lua_source_for_parity_test() {
    return R"LUA(
local M = {}
M.id = "test.map.algorithm.binding.parity"

M.generate = function(ctx, map)
    local W, H = 8, 8

    local function elev(gx, gy) return ((gx * 13 + gy * 7) % 17) * 100.0 - 500.0 end
    local function temp(gx, gy) return 5.0 + (gx % 4) * 3.0 end
    local function moist(gx, gy) return 0.3 + ((gx + gy) % 5) * 0.15 end

    local elevation, temperature, moisture = {}, {}, {}
    for gy = 0, H - 1 do
        for gx = 0, W - 1 do
            local i = gy * W + gx + 1
            elevation[i]   = elev(gx, gy)
            temperature[i] = temp(gx, gy)
            moisture[i]    = moist(gx, gy)
        end
    end

    local mtn = map:generateMountainRanges(ctx.variation, 2, 200.0, 1800.0)
    map:applyMountainRanges(W, H, elevation, mtn, 400.0)

    local hyd = map:traceRivers(W, H, elevation)
    map:carveRivers(W, H, elevation, hyd)

    map:setBiomeField(W, H, elevation, temperature, moisture)

    map:appendHydrologyFeatures(hyd, "nordic", ctx.variation, 2)
    map:appendMountainRangeFeatures(W, H, elevation, mtn, "nordic", ctx.variation)

    map:applyCoastalBeach(1, 100.0)
    map:applySwampFlatnessCheck(200.0)

    map:setMetadata(M.id, "nordic")
end

return M
)LUA";
}

MapTilePayload run_lua(std::uint64_t entropy, std::string* error_out = nullptr) {
    LuaSandbox    sandbox;
    MapGenContext ctx;
    ctx.tile        = TileCoord{0, 0, 0};
    ctx.entropy     = entropy;
    ctx.sea_level_m = SEA_LVL;
    ctx.parent      = nullptr;
    return sandbox.executeMap(lua_source_for_parity_test(), ctx, error_out);
}

void expect_features_equal(const std::vector<MapFeature>& a, const std::vector<MapFeature>& b) {
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i].type, b[i].type) << "feature " << i;
        EXPECT_EQ(a[i].name, b[i].name) << "feature " << i;
        ASSERT_EQ(a[i].points.size(), b[i].points.size()) << "feature " << i;
        for (std::size_t p = 0; p < a[i].points.size(); ++p) {
            EXPECT_DOUBLE_EQ(a[i].points[p][0], b[i].points[p][0]) << "feature " << i << " point " << p;
            EXPECT_DOUBLE_EQ(a[i].points[p][1], b[i].points[p][1]) << "feature " << i << " point " << p;
        }
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Binding-surface smoke tests (M313/M314/M315): each new call is reachable,
// returns the right shape, and never crashes the process on plausible input.
// ---------------------------------------------------------------------------

TEST(MapAlgorithmBindingLuaTest, TraceAndCarveRiversRunWithoutError) {
    const std::string source = R"LUA(
local M = {}
M.generate = function(ctx, map)
    local W, H = 8, 8
    local elevation, temperature, moisture = {}, {}, {}
    for i = 1, W * H do
        elevation[i]   = ((i * 37) % 23) * 80.0 - 400.0
        temperature[i] = 12.0
        moisture[i]    = 0.5
    end
    local network = map:traceRivers(W, H, elevation)
    map:carveRivers(W, H, elevation, network)
    map:setBiomeField(W, H, elevation, temperature, moisture)
    map:appendHydrologyFeatures(network, "nordic", ctx.variation)
    map:setMetadata("test.hydrology", "nordic")
end
return M
)LUA";
    std::string error;
    MapGenContext ctx;
    ctx.tile        = TileCoord{0, 0, 0};
    ctx.entropy     = 7;
    ctx.sea_level_m = SEA_LVL;
    LuaSandbox sandbox;
    const MapTilePayload payload = sandbox.executeMap(source, ctx, &error);
    EXPECT_TRUE(error.empty()) << error;
    EXPECT_FALSE(payload.elevation.empty());
}

TEST(MapAlgorithmBindingLuaTest, GenerateAndApplyMountainRangesRunWithoutError) {
    const std::string source = R"LUA(
local M = {}
M.generate = function(ctx, map)
    local W, H = 8, 8
    local elevation, temperature, moisture = {}, {}, {}
    for i = 1, W * H do
        elevation[i]   = 300.0
        temperature[i] = 0.0
        moisture[i]    = 0.4
    end
    local network = map:generateMountainRanges(ctx.variation, 3, 500.0, 2000.0)
    map:applyMountainRanges(W, H, elevation, network, 600.0)
    map:setBiomeField(W, H, elevation, temperature, moisture)
    map:appendMountainRangeFeatures(W, H, elevation, network, "nordic", ctx.variation)
    map:setMetadata("test.mountains", "nordic")
end
return M
)LUA";
    std::string error;
    MapGenContext ctx;
    ctx.tile        = TileCoord{0, 0, 0};
    ctx.entropy     = 9;
    ctx.sea_level_m = SEA_LVL;
    LuaSandbox sandbox;
    const MapTilePayload payload = sandbox.executeMap(source, ctx, &error);
    EXPECT_TRUE(error.empty()) << error;
    EXPECT_FALSE(payload.elevation.empty());
}

TEST(MapAlgorithmBindingLuaTest, BiomeRefinementCallsAreNoOpBeforeSetBiomeField) {
    // applyCoastalBeach()/applySwampFlatnessCheck() called before
    // setBiomeField() must not crash (payload_.biome is still empty) --
    // mirrors MapBuilder::applyCoastalBeach's own doc comment.
    const std::string source = R"LUA(
local M = {}
M.generate = function(ctx, map)
    map:applyCoastalBeach()
    map:applySwampFlatnessCheck()
    local elevation, temperature, moisture = {}, {}, {}
    for i = 1, 64 do elevation[i] = 100.0; temperature[i] = 10.0; moisture[i] = 0.5 end
    map:setBiomeField(8, 8, elevation, temperature, moisture)
    map:setMetadata("test.refinement.noop", "nordic")
end
return M
)LUA";
    std::string error;
    MapGenContext ctx;
    ctx.tile        = TileCoord{0, 0, 0};
    ctx.entropy     = 3;
    ctx.sea_level_m = SEA_LVL;
    LuaSandbox sandbox;
    const MapTilePayload payload = sandbox.executeMap(source, ctx, &error);
    EXPECT_TRUE(error.empty()) << error;
    EXPECT_FALSE(payload.biome.empty());
}

TEST(MapAlgorithmBindingLuaTest, ApplyCoastalBeachAndSwampFlatnessCheckRunWithDefaultsAndOverrides) {
    const std::string source = R"LUA(
local M = {}
M.generate = function(ctx, map)
    local elevation, temperature, moisture = {}, {}, {}
    for i = 1, 64 do elevation[i] = 50.0; temperature[i] = 8.0; moisture[i] = 0.85 end
    map:setBiomeField(8, 8, elevation, temperature, moisture)
    map:applyCoastalBeach()
    map:applySwampFlatnessCheck()
    map:applyCoastalBeach(2, 80.0)
    map:applySwampFlatnessCheck(120.0)
    map:setMetadata("test.refinement", "nordic")
end
return M
)LUA";
    std::string error;
    MapGenContext ctx;
    ctx.tile        = TileCoord{0, 0, 0};
    ctx.entropy     = 4;
    ctx.sea_level_m = SEA_LVL;
    LuaSandbox sandbox;
    const MapTilePayload payload = sandbox.executeMap(source, ctx, &error);
    EXPECT_TRUE(error.empty()) << error;
    EXPECT_FALSE(payload.biome.empty());
}

// ---------------------------------------------------------------------------
// M318 — parity: Lua-invoked output must exactly match direct C++ output.
// ---------------------------------------------------------------------------

TEST(MapAlgorithmBindingLuaTest, LuaInvokedOutputMatchesDirectCppCallAcrossSeveralEntropies) {
    for (std::uint64_t entropy = 1; entropy <= 5; ++entropy) {
        const MapTilePayload expected = run_cpp_direct(entropy);

        std::string error;
        const MapTilePayload actual = run_lua(entropy, &error);
        ASSERT_TRUE(error.empty()) << "entropy=" << entropy << ": " << error;

        ASSERT_EQ(actual.elevation.w, expected.elevation.w) << "entropy=" << entropy;
        ASSERT_EQ(actual.elevation.h, expected.elevation.h) << "entropy=" << entropy;
        ASSERT_EQ(actual.elevation.data.size(), expected.elevation.data.size()) << "entropy=" << entropy;
        for (std::size_t i = 0; i < expected.elevation.data.size(); ++i)
            EXPECT_FLOAT_EQ(actual.elevation.data[i], expected.elevation.data[i])
                << "entropy=" << entropy << " cell=" << i;

        ASSERT_EQ(actual.biome.data.size(), expected.biome.data.size()) << "entropy=" << entropy;
        for (std::size_t i = 0; i < expected.biome.data.size(); ++i)
            EXPECT_EQ(actual.biome.data[i], expected.biome.data[i])
                << "entropy=" << entropy << " cell=" << i;

        expect_features_equal(expected.features, actual.features);
    }
}
