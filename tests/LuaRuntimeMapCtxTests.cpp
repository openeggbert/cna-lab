// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M092/M093 — map-generator Lua `ctx` table (level/tile_x/tile_y/
// tile_size_m/variation/parent/edges) and seeded ctx.noise()/ctx.random()/
// ctx.randomInt(). No LuaSandbox execute() entry point yet (M094) — this
// exercises the LuaRuntime(MapBuilder&, parent) constructor directly.
//
// Values crossing the Lua<->C++ boundary for comparison here are kept
// exactly representable in binary (integers, halves/quarters) so Lua's
// tostring() and C++'s expectations agree without floating-point-format
// mismatches; ctx.tile_size_m (not always "nice") is instead round-tripped
// via string.format("%.17g", ...) + std::stod for an exact numeric compare.

#include <gtest/gtest.h>

#include <string>

#include "LuaRuntime.hpp"
#include "MapBuilder.hpp"
#include "Map/MapTilePayload.hpp"

using namespace MeshWorld;
using namespace MeshWorld::Map;

namespace {

MapBuilder make_builder(TileCoord tile, std::uint64_t entropy) {
    return MapBuilder(tile, entropy, /*sea_level_m=*/0.0);
}

} // namespace

TEST(LuaRuntimeMapCtxTest, IntegerTileFieldsArePopulated) {
    MapBuilder builder = make_builder(TileCoord{5, 3, 7}, 0xABCDULL);
    LuaRuntime runtime(builder);

    // setMetadata(generator_id, culture) is (ab)used here purely as a channel
    // to smuggle the computed string back out for assertions.
    constexpr const char* SOURCE = R"lua(
local M = {}
function M.generate(ctx, map)
    map:setMetadata(ctx.level .. "|" .. ctx.tile_x .. "|" .. ctx.tile_y .. "|" ..
                     ctx.variation, "nordic")
end
return M
)lua";

    EXPECT_EQ(runtime.run(SOURCE), "");
    EXPECT_EQ(builder.payload().generator, "5|3|7|43981");
}

TEST(LuaRuntimeMapCtxTest, TileSizeMatchesTileCoordSizeM) {
    const TileCoord tile{5, 3, 7};
    MapBuilder builder = make_builder(tile, 1);
    LuaRuntime runtime(builder);

    constexpr const char* SOURCE = R"lua(
local M = {}
function M.generate(ctx, map)
    map:setMetadata(string.format("%.17g", ctx.tile_size_m), "nordic")
end
return M
)lua";

    EXPECT_EQ(runtime.run(SOURCE), "");
    const double reported = std::stod(builder.payload().generator);
    EXPECT_DOUBLE_EQ(reported, tile.size_m());
}

TEST(LuaRuntimeMapCtxTest, ParentAndEdgesAreNilAtLevelZero) {
    MapBuilder builder = make_builder(TileCoord{0, 0, 0}, 1);
    LuaRuntime runtime(builder, /*parent=*/nullptr);

    constexpr const char* SOURCE = R"lua(
local M = {}
function M.generate(ctx, map)
    map:setMetadata("parent=" .. tostring(ctx.parent == nil) ..
                     ",edges=" .. tostring(ctx.edges == nil), "nordic")
end
return M
)lua";

    EXPECT_EQ(runtime.run(SOURCE), "");
    EXPECT_EQ(builder.payload().generator, "parent=true,edges=true");
}

TEST(LuaRuntimeMapCtxTest, ParentGridsAndEdgesAreReadableFromLuaWhenPresent) {
    MapTilePayload parent;
    parent.tile              = TileCoord{4, 2, 6};
    parent.entropy           = 0x55ULL;
    parent.culture           = "romance";
    parent.elevation.w       = 2;
    parent.elevation.h       = 1;
    parent.elevation.data    = {10.0f, 20.0f};
    parent.temperature.w     = 2;
    parent.temperature.h     = 1;
    parent.temperature.data  = {1.0f, 2.0f};
    parent.moisture.w        = 2;
    parent.moisture.h        = 1;
    parent.moisture.data     = {0.5f, 0.25f};       // exact in binary
    parent.edges[0].elevation = {100.0f, 200.0f};   // N

    // TileCoord{5,4,12} really is child(0,0) of TileCoord{4,2,6}.
    MapBuilder builder = make_builder(TileCoord{5, 4, 12}, 99);
    LuaRuntime runtime(builder, &parent);

    constexpr const char* SOURCE = R"lua(
local M = {}
function M.generate(ctx, map)
    local p = ctx.parent
    local result = p.level .. "|" .. p.tile_x .. "|" .. p.tile_y .. "|" ..
                   p.culture .. "|" .. p.elevation.w .. "x" .. p.elevation.h .. "|" ..
                   p.elevation.data[1] .. "," .. p.elevation.data[2] .. "|" ..
                   p.temperature.data[2] .. "|" .. p.moisture.data[2] .. "|" ..
                   ctx.edges.N.elevation[1] .. "," .. ctx.edges.N.elevation[2]
    map:setMetadata(result, "nordic")
end
return M
)lua";

    EXPECT_EQ(runtime.run(SOURCE), "");
    EXPECT_EQ(builder.payload().generator,
              "4|2|6|romance|2x1|10.0,20.0|2.0|0.25|100.0,200.0");
}

