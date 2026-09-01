// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// scene:callGenerator composition infra tests (procedural-model-generator-
// roadmap, 2026-07-11): the transform/id-prefix stack (Mc3SceneBuilder::
// pushTransform/popTransform), ctx.lod/ctx.exits, and ctx.random/randomInt
// for object/chunk-mode generators -- all previously documented
// (docs/lua-generators.md) but not implemented before this session.

#include <gtest/gtest.h>
#include <array>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include "LuaSandbox.hpp"
#include "LuaGeneratorRegistry.hpp"
#include "ContainmentRuleRegistry.hpp"
#include "StyleRegistry.hpp"
#include "BuiltinStyles.hpp"
#include "ChunkGenerator.hpp"
#include "ZoneType.hpp"
#include "RegionType.hpp"

namespace {

// callGenerator resolves ids via the GLOBAL LuaGeneratorRegistry singleton
// (the same one ChunkPipeline.cpp/ContentPackLoader.cpp use in production),
// NOT a per-test local instance -- must be populated once here. Idempotent
// (load_from_dir just overwrites the same ids), so safe even if another
// translation unit in this same test binary also populates it. Same for
// ContainmentRuleRegistry::instance() (ctx.containment.childrenOf()).
struct LuaCompositionFixture : ::testing::Test {
    static void SetUpTestSuite() {
        MeshWorld::LuaGeneratorRegistry::instance().load_from_dir("generators/lua");
        MeshWorld::ContainmentRuleRegistry::instance().load("data/taxonomy/containment.json");
    }

    MeshWorld::LuaSandbox sandbox;

    MeshWorld::ChunkContext make_ctx(uint64_t seed = 7) {
        MeshWorld::ChunkContext ctx;
        ctx.seed         = seed;
        ctx.zone         = MeshWorld::ZoneType::city;
        ctx.region       = MeshWorld::RegionType::park;
        ctx.chunk_size_m = 64.0f;
        ctx.coord        = {0, 0};
        return ctx;
    }
};

// Non-overlapping occurrences of `needle` in `haystack`. Used to count
// distinct object placements by a unique substring each one emits exactly
// once (e.g. "_trunk" -- only tree.lua's own generate() ever produces an
// object id ending in that suffix, once per call).
std::size_t count_occurrences(const std::string& haystack, const std::string& needle) {
    std::size_t count = 0, pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

// Finds the start tag containing id="<id>" and returns its position="x y
// z" attribute, parsed. Returns {0,0,0} if the element has a position
// attribute but its own value IS the origin -- Mc3XmlWriter's own
// nonzero3() gate omits the attribute entirely when all 3 components are
// zero (same gate RotationBindingTests.cpp's own find_rotation() helper
// already accounts for on the rotation attribute). Returns nullopt only
// if the id itself isn't found at all.
std::optional<std::array<float, 3>> find_position(const std::string& xml, const std::string& id) {
    const std::string needle = "id=\"" + id + "\"";
    const auto id_pos = xml.find(needle);
    if (id_pos == std::string::npos) return std::nullopt;
    const auto tag_end = xml.find('>', id_pos);
    if (tag_end == std::string::npos) return std::nullopt;
    const std::string tag = xml.substr(id_pos, tag_end - id_pos);

    const auto pos_pos = tag.find("position=\"");
    if (pos_pos == std::string::npos) return std::array<float, 3>{0.0f, 0.0f, 0.0f};
    const auto value_start = pos_pos + std::string("position=\"").size();
    const auto value_end = tag.find('"', value_start);
    const std::string value = tag.substr(value_start, value_end - value_start);

    std::istringstream iss(value);
    std::array<float, 3> out{0.0f, 0.0f, 0.0f};
    iss >> out[0] >> out[1] >> out[2];
    return out;
}

} // namespace

// ── callGenerator: basic composition ─────────────────────────────────────

TEST_F(LuaCompositionFixture, CallGeneratorMergesSubGeneratorGeometryIntoParent) {
    constexpr const char* src = R"lua(
local M = {}
function M.generate(ctx, scene)
    scene:addBox("marker", {position={0,0,0}, size={1,1,1}, material="wood_natural"})
    scene:callGenerator("lua.object.bench.simple", {variation=1, parameters={}}, {
        position = {5, 0, 3}, id = "b1"
    })
    scene:setMetadata({generator={id="test.compose"}, object={type="test"}})
end
return M
)lua";
    std::string err;
    auto xml = sandbox.execute(src, make_ctx(), &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("id=\"marker\""), std::string::npos) << "parent's own geometry\n" << xml;
    EXPECT_NE(xml.find("b1_0_seat"), std::string::npos)
        << "sub-generator geometry, prefixed with its placement id + auto counter\n" << xml;
}

