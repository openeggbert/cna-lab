// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// T147-T157: Lua generator unit tests

#include <gtest/gtest.h>
#include "LuaGeneratorRegistry.hpp"
#include "LuaSandbox.hpp"
#include "ChunkGenerator.hpp"
#include "ZoneType.hpp"
#include "RegionType.hpp"
#include "ContainmentRuleRegistry.hpp"

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

struct LuaGenFixture : ::testing::Test {
    // G13 -- park.lua now delegates tree/bench/lamp/fountain placement via
    // scene:callGenerator(), which ALWAYS resolves against the GLOBAL
    // LuaGeneratorRegistry::instance() singleton (never this fixture's own
    // local `reg` below -- see LuaRuntime.cpp's callGenerator doc comment).
    // Also needs ContainmentRuleRegistry populated, or ctx.containment.
    // childrenOf("zone.park") returns empty and park.lua places nothing.
    // Other LuaGenFixture tests are unaffected -- they only ever touch
    // their own local `reg` member below, never the global singleton.
    static void SetUpTestSuite() {
        MeshWorld::LuaGeneratorRegistry::instance().load_from_dir(LUA_DIR);
        MeshWorld::ContainmentRuleRegistry::instance().load("data/taxonomy/containment.json");
    }

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

// ── T147: skeleton — fixture loads ok ────────────────────────────────────────

TEST(LuaGeneratorTests, RegistryLoadsFromDisk) {
    MeshWorld::LuaGeneratorRegistry reg;
    ASSERT_NO_THROW(reg.load_from_dir(LUA_DIR));
    EXPECT_GE(reg.list().size(), 10u) << "Expected at least 10 Lua generators";
}

// ── T148: chair.lua ──────────────────────────────────────────────────────────

TEST_F(LuaGenFixture, ChairHasCylinderLegsAndBoxSeat) {
    ASSERT_TRUE(reg.has("lua.object.chair.simple"));
    std::string err;
    auto xml = run("lua.object.chair.simple", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("<cylinder"), std::string::npos) << "Expected cylinder legs\n" << xml;
    EXPECT_NE(xml.find("<box"),      std::string::npos) << "Expected box seat\n"      << xml;
}

// ── T149: table.lua ──────────────────────────────────────────────────────────

TEST_F(LuaGenFixture, TableHasLegsAndTop) {
    ASSERT_TRUE(reg.has("lua.object.table.simple"));
    std::string err;
    auto xml = run("lua.object.table.simple", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("<box"), std::string::npos) << "Expected box elements (legs + top)\n" << xml;
    // table has 4 legs (leg_1..4) + tabletop
    EXPECT_NE(xml.find("leg_"), std::string::npos) << "Expected leg IDs (leg_1..4)\n" << xml;
}

// ── T150: tree.lua ───────────────────────────────────────────────────────────

// MAP20, M330 -- canopy is now an icosphere (matching ObjectDefinitionLibrary.
// cpp's own C++ tree definitions), not a box approximation.
TEST_F(LuaGenFixture, TreeHasTrunkAndCanopy) {
    ASSERT_TRUE(reg.has("lua.object.tree.deciduous"));
    std::string err;
    auto xml = run("lua.object.tree.deciduous", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("<cylinder"),  std::string::npos) << "Expected cylinder trunk\n"    << xml;
    EXPECT_NE(xml.find("<icosphere"), std::string::npos) << "Expected icosphere canopy\n"  << xml;
}

// M330 -- birch gets its own distinct single-icosphere, non-uniformly-
// scaled canopy (birch_tree()'s C++ shape), not the two-tier oak-style
// canopy every other species uses.
TEST_F(LuaGenFixture, BirchTreeUsesASingleNonUniformlyScaledCanopy) {
    ASSERT_TRUE(reg.has("lua.object.tree.deciduous"));
    MeshWorld::ChunkContext ctx = make_ctx();
    ctx.zone = MeshWorld::ZoneType::city;
    ctx.region = MeshWorld::RegionType::park;

    MeshWorld::LuaSandbox local_sandbox;
    std::string err;
    // p.species = "birch" via ctx.parameters isn't wired here (M.generate()
    // reads ctx.parameters, not exposed by make_ctx()) -- instead rely on
    // variation selecting birch deterministically (SPECIES_LIST[3] via
    // var % 4 == 2) and just check the shape invariant on whichever species
    // that seed happens to pick, falling back over a few seeds if needed.
    bool found_birch_shape = false;
    for (uint64_t seed = 0; seed < 8 && !found_birch_shape; ++seed) {
        MeshWorld::ChunkContext c = make_ctx(seed);
        auto xml = local_sandbox.execute(reg.get("lua.object.tree.deciduous"), c, &err);
        ASSERT_TRUE(err.empty()) << "Lua error: " << err;
        // "narrow" style (birch) emits exactly one icosphere, id="canopy";
        // "twin" style (every other species) emits "canopy_main"/
        // "canopy_top" instead -- a bare "canopy" hit with no "canopy_main"
        // means this seed picked birch.
        if (xml.find("id=\"canopy\"") != std::string::npos
            && xml.find("id=\"canopy_main\"") == std::string::npos) {
            found_birch_shape = true;
        }
    }
    EXPECT_TRUE(found_birch_shape) << "expected at least one of 8 seeds to pick birch's narrow-canopy style";
}

// ── T151: bench.lua ──────────────────────────────────────────────────────────

TEST_F(LuaGenFixture, BenchHasSeatAndLegs) {
    ASSERT_TRUE(reg.has("lua.object.bench.simple"));
    std::string err;
    auto xml = run("lua.object.bench.simple", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("<box"), std::string::npos) << "Expected box seat\n" << xml;
    // legs: cylinder or box
    bool has_leg = xml.find("<cylinder") != std::string::npos ||
                   xml.find("leg") != std::string::npos;
    EXPECT_TRUE(has_leg) << "Expected legs in bench output\n" << xml;
}

// ── T152: lamp.lua ───────────────────────────────────────────────────────────

TEST_F(LuaGenFixture, LampHasPoleAndHead) {
    ASSERT_TRUE(reg.has("lua.object.lamp.post"));
    std::string err;
    auto xml = run("lua.object.lamp.post", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("<cylinder"), std::string::npos) << "Expected cylinder pole\n" << xml;
    EXPECT_NE(xml.find("<box"),      std::string::npos) << "Expected box head\n"      << xml;
}

// ── T153: park.lua ───────────────────────────────────────────────────────────

TEST_F(LuaGenFixture, ParkContainsBenchTreeLamp) {
    ASSERT_TRUE(reg.has("lua.zone.park"));
    std::string err;
    auto xml = run("lua.zone.park", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("bench_"),    std::string::npos) << "Expected bench objects\n" << xml;
    EXPECT_NE(xml.find("tree_"),     std::string::npos) << "Expected tree objects\n"  << xml;
    // lamp or fountain is present
    bool has_light = xml.find("lamp_")     != std::string::npos ||
                     xml.find("fountain")  != std::string::npos;
    EXPECT_TRUE(has_light) << "Expected lamp or fountain in park\n" << xml;
}

// §5 #17 discovery: ctx.variation reinterprets ctx.seed as a signed int64
// (LuaRuntime.cpp), which is negative whenever the seed's high bit is set --
// roughly half of all real seeds. park.lua's place_tree()/place_flower_bed()
// used to index SPECIES/FLOWER_COLORS with `math.fmod(seed_i, 4) + 1`
// (C fmod semantics: same sign as the dividend), so a negative seed_i
// produced a <= 0 index -> nil -> a Lua runtime error. This bug was never
// exercised by any real run before ContentPackLoader was wired into the real
// binaries (§5 #17 fix) -- nothing had ever actually invoked this script
// outside a test using the small positive default seed=7. Fixed by switching
// to Lua's own `%` (floored, always non-negative for a positive divisor).
TEST_F(LuaGenFixture, ParkHandlesANegativeVariationSeedWithoutError) {
    // 0x8000000000000001 reinterprets as a large negative int64 --
    // ctx.variation (= static_cast<int64_t>(ctx.seed)) is negative here.
    MeshWorld::ChunkContext ctx = make_ctx(0x8000000000000001ULL);
    std::string err;
    auto xml = sandbox.execute(reg.get("lua.zone.park"), ctx, &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("tree_"), std::string::npos) << "Expected tree objects\n" << xml;
}

// ── T195-T212: furniture/object Lua generators (2026-07-11 T-series triage) ──

TEST_F(LuaGenFixture, BedHasFrameMattressAndPillows) {
    ASSERT_TRUE(reg.has("lua.object.bed.simple"));
    std::string err;
    auto xml = run("lua.object.bed.simple", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("<box"),      std::string::npos) << "Expected box frame/mattress\n" << xml;
    EXPECT_NE(xml.find("mattress"),  std::string::npos) << "Expected mattress\n" << xml;
    EXPECT_NE(xml.find("pillow_l"),  std::string::npos) << "Expected 2 pillows\n" << xml;
    EXPECT_NE(xml.find("pillow_r"),  std::string::npos) << "Expected 2 pillows\n" << xml;
}

TEST_F(LuaGenFixture, SofaHasSeatBackAndTwoArms) {
    ASSERT_TRUE(reg.has("lua.object.sofa.simple"));
    std::string err;
    auto xml = run("lua.object.sofa.simple", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("id=\"seat\""), std::string::npos) << "Expected seat\n" << xml;
    EXPECT_NE(xml.find("id=\"back\""), std::string::npos) << "Expected back\n" << xml;
    EXPECT_NE(xml.find("arm_l"), std::string::npos) << "Expected 2 arms\n" << xml;
    EXPECT_NE(xml.find("arm_r"), std::string::npos) << "Expected 2 arms\n" << xml;
}

TEST_F(LuaGenFixture, BookshelfHasFrameAndMultipleShelves) {
    ASSERT_TRUE(reg.has("lua.object.bookshelf.simple"));
    std::string err;
    auto xml = run("lua.object.bookshelf.simple", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("shelf_1"), std::string::npos) << "Expected shelves\n" << xml;
    EXPECT_NE(xml.find("shelf_4"), std::string::npos) << "Expected default 4 shelves\n" << xml;
}

TEST_F(LuaGenFixture, MicrowaveHasBodyAndHandle) {
    ASSERT_TRUE(reg.has("lua.object.microwave.simple"));
    std::string err;
    auto xml = run("lua.object.microwave.simple", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("id=\"body\""),   std::string::npos) << "Expected body\n" << xml;
    EXPECT_NE(xml.find("id=\"handle\""), std::string::npos) << "Expected handle\n" << xml;
}

TEST_F(LuaGenFixture, OvenHasBodyAndDoor) {
    ASSERT_TRUE(reg.has("lua.object.oven.simple"));
    std::string err;
    auto xml = run("lua.object.oven.simple", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("id=\"body\""), std::string::npos) << "Expected body\n" << xml;
    EXPECT_NE(xml.find("id=\"door\""), std::string::npos) << "Expected door\n" << xml;
}

TEST_F(LuaGenFixture, ToiletHasBaseAndTank) {
    ASSERT_TRUE(reg.has("lua.object.toilet.simple"));
    std::string err;
    auto xml = run("lua.object.toilet.simple", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("id=\"base\""), std::string::npos) << "Expected base\n" << xml;
    EXPECT_NE(xml.find("id=\"tank\""), std::string::npos) << "Expected tank\n" << xml;
}

TEST_F(LuaGenFixture, BathtubHasBody) {
    ASSERT_TRUE(reg.has("lua.object.bathtub.simple"));
    std::string err;
    auto xml = run("lua.object.bathtub.simple", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("id=\"body\""), std::string::npos) << "Expected body\n" << xml;
}

// Wheels are flattened addIcoSphere ellipsoids, not addCylinder -- see
// car.lua's own header comment for why (no rx/rz rotation binding exists).
TEST_F(LuaGenFixture, CarHasBodyRoofAndFourWheels) {
    ASSERT_TRUE(reg.has("lua.object.car.sedan"));
    std::string err;
    auto xml = run("lua.object.car.sedan", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("id=\"body\""), std::string::npos) << "Expected body\n" << xml;
    EXPECT_NE(xml.find("id=\"roof\""), std::string::npos) << "Expected roof\n" << xml;
    EXPECT_NE(xml.find("<icosphere"),  std::string::npos) << "Expected icosphere wheels\n" << xml;
    for (const char* w : {"wheel_fl", "wheel_fr", "wheel_rl", "wheel_rr"}) {
        EXPECT_NE(xml.find(w), std::string::npos) << "Expected " << w << "\n" << xml;
    }
}

TEST_F(LuaGenFixture, BicycleHasTwoWheelsAndFrame) {
    ASSERT_TRUE(reg.has("lua.object.bicycle.simple"));
    std::string err;
    auto xml = run("lua.object.bicycle.simple", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("wheel_front"),  std::string::npos) << "Expected front wheel\n" << xml;
    EXPECT_NE(xml.find("wheel_rear"),   std::string::npos) << "Expected rear wheel\n" << xml;
    EXPECT_NE(xml.find("frame_main"),   std::string::npos) << "Expected frame\n" << xml;
}

TEST_F(LuaGenFixture, MailboxHasBodyAndPost) {
    ASSERT_TRUE(reg.has("lua.object.mailbox.simple"));
    std::string err;
    auto xml = run("lua.object.mailbox.simple", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("<cylinder"), std::string::npos) << "Expected cylinder post\n" << xml;
    EXPECT_NE(xml.find("id=\"body\""), std::string::npos) << "Expected box body\n" << xml;
}

TEST_F(LuaGenFixture, FireHydrantHasBodyCapAndNozzles) {
    ASSERT_TRUE(reg.has("lua.object.fire_hydrant.simple"));
    std::string err;
    auto xml = run("lua.object.fire_hydrant.simple", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("id=\"body\""), std::string::npos) << "Expected cylinder body\n" << xml;
    EXPECT_NE(xml.find("id=\"cap\""),  std::string::npos) << "Expected box cap\n" << xml;
    EXPECT_NE(xml.find("nozzle_l"),    std::string::npos) << "Expected 2 nozzles\n" << xml;
    EXPECT_NE(xml.find("nozzle_r"),    std::string::npos) << "Expected 2 nozzles\n" << xml;
}

TEST_F(LuaGenFixture, SignHasPanelAndPost) {
    ASSERT_TRUE(reg.has("lua.object.sign.simple"));
    std::string err;
    auto xml = run("lua.object.sign.simple", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("<cylinder"), std::string::npos) << "Expected cylinder post\n" << xml;
    EXPECT_NE(xml.find("id=\"panel\""), std::string::npos) << "Expected box panel\n" << xml;
}

TEST_F(LuaGenFixture, PicnicTableHasTableAndTwoAttachedBenches) {
    ASSERT_TRUE(reg.has("lua.object.picnic_table.simple"));
    std::string err;
    auto xml = run("lua.object.picnic_table.simple", &err);
    EXPECT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_NE(xml.find("id=\"top\""), std::string::npos) << "Expected tabletop\n" << xml;
    EXPECT_NE(xml.find("bench_l"),    std::string::npos) << "Expected 2 attached benches\n" << xml;
    EXPECT_NE(xml.find("bench_r"),    std::string::npos) << "Expected 2 attached benches\n" << xml;
}

// ── T154-T157: Sandbox security ──────────────────────────────────────────────

TEST(LuaGeneratorTests, SandboxBlocksIo) {
    MeshWorld::LuaSandbox sandbox;
    constexpr const char* src = R"lua(
local M = {}
function M.generate(ctx, scene)
    local f = io.open("/etc/passwd", "r")
end
return M
)lua";
    std::string err;
    auto xml = sandbox.execute(src, make_ctx(), &err);
    EXPECT_TRUE(xml.empty())  << "io.open should be blocked";
    EXPECT_FALSE(err.empty()) << "Expected error for io access";
}

