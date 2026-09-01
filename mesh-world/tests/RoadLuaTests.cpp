// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// R121 zone/chunk audit follow-up (2026-07-12) — generators/lua/zone/
// road.lua used to ALWAYS build a north-south (Z-axis) road regardless of
// ctx.exits, a real live correctness bug found by comparing it against
// the exit-aware C++ fallback (src/generators/RoadGenerator.cpp). These
// tests prove the fix: the road's own "road" <plane> element's `size="w
// d"` attribute is (narrow, full-length) for a north-south road and
// (full-length, narrow) for an east-west one — the two are structurally
// distinguishable without needing a full XML object model, just the
// plane's own real size attribute (mesh-craft's Mc3XmlWriter.cpp writes
// exactly `size="width depth"` for ObjectType::Plane).

#include <gtest/gtest.h>

#include <array>
#include <fstream>
#include <sstream>
#include <string>

#include "ChunkCoord.hpp"
#include "ChunkGenerator.hpp"
#include "LuaSandbox.hpp"
#include "RegionType.hpp"
#include "ZoneType.hpp"

using namespace MeshWorld;

namespace {

std::string read_road_lua() {
    std::ifstream ifs("generators/lua/zone/road.lua");
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

ChunkContext make_ctx(bool north, bool south, bool east, bool west) {
    ChunkContext ctx;
    ctx.coord.x = 0;
    ctx.coord.y = 0;
    ctx.seed = 1;
    ctx.zone = ZoneType::city;
    ctx.region = RegionType::road;
    ctx.chunk_size_m = 64.0f;
    ctx.exits.north_road = north;
    ctx.exits.south_road = south;
    ctx.exits.east_road  = east;
    ctx.exits.west_road  = west;
    return ctx;
}

// Extracts the "size" attribute of the <plane id="road" .../> element as
// {width, depth}. Fails the calling test (via ADD_FAILURE + a zeroed
// return) if not found -- deliberately simple substring parsing, not a
// full XML parse, matching this test file's own narrow scope.
std::array<float, 2> road_plane_size(const std::string& xml) {
    const auto id_pos = xml.find(R"(id="road")");
    if (id_pos == std::string::npos) {
        ADD_FAILURE() << "no <plane id=\"road\"> element found in generated XML";
        return {0.0f, 0.0f};
    }
    const auto size_pos = xml.find(R"(size=")", id_pos);
    if (size_pos == std::string::npos) {
        ADD_FAILURE() << "road element has no 'size' attribute";
        return {0.0f, 0.0f};
    }
    const auto value_start = size_pos + std::string(R"(size=")").size();
    const auto value_end = xml.find('"', value_start);
    std::istringstream iss(xml.substr(value_start, value_end - value_start));
    std::array<float, 2> size{0.0f, 0.0f};
    iss >> size[0] >> size[1];
    return size;
}

} // namespace

TEST(RoadLuaTest, FileReadsAndRunsWithoutError) {
    const std::string source = read_road_lua();
    ASSERT_FALSE(source.empty()) << "generators/lua/zone/road.lua not found or empty";

    ChunkContext ctx = make_ctx(true, true, false, false);
    LuaSandbox sandbox;
    std::string error;
    const std::string xml = sandbox.execute(source, ctx, &error);

    EXPECT_TRUE(error.empty()) << error;
    EXPECT_FALSE(xml.empty());
}

TEST(RoadLuaTest, NorthSouthExitsProduceANorthSouthRoad) {
    const std::string source = read_road_lua();
    ChunkContext ctx = make_ctx(/*north=*/true, /*south=*/true, /*east=*/false, /*west=*/false);
    LuaSandbox sandbox;
    const std::string xml = sandbox.execute(source, ctx);
    ASSERT_FALSE(xml.empty());

    const auto size = road_plane_size(xml);
    EXPECT_LT(size[0], size[1]) << "north-south road should be narrow (width) x long (depth), got "
                                << size[0] << " x " << size[1];
}

TEST(RoadLuaTest, EastWestExitsProduceAnEastWestRoad) {
    const std::string source = read_road_lua();
    ChunkContext ctx = make_ctx(/*north=*/false, /*south=*/false, /*east=*/true, /*west=*/true);
    LuaSandbox sandbox;
    const std::string xml = sandbox.execute(source, ctx);
    ASSERT_FALSE(xml.empty());

    const auto size = road_plane_size(xml);
    EXPECT_GT(size[0], size[1]) << "east-west road should be long (width) x narrow (depth), got "
                                << size[0] << " x " << size[1];
}

// A non-straight road is a centre patch plus exactly the edges declared by
// ctx.exits.  In particular, an absent exit must never grow a full-tile
// fallback road to that chunk boundary: that was the visible road-to-nowhere
// bug in the Lua-first path.
TEST(RoadLuaTest, AmbiguousExitsOnlyMaterialiseCanonicalArms) {
    const std::string source = read_road_lua();
    LuaSandbox sandbox;

    {
        ChunkContext ctx = make_ctx(true, true, true, true);
        const std::string xml = sandbox.execute(source, ctx);
        ASSERT_FALSE(xml.empty());
        EXPECT_NE(xml.find(R"(id="road_center")"), std::string::npos);
        EXPECT_NE(xml.find(R"(id="road_n")"), std::string::npos);
        EXPECT_NE(xml.find(R"(id="road_s")"), std::string::npos);
        EXPECT_NE(xml.find(R"(id="road_e")"), std::string::npos);
        EXPECT_NE(xml.find(R"(id="road_w")"), std::string::npos);
    }
    {
        ChunkContext ctx = make_ctx(false, false, false, false);
        const std::string xml = sandbox.execute(source, ctx);
        ASSERT_FALSE(xml.empty());
        EXPECT_NE(xml.find(R"(id="road_center")"), std::string::npos);
        EXPECT_EQ(xml.find(R"(id="road_n")"), std::string::npos);
        EXPECT_EQ(xml.find(R"(id="road_s")"), std::string::npos);
        EXPECT_EQ(xml.find(R"(id="road_e")"), std::string::npos);
        EXPECT_EQ(xml.find(R"(id="road_w")"), std::string::npos);
    }
}