TEST_F(LuaCompositionFixture, CallGeneratorAppliesPositionOffset) {
    // bench.lua's own "seat" box sits at local base-elevation {0, sh+0.02,
    // 0.08} with the default sh=0.44 -> {0, 0.46, 0.08}; Mc3DocumentBuilder::
    // box() stores the CENTER, adding sy/2 (seat's own sy=0.04) internally,
    // so the actual stored y is 0.46+0.02=0.48. Placed at {123, 0, 0}, the
    // world position must be that same local offset translated by the
    // placement, i.e. {123, 0.48, 0.08} -- not the untransformed local value.
    constexpr const char* src = R"lua(
local M = {}
function M.generate(ctx, scene)
    scene:callGenerator("lua.object.bench.simple", {variation=1, parameters={}}, {
        position = {123, 0, 0}, id = "b1"
    })
    scene:setMetadata({generator={id="test.compose"}, object={type="test"}})
end
return M
)lua";
    std::string err;
    auto xml = sandbox.execute(src, make_ctx(), &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("123 0.48 0.08"), std::string::npos)
        << "expected the sub-generator's local seat offset translated by the placement\n" << xml;
}

TEST_F(LuaCompositionFixture, CallGeneratorRotationChangesOutput) {
    // bench.lua's legs sit at nonzero local x -- rotating the placement
    // must actually change where they land, not just prefix the id.
    auto run_with_rotation = [&](float ry) {
        std::string src = R"lua(
local M = {}
function M.generate(ctx, scene)
    scene:callGenerator("lua.object.bench.simple", {variation=1, parameters={}}, {
        position = {0, 0, 0}, rotation_y = )lua" + std::to_string(ry) + R"lua(, id = "b1"
    })
    scene:setMetadata({generator={id="test.compose"}, object={type="test"}})
end
return M
)lua";
        std::string err;
        auto xml = sandbox.execute(src, make_ctx(), &err);
        EXPECT_TRUE(err.empty()) << "Lua error: " << err;
        return xml;
    };
    auto xml_0  = run_with_rotation(0.0f);
    auto xml_90 = run_with_rotation(90.0f);
    EXPECT_NE(xml_0, xml_90) << "rotation_y must actually affect emitted geometry";
}

TEST_F(LuaCompositionFixture, CallGeneratorNoCollisionAcrossRepeatedCalls) {
    // Calling the same sub-generator id twice with no explicit placement.id
    // must not collide -- each call gets its own auto-incrementing prefix.
    constexpr const char* src = R"lua(
local M = {}
function M.generate(ctx, scene)
    scene:callGenerator("lua.object.bench.simple", {variation=1, parameters={}}, {position={0,0,0}})
    scene:callGenerator("lua.object.bench.simple", {variation=2, parameters={}}, {position={10,0,0}})
    scene:setMetadata({generator={id="test.compose"}, object={type="test"}})
end
return M
)lua";
    std::string err;
    auto xml = sandbox.execute(src, make_ctx(), &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("bench_simple_0_seat"), std::string::npos) << xml;
    EXPECT_NE(xml.find("bench_simple_1_seat"), std::string::npos) << xml;
}