TEST(LuaGeneratorTests, SandboxBlocksOs) {
    MeshWorld::LuaSandbox sandbox;
    constexpr const char* src = R"lua(
local M = {}
function M.generate(ctx, scene)
    os.execute("id")
end
return M
)lua";
    std::string err;
    auto xml = sandbox.execute(src, make_ctx(), &err);
    EXPECT_TRUE(xml.empty())  << "os.execute should be blocked";
    EXPECT_FALSE(err.empty()) << "Expected error for os access";
}

TEST(LuaGeneratorTests, SandboxBlocksRequire) {
    MeshWorld::LuaSandbox sandbox;
    constexpr const char* src = R"lua(
local M = {}
function M.generate(ctx, scene)
    local s = require("socket")
end
return M
)lua";
    std::string err;
    auto xml = sandbox.execute(src, make_ctx(), &err);
    EXPECT_TRUE(xml.empty())  << "require should be blocked";
    EXPECT_FALSE(err.empty()) << "Expected error for require";
}

TEST(LuaGeneratorTests, SyntaxErrorNoCrash) {
    MeshWorld::LuaSandbox sandbox;
    constexpr const char* src = "this is definitely !!!! not lua @@@@";
    std::string err;
    auto xml = sandbox.execute(src, make_ctx(), &err);
    EXPECT_TRUE(xml.empty())  << "Syntax error should produce empty XML";
    EXPECT_FALSE(err.empty()) << "Expected error string for syntax error";
}
