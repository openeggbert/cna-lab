// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include <gtest/gtest.h>
#include "LuaSandbox.hpp"
#include "ChunkGenerator.hpp"
#include "ZoneType.hpp"
#include "RegionType.hpp"

namespace {

MeshWorld::ChunkContext make_ctx(uint64_t seed = 1) {
    MeshWorld::ChunkContext ctx;
    ctx.seed         = seed;
    ctx.zone         = MeshWorld::ZoneType::city;
    ctx.region       = MeshWorld::RegionType::park;
    ctx.chunk_size_m = 64.0f;
    ctx.coord.x      = 0;
    ctx.coord.y      = 0;
    return ctx;
}

// Minimal generator that uses scene:addBox
constexpr const char* BOX_GENERATOR = R"lua(
local M = {}
M.id = "lua.test.box"
function M.generate(ctx, scene)
    scene:addBox("floor", {
        position = {32, 0, 32},
        size     = {64, 0.1, 64},
        material = "grass"
    })
end
return M
)lua";

// Generator that uses all primitive types
constexpr const char* ALL_PRIMITIVES = R"lua(
local M = {}
function M.generate(ctx, scene)
    scene:addGround("dirt")
    scene:addPlane("p1", {
        position = {0, 0, 0},
        size     = {10, 10},
        material = "stone"
    })
    scene:addBox("b1", {
        position = {5, 0, 5},
        size     = {2, 3, 2},
        material = "brick"
    })
    scene:addCylinder("c1", {
        position = {10, 0, 10},
        radius   = 0.5,
        height   = 4,
        material = "metal"
    })
end
return M
)lua";

// Generator that calls setMetadata
constexpr const char* META_GENERATOR = R"lua(
local M = {}
M.id = "lua.test.meta"
function M.generate(ctx, scene)
    scene:addBox("b", {position={0,0,0}, size={1,1,1}, material="wood"})
    scene:setMetadata({
        generator  = { id = M.id, version = "0.1.0", language = "lua" },
        generation = { variationInput = ctx.variation }
    })
end
return M
)lua";

// Sandbox violation: io
constexpr const char* IO_ESCAPE = R"lua(
local M = {}
function M.generate(ctx, scene)
    local f = io.open("/etc/passwd", "r")
end
return M
)lua";

// Sandbox violation: os
constexpr const char* OS_ESCAPE = R"lua(
local M = {}
function M.generate(ctx, scene)
    os.execute("id")
end
return M
)lua";

// Sandbox violation: require
constexpr const char* REQUIRE_ESCAPE = R"lua(
local M = {}
function M.generate(ctx, scene)
    local s = require("socket")
end
return M
)lua";

// Syntax error
constexpr const char* SYNTAX_ERROR = R"lua(
this is not valid lua !!!
)lua";

} // namespace

// T053 — scene:addBox() produces valid MC3 XML containing <box>
TEST(LuaSandboxTests, AddBoxProducesValidXml) {
    MeshWorld::LuaSandbox sandbox;
    std::string error;
    std::string xml = sandbox.execute(BOX_GENERATOR, make_ctx(), &error);

    EXPECT_TRUE(error.empty()) << "Unexpected error: " << error;
    EXPECT_FALSE(xml.empty());
    EXPECT_NE(xml.find("<box"), std::string::npos)  << "Expected <box> in output:\n" << xml;
    EXPECT_NE(xml.find("<mc3"),  std::string::npos) << "Expected <mc3> root:\n" << xml;
}

// All primitive types produce their respective XML tags
TEST(LuaSandboxTests, AllPrimitivesProduceCorrectTags) {
    MeshWorld::LuaSandbox sandbox;
    std::string error;
    std::string xml = sandbox.execute(ALL_PRIMITIVES, make_ctx(), &error);

    EXPECT_TRUE(error.empty()) << error;
    EXPECT_NE(xml.find("<plane"),    std::string::npos);
    EXPECT_NE(xml.find("<box"),      std::string::npos);
    EXPECT_NE(xml.find("<cylinder"), std::string::npos);
}

// setMetadata injects <metadata> tag with JSON
TEST(LuaSandboxTests, SetMetadataInjectsTag) {
    MeshWorld::LuaSandbox sandbox;
    std::string error;
    std::string xml = sandbox.execute(META_GENERATOR, make_ctx(42), &error);

    EXPECT_TRUE(error.empty()) << error;
    EXPECT_NE(xml.find("<metadata"), std::string::npos) << xml;
    EXPECT_NE(xml.find("lua.test.meta"), std::string::npos) << xml;
}

// Regression test: a full-width chunk_seed()-shaped value (high bit set,
// i.e. > INT64_MAX as unsigned) must not crash ctx.variation construction.
// Previously threw "integer value will be misrepresented in lua" (sol2,
// under SOL_ALL_SAFETIES_ON) whenever ctx.seed exceeded INT64_MAX — ~50% of
// real chunk_seed() outputs, undetected because every prior test used small
// hand-picked seeds. Fixed by reinterpreting as int64_t before the sol2
// push (see build_ctx_table() in LuaRuntime.cpp).
TEST(LuaSandboxTests, HugeSeedWithHighBitSetDoesNotCrash) {
    MeshWorld::LuaSandbox sandbox;
    std::string error;
    std::string xml = sandbox.execute(META_GENERATOR, make_ctx(0xFFFFFFFFFFFFFFFFULL), &error);

    EXPECT_TRUE(error.empty()) << error;
    EXPECT_NE(xml.find("<metadata"), std::string::npos) << xml;
}

// T054 — io.open() is blocked
TEST(LuaSandboxTests, IoOpenIsBlocked) {
    MeshWorld::LuaSandbox sandbox;
    std::string error;
    std::string xml = sandbox.execute(IO_ESCAPE, make_ctx(), &error);

    EXPECT_TRUE(xml.empty());
    EXPECT_FALSE(error.empty()) << "Expected sandbox error for io access";
}

// T055 — os.execute() is blocked
TEST(LuaSandboxTests, OsExecuteIsBlocked) {
    MeshWorld::LuaSandbox sandbox;
    std::string error;
    std::string xml = sandbox.execute(OS_ESCAPE, make_ctx(), &error);

    EXPECT_TRUE(xml.empty());
    EXPECT_FALSE(error.empty()) << "Expected sandbox error for os access";
}

// T056 — require() is blocked
TEST(LuaSandboxTests, RequireIsBlocked) {
    MeshWorld::LuaSandbox sandbox;
    std::string error;
    std::string xml = sandbox.execute(REQUIRE_ESCAPE, make_ctx(), &error);

    EXPECT_TRUE(xml.empty());
    EXPECT_FALSE(error.empty()) << "Expected sandbox error for require";
}

// T057 — syntax error returns error string, does not crash
TEST(LuaSandboxTests, SyntaxErrorReturnedNoCrash) {
    MeshWorld::LuaSandbox sandbox;
    std::string error;
    std::string xml = sandbox.execute(SYNTAX_ERROR, make_ctx(), &error);

    EXPECT_TRUE(xml.empty());
    EXPECT_FALSE(error.empty()) << "Expected error string for syntax error";
}