TEST_F(LuaCompositionFixture, CallGeneratorNestedCompositionWorks) {
    // The real proof composition scales: lua.building.house.detached calls
    // 7 sub-generators (door/2 windows/side window/roof/chimney/steps/
    // mailbox), one of which (mailbox) is a pre-existing object generator
    // this session didn't touch -- proving composition reaches the whole
    // existing library, not just newly-added architecture files.
    std::string err;
    auto xml = sandbox.execute(
        MeshWorld::LuaGeneratorRegistry::instance().get("lua.building.house.detached"),
        make_ctx(), &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("id=\"wall_front\""), std::string::npos) << "house's own shell\n" << xml;
    EXPECT_NE(xml.find("front_door_0_"), std::string::npos) << "door sub-generator\n" << xml;
    EXPECT_NE(xml.find("win_front_l_1_"), std::string::npos) << "left window\n" << xml;
    EXPECT_NE(xml.find("win_front_r_2_"), std::string::npos) << "right window\n" << xml;
    EXPECT_NE(xml.find("win_side_l_3_"), std::string::npos) << "side window (rotated)\n" << xml;
    EXPECT_NE(xml.find("roof_4_"), std::string::npos) << "roof sub-generator\n" << xml;
    EXPECT_NE(xml.find("chimney_5_"), std::string::npos) << "chimney sub-generator\n" << xml;
    EXPECT_NE(xml.find("steps_6_"), std::string::npos) << "front steps\n" << xml;
    EXPECT_NE(xml.find("mailbox_7_"), std::string::npos)
        << "mailbox -- a pre-existing, unrelated object generator\n" << xml;
}

// ── Regression: addBox `y` is a base, not a center (found while ────────
// implementing R113, fixed in both composed-house Lua files) ───────────

// Before this fix, house/detached.lua passed `fy + wh/2` as wall_front's
// own `y` -- since Mc3DocumentBuilder::box() always adds sy/2 internally
// (position.y = y + sy/2), the ACTUAL stored center ended up wh/2 too
// high, floating every wall above the foundation with a real gap. Proves
// the fix: wall_front's actual center must now equal fy + wh/2 (the
// correct center of a wall spanning [fy, fy+wh]), and the foundation's
// own top (fy) must touch the wall's own bottom (center - wh/2) with no
// gap -- an end-to-end geometric continuity check, not just "some
// position changed".
TEST_F(LuaCompositionFixture, HouseDetachedWallRestsOnFoundationNoFloatingGap) {
    std::string err;
    auto xml = sandbox.execute(
        MeshWorld::LuaGeneratorRegistry::instance().get("lua.building.house.detached"),
        make_ctx(), &err);
    ASSERT_TRUE(err.empty()) << "Lua error: " << err;

    const auto foundation_pos = find_position(xml, "foundation");
    const auto wall_pos       = find_position(xml, "wall_front");
    ASSERT_TRUE(foundation_pos.has_value()) << xml;
    ASSERT_TRUE(wall_pos.has_value()) << xml;

    // Defaults: fy=0.35, wh=2.8 (generators/lua/building/house/detached.lua).
    constexpr float kFy = 0.35f, kWh = 2.8f;
    const float foundation_top = (*foundation_pos)[1] + kFy / 2.0f;
    const float wall_bottom    = (*wall_pos)[1] - kWh / 2.0f;
    EXPECT_NEAR(foundation_top, wall_bottom, 0.001f)
        << "foundation top=" << foundation_top << " wall bottom=" << wall_bottom
        << " -- wall must rest directly on the foundation, no floating gap";
    EXPECT_NEAR((*wall_pos)[1], kFy + kWh / 2.0f, 0.001f)
        << "wall_front's real center should be fy + wh/2, not fy + wh (the pre-fix bug)";
}

// Same class of fix in simple_house.lua. Before the fix, wall_front's
// real center was wh (default 3.2) -- its bottom sat at wh/2 = 1.6m
// above ground, a large visible floating gap. The floor slab's own
// [-0.10, 0.10] extent (unaffected by this fix -- it was already written
// correctly) is a deliberate small embed below ground level, so the
// correct post-fix relationship is "wall bottom sits AT ground level,
// within the floor's own extent" (embedded like a real foundation), not
// "wall bottom exactly equals the floor's top surface".
TEST_F(LuaCompositionFixture, SimpleHouseWallAndDoorRestOnFloorNoFloatingGap) {
    std::string err;
    auto xml = sandbox.execute(
        MeshWorld::LuaGeneratorRegistry::instance().get("lua.building.simple_house.standard"),
        make_ctx(), &err);
    ASSERT_TRUE(err.empty()) << "Lua error: " << err;

    const auto floor_pos = find_position(xml, "floor");
    const auto wall_pos   = find_position(xml, "wall_front");
    const auto door_pos   = find_position(xml, "door_hole");
    ASSERT_TRUE(floor_pos.has_value()) << xml;
    ASSERT_TRUE(wall_pos.has_value()) << xml;
    ASSERT_TRUE(door_pos.has_value()) << xml;

    // Defaults: wh=3.2 (wall height), dh=2.10 (door height).
    constexpr float kWh = 3.2f, kDh = 2.10f;
    const float floor_bottom = (*floor_pos)[1] - 0.20f / 2.0f;  // floor size.y=0.20
    const float floor_top    = (*floor_pos)[1] + 0.20f / 2.0f;
    const float wall_bottom  = (*wall_pos)[1] - kWh / 2.0f;
    const float door_bottom  = (*door_pos)[1] - kDh / 2.0f;

    EXPECT_NEAR(wall_bottom, 0.0f, 0.001f)
        << "wall_front's real bottom should be at ground level (y=0), not "
        << "floor_top + wh/2 (the pre-fix bug, ~" << (floor_top + kWh / 2.0f) << ")";
    EXPECT_GE(wall_bottom, floor_bottom - 0.001f)
        << "wall bottom=" << wall_bottom << " floor bottom=" << floor_bottom
        << " -- wall must not sink below the floor slab's own extent";
    EXPECT_NEAR(door_bottom, wall_bottom, 0.001f)
        << "door must rest at the same ground level as the wall, not float "
           "at door height above it";
}

