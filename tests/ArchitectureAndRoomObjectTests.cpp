// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// T195-T218 follow-on (2026-07-11, procedural-model-generator-roadmap):
// individual-load tests for the new architecture-tier generators
// (generators/lua/architecture/*) and the new bathroom/bedroom object
// generators (sink/wardrobe/nightstand) -- same LuaGenFixture pattern
// tests/LuaGeneratorTests.cpp already established, since none of these
// call scene:callGenerator themselves (composition tests for the room
// generators that DO compose them live in tests/LuaCompositionTests.cpp,
// which needs the global LuaGeneratorRegistry singleton).

#include <gtest/gtest.h>
#include "LuaGeneratorRegistry.hpp"
#include "LuaSandbox.hpp"
#include "ChunkGenerator.hpp"
#include "ZoneType.hpp"
#include "RegionType.hpp"

namespace {

const std::filesystem::path LUA_DIR = "generators/lua";

MeshWorld::ChunkContext make_ctx(uint64_t seed = 7) {
    MeshWorld::ChunkContext ctx;
    ctx.seed         = seed;
    ctx.zone         = MeshWorld::ZoneType::city;
    ctx.region       = MeshWorld::RegionType::park;
    ctx.chunk_size_m = 64.0f;
    ctx.coord        = {0, 0};
    return ctx;
}

struct ArchRoomFixture : ::testing::Test {
    MeshWorld::LuaGeneratorRegistry reg;
    MeshWorld::LuaSandbox           sandbox;
    void SetUp() override { reg.load_from_dir(LUA_DIR); }

    std::string run(const std::string& id, std::string* err = nullptr) {
        std::string local_err;
        auto& out = err ? *err : local_err;
        return sandbox.execute(reg.get(id), make_ctx(), &out);
    }
};

} // namespace

// ── architecture/window/double_pane.lua ───────────────────────────────────

TEST_F(ArchRoomFixture, DoublePaneWindowHasFrameMullionAndSill) {
    ASSERT_TRUE(reg.has("lua.architecture.window.double_pane"));
    std::string err;
    auto xml = run("lua.architecture.window.double_pane", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("id=\"mullion\""), std::string::npos) << xml;
    EXPECT_NE(xml.find("id=\"sill\""),    std::string::npos) << xml;
    EXPECT_NE(xml.find("pane_l"), std::string::npos) << xml;
    EXPECT_NE(xml.find("pane_r"), std::string::npos) << xml;
}

TEST_F(ArchRoomFixture, DoublePaneWindowShuttersAreExplicitlyControllable) {
    ASSERT_TRUE(reg.has("lua.architecture.window.double_pane"));
    // ChunkContext (and this fixture's plain run()) never populates
    // ctx.parameters (see LuaRuntime.cpp's own build_ctx_table note) -- the
    // only way to pass real parameters is via scene:callGenerator's sub_ctx,
    // so drive both explicit shutters states through a tiny host script via
    // the GLOBAL registry singleton callGenerator resolves against.
    MeshWorld::LuaGeneratorRegistry::instance().load_from_dir(LUA_DIR);
    constexpr const char* src = R"lua(
local M = {}
function M.generate(ctx, scene)
    scene:callGenerator("lua.architecture.window.double_pane", {parameters={shutters=true}}, {id="on"})
    scene:callGenerator("lua.architecture.window.double_pane", {parameters={shutters=false}}, {id="off"})
    scene:setMetadata({generator={id="test"}, object={type="test"}})
end
return M
)lua";
    std::string err;
    auto xml = sandbox.execute(src, make_ctx(), &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("on_0_shutter_l"), std::string::npos) << "shutters=true must add shutters\n" << xml;
    EXPECT_EQ(xml.find("off_1_shutter_l"), std::string::npos) << "shutters=false must omit them\n" << xml;
}

// ── architecture/door/front_panel.lua ─────────────────────────────────────

TEST_F(ArchRoomFixture, FrontPanelDoorHasFrameAndPanel) {
    ASSERT_TRUE(reg.has("lua.architecture.door.front_panel"));
    std::string err;
    auto xml = run("lua.architecture.door.front_panel", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("id=\"panel\""),  std::string::npos) << xml;
    EXPECT_NE(xml.find("id=\"handle\""), std::string::npos) << xml;
}

// ── architecture/roof/gable.lua ───────────────────────────────────────────

TEST_F(ArchRoomFixture, GableRoofHasSteppedSlopesAndRidge) {
    ASSERT_TRUE(reg.has("lua.architecture.roof.gable"));
    std::string err;
    auto xml = run("lua.architecture.roof.gable", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("step_l_1"), std::string::npos) << xml;
    EXPECT_NE(xml.find("step_r_1"), std::string::npos) << xml;
    EXPECT_NE(xml.find("id=\"ridge\""), std::string::npos) << xml;
}