TEST(LuaRuntimeMapCtxTest, NoiseIsDeterministicAndInUnitRange) {
    MapBuilder builder = make_builder(TileCoord{0, 0, 0}, 42);
    LuaRuntime runtime(builder);

    constexpr const char* SOURCE = R"lua(
local M = {}
function M.generate(ctx, map)
    local a = ctx.noise(1.5, 2.5)
    local b = ctx.noise(1.5, 2.5)
    local ok = (a == b) and (a >= 0.0) and (a < 1.0)
    map:setMetadata(tostring(ok), "nordic")
end
return M
)lua";

    EXPECT_EQ(runtime.run(SOURCE), "");
    EXPECT_EQ(builder.payload().generator, "true");
}

TEST(LuaRuntimeMapCtxTest, NoiseDependsOnTileEntropy) {
    constexpr const char* SOURCE = R"lua(
local M = {}
function M.generate(ctx, map)
    map:setMetadata(tostring(ctx.noise(3.3, 4.4)), "nordic")
end
return M
)lua";

    MapBuilder builder_a = make_builder(TileCoord{0, 0, 0}, 1);
    LuaRuntime runtime_a(builder_a);
    EXPECT_EQ(runtime_a.run(SOURCE), "");

    MapBuilder builder_b = make_builder(TileCoord{0, 0, 0}, 2);
    LuaRuntime runtime_b(builder_b);
    EXPECT_EQ(runtime_b.run(SOURCE), "");

    EXPECT_NE(builder_a.payload().generator, builder_b.payload().generator);
}

TEST(LuaRuntimeMapCtxTest, RandomProducesDistinctDeterministicSequence) {
    constexpr const char* SOURCE = R"lua(
local M = {}
function M.generate(ctx, map)
    local a, b, c = ctx.random(), ctx.random(), ctx.random()
    local in_range = a >= 0.0 and a < 1.0 and b >= 0.0 and b < 1.0
    local distinct  = (a ~= b) and (b ~= c)
    map:setMetadata(tostring(in_range) .. "|" .. tostring(distinct) .. "|" ..
                     tostring(a) .. "|" .. tostring(b) .. "|" .. tostring(c), "nordic")
end
return M
)lua";

    MapBuilder builder1 = make_builder(TileCoord{0, 0, 0}, 7);
    LuaRuntime runtime1(builder1);
    EXPECT_EQ(runtime1.run(SOURCE), "");

    MapBuilder builder2 = make_builder(TileCoord{0, 0, 0}, 7);
    LuaRuntime runtime2(builder2);
    EXPECT_EQ(runtime2.run(SOURCE), "");

    // Same entropy + same call sequence -> identical sequence of values.
    EXPECT_EQ(builder1.payload().generator, builder2.payload().generator);
    EXPECT_NE(builder1.payload().generator.find("true|true|"), std::string::npos);
}

TEST(LuaRuntimeMapCtxTest, RandomIntStaysWithinInclusiveRange) {
    constexpr const char* SOURCE = R"lua(
local M = {}
function M.generate(ctx, map)
    local ok = true
    for i = 1, 200 do
        local v = ctx.randomInt(5, 9)
        if v < 5 or v > 9 then ok = false end
    end
    map:setMetadata(tostring(ok), "nordic")
end
return M
)lua";

    MapBuilder builder = make_builder(TileCoord{0, 0, 0}, 123);
    LuaRuntime runtime(builder);
    EXPECT_EQ(runtime.run(SOURCE), "");
    EXPECT_EQ(builder.payload().generator, "true");
}

TEST(LuaRuntimeMapCtxTest, RandomAndRandomIntVaryAcrossDifferentTileEntropy) {
    constexpr const char* SOURCE = R"lua(
local M = {}
function M.generate(ctx, map)
    map:setMetadata(tostring(ctx.random()) .. "|" .. tostring(ctx.randomInt(0, 1000000)), "nordic")
end
return M
)lua";

    MapBuilder builder_a = make_builder(TileCoord{0, 0, 0}, 10);
    LuaRuntime runtime_a(builder_a);
    EXPECT_EQ(runtime_a.run(SOURCE), "");

    MapBuilder builder_b = make_builder(TileCoord{0, 0, 0}, 20);
    LuaRuntime runtime_b(builder_b);
    EXPECT_EQ(runtime_b.run(SOURCE), "");

    EXPECT_NE(builder_a.payload().generator, builder_b.payload().generator);
}