// G14 -- lua.building.garage.detached: own shell + inline roller door +
// composed side pedestrian door (reuses architecture/door/front_panel.lua
// rather than duplicating door geometry).
TEST_F(LuaCompositionFixture, GarageDetachedHasShellRollerDoorAndComposedSideDoor) {
    std::string err;
    auto xml = sandbox.execute(
        MeshWorld::LuaGeneratorRegistry::instance().get("lua.building.garage.detached"),
        make_ctx(), &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("id=\"wall_back\""),     std::string::npos) << "garage's own shell\n" << xml;
    EXPECT_NE(xml.find("id=\"wall_front_l\""),  std::string::npos) << "door-opening pillar\n" << xml;
    EXPECT_NE(xml.find("id=\"lintel\""),        std::string::npos) << xml;
    EXPECT_NE(xml.find("id=\"garage_door\""),   std::string::npos) << "inline roller door panel\n" << xml;
    EXPECT_NE(xml.find("door_groove_1"),        std::string::npos) << "roller door grooves\n" << xml;
    EXPECT_NE(xml.find("side_door_0_"),         std::string::npos)
        << "composed side door sub-generator (rotated onto the right wall)\n" << xml;
}

// side_door=false must omit the composed sub-generator call entirely, not
// just hide it -- proves the flag actually gates callGenerator, not just
// visibility.
TEST_F(LuaCompositionFixture, GarageDetachedOmitsSideDoorWhenDisabled) {
    constexpr const char* src = R"lua(
local M = {}
function M.generate(ctx, scene)
    scene:callGenerator("lua.building.garage.detached",
        {variation=1, parameters={side_door=false}}, {id="g1"})
    scene:setMetadata({generator={id="test"}, object={type="test"}})
end
return M
)lua";
    std::string err;
    auto xml = sandbox.execute(src, make_ctx(), &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("garage_door"), std::string::npos) << "roller door still present\n" << xml;
    EXPECT_EQ(xml.find("side_door"), std::string::npos)
        << "side_door=false must omit the composed door entirely\n" << xml;
}

TEST_F(LuaCompositionFixture, CallGeneratorUnknownIdRaisesError) {
    constexpr const char* src = R"lua(
local M = {}
function M.generate(ctx, scene)
    scene:callGenerator("lua.object.does_not_exist", {}, {})
end
return M
)lua";
    std::string err;
    auto xml = sandbox.execute(src, make_ctx(), &err);
    EXPECT_TRUE(xml.empty());
    EXPECT_FALSE(err.empty());
}

// ── addSphere / addCone: existed in C++, previously unbound to Lua ───────

TEST_F(LuaCompositionFixture, AddSphereAndAddConeAreCallable) {
    constexpr const char* src = R"lua(
local M = {}
function M.generate(ctx, scene)
    scene:addSphere("s1", {position={0,1,0}, radius=0.5, material="wood_natural"})
    scene:addCone("c1", {position={0,0,0}, radius=0.5, height=1.0, material="wood_natural"})
    scene:setMetadata({generator={id="test"}, object={type="test"}})
end
return M
)lua";
    std::string err;
    auto xml = sandbox.execute(src, make_ctx(), &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("<sphere"), std::string::npos) << xml;
    EXPECT_NE(xml.find("<cone"),   std::string::npos) << xml;
}

