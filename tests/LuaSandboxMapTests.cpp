// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M094 — LuaSandbox's map-generator execute() path: builds a MapBuilder from
// a MapGenContext, runs the Lua script via LuaRuntime (same sandbox
// guarantees as the chunk-generator path), and returns the resulting
// Map::MapTilePayload.

#include <gtest/gtest.h>

#include "LuaSandbox.hpp"
#include "Map/MapTilePayload.hpp"
#include "ZoneType.hpp"

using namespace MeshWorld;
using namespace MeshWorld::Map;

namespace {

MapGenContext make_ctx(TileCoord tile = TileCoord{0, 0, 0},
                        std::uint64_t entropy = 42,
                        const MapTilePayload* parent = nullptr) {
    MapGenContext ctx;
    ctx.tile        = tile;
    ctx.entropy     = entropy;
    ctx.sea_level_m = 0.0;
    ctx.parent      = parent;
    return ctx;
}

// Minimal generator exercising map:/names:/ctx: together.
constexpr const char* MAP_GENERATOR = R"lua(
local M = {}
M.id = "lua.test.map"
function M.generate(ctx, map)
    local culture = names.culture(ctx.variation)
    map:setBiomeField(2, 1, {-10.0, 100.0}, {20.0, 20.0}, {0.5, 0.5})
    map:addCity(names.city(culture, 1), 0, 0)
    map:setMetadata(M.id, culture)
end
return M
)lua";

constexpr const char* IO_ESCAPE = R"lua(
local M = {}
function M.generate(ctx, map)
    local f = io.open("/etc/passwd", "r")
end
return M
)lua";

constexpr const char* OS_ESCAPE = R"lua(
local M = {}
function M.generate(ctx, map)
    os.execute("id")
end
return M
)lua";

constexpr const char* REQUIRE_ESCAPE = R"lua(
local M = {}
function M.generate(ctx, map)
    local s = require("socket")
end
return M
)lua";

constexpr const char* SYNTAX_ERROR = R"lua(
this is not valid lua !!!
)lua";

} // namespace

TEST(LuaSandboxMapTests, ExecuteMapReturnsPopulatedPayload) {
    LuaSandbox sandbox;
    std::string error;
    const MapTilePayload payload = sandbox.executeMap(MAP_GENERATOR, make_ctx(), &error);

    EXPECT_TRUE(error.empty()) << "Unexpected error: " << error;
    EXPECT_EQ(payload.tile, (TileCoord{0, 0, 0}));
    EXPECT_EQ(payload.entropy, 42u);
    EXPECT_EQ(payload.generator, "lua.test.map");
    EXPECT_TRUE(payload.culture == "nordic" || payload.culture == "romance"
                || payload.culture == "desert") << payload.culture;

    ASSERT_EQ(payload.elevation.w, 2);
    ASSERT_EQ(payload.elevation.h, 1);
    // depth=10m, shallow, moderate temp -> kelp_forest specifically since
    // M236-M275's ocean-family subtyping (MAP16, 2026-07-10).
    EXPECT_EQ(static_cast<ZoneType>(payload.biome.data[0]), ZoneType::kelp_forest);

    ASSERT_EQ(payload.features.size(), 1u);
    EXPECT_EQ(payload.features[0].type, FeatureType::City);
    EXPECT_FALSE(payload.features[0].name.empty());
}

TEST(LuaSandboxMapTests, ExecuteMapPassesTileAndEntropyThrough) {
    LuaSandbox sandbox;
    std::string error;
    const MapTilePayload payload =
        sandbox.executeMap(MAP_GENERATOR, make_ctx(TileCoord{3, 5, 9}, 777), &error);

    EXPECT_TRUE(error.empty()) << error;
    EXPECT_EQ(payload.tile, (TileCoord{3, 5, 9}));
    EXPECT_EQ(payload.entropy, 777u);
}

TEST(LuaSandboxMapTests, ExecuteMapExposesParentToCtx) {
    MapTilePayload parent;
    parent.tile     = TileCoord{0, 0, 0};
    parent.culture  = "desert";

    constexpr const char* SOURCE = R"lua(
local M = {}
function M.generate(ctx, map)
    local has_parent = ctx.parent ~= nil
    map:setMetadata(tostring(has_parent), ctx.parent and ctx.parent.culture or "none")
end
return M
)lua";

    LuaSandbox sandbox;
    std::string error;
    const MapTilePayload payload =
        sandbox.executeMap(SOURCE, make_ctx(TileCoord{1, 0, 0}, 1, &parent), &error);

    EXPECT_TRUE(error.empty()) << error;
    EXPECT_EQ(payload.generator, "true");
    EXPECT_EQ(payload.culture, "desert");
}

TEST(LuaSandboxMapTests, ExecuteMapCtxParentIsNilWithoutParent) {
    constexpr const char* SOURCE = R"lua(
local M = {}
function M.generate(ctx, map)
    map:setMetadata(tostring(ctx.parent == nil), "nordic")
end
return M
)lua";

    LuaSandbox sandbox;
    std::string error;
    const MapTilePayload payload = sandbox.executeMap(SOURCE, make_ctx(), &error);

    EXPECT_TRUE(error.empty()) << error;
    EXPECT_EQ(payload.generator, "true");
}

TEST(LuaSandboxMapTests, IoOpenIsBlocked) {
    LuaSandbox sandbox;
    std::string error;
    sandbox.executeMap(IO_ESCAPE, make_ctx(), &error);

    EXPECT_FALSE(error.empty()) << "Expected sandbox error for io access";
}

TEST(LuaSandboxMapTests, OsExecuteIsBlocked) {
    LuaSandbox sandbox;
    std::string error;
    sandbox.executeMap(OS_ESCAPE, make_ctx(), &error);

    EXPECT_FALSE(error.empty()) << "Expected sandbox error for os access";
}

TEST(LuaSandboxMapTests, RequireIsBlocked) {
    LuaSandbox sandbox;
    std::string error;
    sandbox.executeMap(REQUIRE_ESCAPE, make_ctx(), &error);

    EXPECT_FALSE(error.empty()) << "Expected sandbox error for require";
}

TEST(LuaSandboxMapTests, SyntaxErrorReturnedNoCrash) {
    LuaSandbox sandbox;
    std::string error;
    sandbox.executeMap(SYNTAX_ERROR, make_ctx(), &error);

    EXPECT_FALSE(error.empty()) << "Expected error string for syntax error";
}