// ── architecture/chimney/brick.lua ────────────────────────────────────────

TEST_F(ArchRoomFixture, BrickChimneyHasBodyAndCap) {
    ASSERT_TRUE(reg.has("lua.architecture.chimney.brick"));
    std::string err;
    auto xml = run("lua.architecture.chimney.brick", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("id=\"body\""), std::string::npos) << xml;
    EXPECT_NE(xml.find("id=\"cap\""),  std::string::npos) << xml;
}

// ── architecture/stairs/front_steps.lua ───────────────────────────────────

TEST_F(ArchRoomFixture, FrontStepsHasConfiguredStepCount) {
    ASSERT_TRUE(reg.has("lua.architecture.stairs.front_steps"));
    std::string err;
    auto xml = run("lua.architecture.stairs.front_steps", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("step_1"), std::string::npos) << xml;
    EXPECT_NE(xml.find("step_3"), std::string::npos) << "default step_count=3\n" << xml;
}

// ── architecture/fence/wood_picket.lua (G14) ──────────────────────────────

TEST_F(ArchRoomFixture, WoodPicketFenceHasPostsRailsAndPickets) {
    ASSERT_TRUE(reg.has("lua.architecture.fence.wood_picket"));
    std::string err;
    auto xml = run("lua.architecture.fence.wood_picket", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("id=\"post_l\""),      std::string::npos) << xml;
    EXPECT_NE(xml.find("id=\"post_r\""),      std::string::npos) << xml;
    EXPECT_NE(xml.find("id=\"rail_top\""),    std::string::npos) << xml;
    EXPECT_NE(xml.find("id=\"rail_bottom\""), std::string::npos) << xml;
    EXPECT_NE(xml.find("picket_1"),           std::string::npos) << "at least one picket\n" << xml;
}

// Default parameters (length=2.0, picket_width=0.08, picket_gap=0.06) must
// produce more than one picket -- proves the spacing math actually fires
// the loop more than once, not just a single hardcoded picket.
TEST_F(ArchRoomFixture, WoodPicketFenceProducesMultiplePicketsAtDefaultLength) {
    ASSERT_TRUE(reg.has("lua.architecture.fence.wood_picket"));
    std::string err;
    auto xml = run("lua.architecture.fence.wood_picket", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("picket_1"), std::string::npos) << xml;
    EXPECT_NE(xml.find("picket_2"), std::string::npos)
        << "Expected more than one picket at the default 2m length\n" << xml;
}

// ── architecture/gate/simple.lua (G14) ────────────────────────────────────

TEST_F(ArchRoomFixture, SimpleGateHasPostsPanelAndHardware) {
    ASSERT_TRUE(reg.has("lua.architecture.gate.simple"));
    std::string err;
    auto xml = run("lua.architecture.gate.simple", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("id=\"post_l\""),      std::string::npos) << xml;
    EXPECT_NE(xml.find("id=\"post_r\""),      std::string::npos) << xml;
    EXPECT_NE(xml.find("id=\"frame_top\""),   std::string::npos) << xml;
    EXPECT_NE(xml.find("picket_1"),           std::string::npos) << xml;
    EXPECT_NE(xml.find("id=\"hinge_top\""),   std::string::npos) << xml;
    EXPECT_NE(xml.find("id=\"hinge_bottom\""),std::string::npos) << xml;
    EXPECT_NE(xml.find("id=\"latch\""),       std::string::npos) << xml;
}

// ── object/sink.lua, object/wardrobe.lua, object/nightstand.lua ──────────

TEST_F(ArchRoomFixture, SinkHasPedestalBasinAndTap) {
    ASSERT_TRUE(reg.has("lua.object.sink.simple"));
    std::string err;
    auto xml = run("lua.object.sink.simple", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("id=\"pedestal\""), std::string::npos) << xml;
    EXPECT_NE(xml.find("id=\"basin\""),    std::string::npos) << xml;
    EXPECT_NE(xml.find("<cylinder"),       std::string::npos) << "tap\n" << xml;
}

TEST_F(ArchRoomFixture, WardrobeHasTwoDoors) {
    ASSERT_TRUE(reg.has("lua.object.wardrobe.simple"));
    std::string err;
    auto xml = run("lua.object.wardrobe.simple", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("door_l"), std::string::npos) << xml;
    EXPECT_NE(xml.find("door_r"), std::string::npos) << xml;
}

TEST_F(ArchRoomFixture, NightstandHasDrawerAndHandle) {
    ASSERT_TRUE(reg.has("lua.object.nightstand.simple"));
    std::string err;
    auto xml = run("lua.object.nightstand.simple", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("id=\"drawer\""), std::string::npos) << xml;
    EXPECT_NE(xml.find("id=\"handle\""), std::string::npos) << xml;
}