// ── ctx.lod / ctx.exits ───────────────────────────────────────────────────

TEST_F(LuaCompositionFixture, CtxLodIsPopulated) {
    constexpr const char* src = R"lua(
local M = {}
function M.generate(ctx, scene)
    scene:addBox("lod_marker", {position={0,0,0}, size={ctx.lod, 1, 1}, material="wood_natural"})
    scene:setMetadata({generator={id="test"}, object={type="test"}})
end
return M
)lua";
    MeshWorld::ChunkContext ctx = make_ctx();
    ctx.lod = 4;
    std::string err;
    auto xml = sandbox.execute(src, ctx, &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    // size={4,1,1} -> "4 1 1"
    EXPECT_NE(xml.find("4 1 1"), std::string::npos) << xml;
}

TEST_F(LuaCompositionFixture, CtxExitsIsPopulated) {
    constexpr const char* src = R"lua(
local M = {}
function M.generate(ctx, scene)
    local w = ctx.exits.north_road and 9 or 1
    scene:addBox("exit_marker", {position={0,0,0}, size={w,1,1}, material="wood_natural"})
    scene:setMetadata({generator={id="test"}, object={type="test"}})
end
return M
)lua";
    MeshWorld::ChunkContext ctx = make_ctx();
    ctx.exits.north_road = true;
    std::string err;
    auto xml = sandbox.execute(src, ctx, &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("9 1 1"), std::string::npos) << xml;
}

// ── ctx.random / ctx.randomInt (object/chunk mode) ────────────────────────

TEST_F(LuaCompositionFixture, CtxRandomIsDeterministicForSameSeed) {
    constexpr const char* src = R"lua(
local M = {}
function M.generate(ctx, scene)
    local a = ctx.randomInt(1, 1000000)
    scene:addBox("r", {position={0,0,0}, size={a,1,1}, material="wood_natural"})
    scene:setMetadata({generator={id="test"}, object={type="test"}})
end
return M
)lua";
    std::string err1, err2, err3;
    auto xml1 = sandbox.execute(src, make_ctx(42), &err1);
    auto xml2 = sandbox.execute(src, make_ctx(42), &err2);
    auto xml3 = sandbox.execute(src, make_ctx(43), &err3);
    EXPECT_TRUE(err1.empty());
    EXPECT_TRUE(err2.empty());
    EXPECT_TRUE(err3.empty());
    EXPECT_EQ(xml1, xml2) << "same seed must produce the same ctx.randomInt() draw";
    EXPECT_NE(xml1, xml3) << "different seeds should (almost certainly) differ";
}

TEST_F(LuaCompositionFixture, CtxRandomStaysWithinBounds) {
    constexpr const char* src = R"lua(
local M = {}
function M.generate(ctx, scene)
    for i = 1, 50 do
        local v = ctx.randomInt(10, 20)
        assert(v >= 10 and v <= 20, "randomInt out of bounds: " .. v)
        local f = ctx.random()
        assert(f >= 0.0 and f < 1.0, "random out of bounds: " .. f)
    end
    scene:addBox("ok", {position={0,0,0}, size={1,1,1}, material="wood_natural"})
    scene:setMetadata({generator={id="test"}, object={type="test"}})
end
return M
)lua";
    std::string err;
    auto xml = sandbox.execute(src, make_ctx(), &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("id=\"ok\""), std::string::npos);
}

// ── T216-T218: living_room / bedroom / bathroom, composed via callGenerator ─

TEST_F(LuaCompositionFixture, LivingRoomComposesSofaTableTvAndBookshelves) {
    ASSERT_TRUE(MeshWorld::LuaGeneratorRegistry::instance().has("lua.room.living_room.basic"));
    std::string err;
    auto xml = sandbox.execute(
        MeshWorld::LuaGeneratorRegistry::instance().get("lua.room.living_room.basic"),
        make_ctx(), &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("sofa_0_"),          std::string::npos) << xml;
    EXPECT_NE(xml.find("coffee_table_1_"),  std::string::npos) << xml;
    EXPECT_NE(xml.find("tv_2_"),            std::string::npos) << xml;
    EXPECT_NE(xml.find("bookshelf_a_3_"),   std::string::npos) << xml;
    EXPECT_NE(xml.find("bookshelf_b_4_"),   std::string::npos) << xml;
}

TEST_F(LuaCompositionFixture, BedroomComposesBedWardrobeAndNightstand) {
    ASSERT_TRUE(MeshWorld::LuaGeneratorRegistry::instance().has("lua.room.bedroom.basic"));
    std::string err;
    auto xml = sandbox.execute(
        MeshWorld::LuaGeneratorRegistry::instance().get("lua.room.bedroom.basic"),
        make_ctx(), &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("bed_0_"),        std::string::npos) << xml;
    EXPECT_NE(xml.find("wardrobe_1_"),   std::string::npos) << xml;
    EXPECT_NE(xml.find("nightstand_2_"), std::string::npos) << xml;
}

TEST_F(LuaCompositionFixture, BathroomComposesBathtubToiletAndSink) {
    ASSERT_TRUE(MeshWorld::LuaGeneratorRegistry::instance().has("lua.room.bathroom.basic"));
    std::string err;
    auto xml = sandbox.execute(
        MeshWorld::LuaGeneratorRegistry::instance().get("lua.room.bathroom.basic"),
        make_ctx(), &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("bathtub_0_"), std::string::npos) << xml;
    EXPECT_NE(xml.find("toilet_1_"),  std::string::npos) << xml;
    EXPECT_NE(xml.find("sink_2_"),    std::string::npos) << xml;
}

// ── ctx.containment.childrenOf: real gap, found+fixed 2026-07-11 ─────────
//
// ContainmentRuleRegistry + real data/taxonomy/containment.json content
// have existed since before this session; docs/taxonomy-and-containment.md
// documented `ctx.containment.childrenOf(...)` for just as long -- but no
// Lua binding for it ever existed until now.

TEST_F(LuaCompositionFixture, ContainmentChildrenOfReturnsRealRuleData) {
    constexpr const char* src = R"lua(
local M = {}
function M.generate(ctx, scene)
    local rules = ctx.containment.childrenOf("zone.park")
    assert(#rules > 0, "expected real containment rules for zone.park")
    local found_tree = false
    for _, r in ipairs(rules) do
        if r.child == "object.tree" then
            found_tree = true
            assert(r.min_count > 0, "expected a positive min_count")
            assert(r.probability > 0 and r.probability <= 1.0, "expected a valid probability")
        end
    end
    assert(found_tree, "expected zone.park to be able to contain object.tree")
    scene:addBox("ok", {position={0,0,0}, size={1,1,1}, material="wood_natural"})
    scene:setMetadata({generator={id="test"}, object={type="test"}})
end
return M
)lua";
    std::string err;
    auto xml = sandbox.execute(src, make_ctx(), &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("id=\"ok\""), std::string::npos);
}

TEST_F(LuaCompositionFixture, ContainmentChildrenOfUnknownParentReturnsEmpty) {
    constexpr const char* src = R"lua(
local M = {}
function M.generate(ctx, scene)
    local rules = ctx.containment.childrenOf("does.not.exist")
    assert(#rules == 0, "expected no rules for an unknown parent")
    scene:addBox("ok", {position={0,0,0}, size={1,1,1}, material="wood_natural"})
    scene:setMetadata({generator={id="test"}, object={type="test"}})
end
return M
)lua";
    std::string err;
    auto xml = sandbox.execute(src, make_ctx(), &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("id=\"ok\""), std::string::npos);
}

// ── ctx.style resolution through StyleRegistry: G11, found+fixed 2026-07-11 ─

TEST_F(LuaCompositionFixture, CtxStyleResolvesToARealPaletteTableWhenRegistered) {
    MeshWorld::register_builtin_styles();
    constexpr const char* src = R"lua(
local M = {}
function M.generate(ctx, scene)
    assert(type(ctx.style) == "table", "expected ctx.style to resolve to a table")
    assert(ctx.style.id == "central_europe_small_city", "expected the real style id")
    local lamp_mat = ctx.style.palette["park.lamp"]
    assert(lamp_mat == "metal_lamp_ornate", "expected a real palette value, got: " .. tostring(lamp_mat))
    scene:addBox("ok", {position={0,0,0}, size={1,1,1}, material="wood_natural"})
    scene:setMetadata({generator={id="test"}, object={type="test"}})
end
return M
)lua";
    MeshWorld::ChunkContext ctx = make_ctx();
    ctx.style = "central_europe_small_city";
    std::string err;
    auto xml = sandbox.execute(src, ctx, &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("id=\"ok\""), std::string::npos);
}

TEST_F(LuaCompositionFixture, CtxStyleFallsBackToStringWhenUnregistered) {
    constexpr const char* src = R"lua(
local M = {}
function M.generate(ctx, scene)
    assert(type(ctx.style) == "string", "expected ctx.style to stay a plain string")
    scene:addBox("ok", {position={0,0,0}, size={1,1,1}, material="wood_natural"})
    scene:setMetadata({generator={id="test"}, object={type="test"}})
end
return M
)lua";
    MeshWorld::ChunkContext ctx = make_ctx();
    ctx.style = "definitely_not_a_registered_style_xyz";
    std::string err;
    auto xml = sandbox.execute(src, ctx, &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("id=\"ok\""), std::string::npos);
}

// ── G13: zone/park.lua consumes real ctx.containment.childrenOf() data ──

// Proves the migration is real, not vacuous: data/taxonomy/containment.json's
// real "zone.park" -> "object.tree" rule (probability=1.0, min_count=8,
// max_count=40) must produce a VARYING tree count across seeds, not always
// the same fixed number the old hardcoded 16-entry position table gave.
TEST_F(LuaCompositionFixture, ParkTreeCountVariesAcrossSeedsWithinContainmentRuleRange) {
    std::set<std::size_t> distinct_counts;
    for (uint64_t seed = 1; seed <= 40; ++seed) {
        std::string err;
        const auto xml = sandbox.execute(
            MeshWorld::LuaGeneratorRegistry::instance().get("lua.zone.park"), make_ctx(seed), &err);
        ASSERT_TRUE(err.empty()) << "Lua error (seed " << seed << "): " << err;

        const auto tree_count = count_occurrences(xml, "_trunk\"");
        EXPECT_GE(tree_count, 8u)  << "seed " << seed << ": below containment rule's min_count";
        EXPECT_LE(tree_count, 40u) << "seed " << seed << ": above containment rule's max_count";
        distinct_counts.insert(tree_count);
    }
    EXPECT_GT(distinct_counts.size(), 1u)
        << "Expected tree count to vary across seeds (containment-rule-driven), "
           "not stay fixed like the old hardcoded 16-tree layout";
}

// Proves the fountain (min_count=0, max_count=1, probability=0.5) is
// genuinely CONDITIONAL now, not the old "always exactly one" behavior --
// checked statistically (loose bounds: real probability is 0.5, but the
// gate is `rule.probability > ctx.random()`, an exclusive comparison, and
// ctx.randomInt(0,1) adds its own independent roll on top of the gate, so
// the true observed rate isn't exactly 50% -- this only needs to prove
// "sometimes present, sometimes absent", not pin down the exact rate).
TEST_F(LuaCompositionFixture, ParkFountainIsConditionalNotAlwaysPresent) {
    int with_fountain = 0;
    constexpr int kTrials = 60;
    for (uint64_t seed = 1; seed <= kTrials; ++seed) {
        std::string err;
        const auto xml = sandbox.execute(
            MeshWorld::LuaGeneratorRegistry::instance().get("lua.zone.park"), make_ctx(seed), &err);
        ASSERT_TRUE(err.empty()) << "Lua error (seed " << seed << "): " << err;
        if (xml.find("fountain_") != std::string::npos) ++with_fountain;
    }
    EXPECT_GT(with_fountain, 0) << "Expected at least one seed to place a fountain";
    EXPECT_LT(with_fountain, kTrials) << "Expected at least one seed to OMIT the fountain "
                                          "(min_count=0, probability=0.5 -- not always present)";
}

// object.flower_bed has no matching Lua generator (checked
// generators/lua/object/*.lua) -- park.lua keeps it as inline geometry,
// still driven by the same containment rule's count/probability, not
// silently dropped from the migration.
TEST_F(LuaCompositionFixture, ParkFlowerBedStaysInlineNotCallGenerator) {
    std::string err;
    const auto xml = sandbox.execute(
        MeshWorld::LuaGeneratorRegistry::instance().get("lua.zone.park"), make_ctx(3), &err);
    ASSERT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("bed_1_flowers"), std::string::npos)
        << "Expected at least one inline flower bed (containment rule "
           "min_count=2, probability=0.8):\n" << xml;
}
