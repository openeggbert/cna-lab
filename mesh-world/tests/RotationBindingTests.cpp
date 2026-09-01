// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// G12 -- rx/rz rotation binding (Mc3SceneBuilder/Mc3DocumentBuilder/
// MC3Writer previously only ever exposed ry/yaw; MeshCraft's own
// Mc3Transform.rotation was already a full 3-component vector, this was a
// MeshWorld-side binding gap). Covers both the low-level Lua binding itself
// and the real regression it fixes: building/simple_house.lua's gable roof
// computed a real slope angle but applied it as ry (yaw, spins flat)
// instead of rz (pitch, since its ridge runs along Z) -- documented as a
// known, deliberately-unfixed bug until this binding existed.

#include <gtest/gtest.h>

#include <array>
#include <optional>
#include <sstream>
#include <string>

#include "ChunkGenerator.hpp"
#include "LuaGeneratorRegistry.hpp"
#include "LuaSandbox.hpp"
#include "ZoneType.hpp"
#include "RegionType.hpp"

using namespace MeshWorld;

namespace {

ChunkContext make_ctx(uint64_t seed = 7) {
    ChunkContext ctx;
    ctx.seed         = seed;
    ctx.zone         = ZoneType::city;
    ctx.region       = RegionType::park;
    ctx.chunk_size_m = 64.0f;
    ctx.coord        = {0, 0};
    return ctx;
}

// Finds the start tag containing id="<id>" and, if it has a rotation="rx ry
// rz" attribute, returns the 3 parsed components. Returns nullopt if the
// element has no rotation attribute at all (Mc3XmlWriter omits it entirely
// when all 3 components are zero -- nonzero3() gate in Mc3XmlWriter.cpp).
std::optional<std::array<float, 3>> find_rotation(const std::string& xml, const std::string& id) {
    const std::string needle = "id=\"" + id + "\"";
    const auto id_pos = xml.find(needle);
    if (id_pos == std::string::npos) return std::nullopt;
    const auto tag_end = xml.find('>', id_pos);
    if (tag_end == std::string::npos) return std::nullopt;
    const std::string tag = xml.substr(id_pos, tag_end - id_pos);

    const auto rot_pos = tag.find("rotation=\"");
    if (rot_pos == std::string::npos) return std::nullopt;
    const auto value_start = rot_pos + std::string("rotation=\"").size();
    const auto value_end = tag.find('"', value_start);
    const std::string value = tag.substr(value_start, value_end - value_start);

    std::istringstream iss(value);
    std::array<float, 3> out{0.0f, 0.0f, 0.0f};
    iss >> out[0] >> out[1] >> out[2];
    return out;
}

struct RotationBindingFixture : ::testing::Test {
    static void SetUpTestSuite() {
        LuaGeneratorRegistry::instance().load_from_dir("generators/lua");
    }
    LuaSandbox sandbox;
};

} // namespace

// ── Low-level binding: addBox ────────────────────────────────────────────

TEST_F(RotationBindingFixture, AddBoxAcceptsRxRyRz) {
    constexpr const char* src = R"lua(
local M = {}
function M.generate(ctx, scene)
    scene:addBox("tilted", {
        position = {5, 0, 5}, size = {2, 0.2, 3}, material = "wood_natural",
        rx = 10, ry = 20, rz = 30
    })
end
return M
)lua";
    std::string err;
    const auto xml = sandbox.execute(src, make_ctx(), &err);
    ASSERT_TRUE(err.empty()) << "Lua error: " << err;

    const auto rot = find_rotation(xml, "tilted");
    ASSERT_TRUE(rot.has_value()) << "Expected a rotation attribute:\n" << xml;
    EXPECT_FLOAT_EQ((*rot)[0], 10.0f);
    EXPECT_FLOAT_EQ((*rot)[1], 20.0f);
    EXPECT_FLOAT_EQ((*rot)[2], 30.0f);
}

// A box with no rotation fields at all still omits the attribute entirely
// (Mc3XmlWriter's own nonzero3() gate) -- rx/rz are additive, not a
// regression for the common all-zero case.
TEST_F(RotationBindingFixture, AddBoxWithNoRotationOmitsAttribute) {
    constexpr const char* src = R"lua(
local M = {}
function M.generate(ctx, scene)
    scene:addBox("flat", {position={0,0,0}, size={1,1,1}, material="wood_natural"})
end
return M
)lua";
    std::string err;
    const auto xml = sandbox.execute(src, make_ctx(), &err);
    ASSERT_TRUE(err.empty()) << "Lua error: " << err;
    EXPECT_FALSE(find_rotation(xml, "flat").has_value()) << xml;
}

// ── Low-level binding: addPlane, addCylinder, addInstance ────────────────

TEST_F(RotationBindingFixture, AddPlaneAcceptsRxRz) {
    constexpr const char* src = R"lua(
local M = {}
function M.generate(ctx, scene)
    scene:addPlane("panel", {
        position = {0, 0, 0}, size = {4, 4}, material = "roof_tile_red",
        rx = 15, rz = -15
    })
end
return M
)lua";
    std::string err;
    const auto xml = sandbox.execute(src, make_ctx(), &err);
    ASSERT_TRUE(err.empty()) << "Lua error: " << err;

    const auto rot = find_rotation(xml, "panel");
    ASSERT_TRUE(rot.has_value()) << xml;
    EXPECT_FLOAT_EQ((*rot)[0], 15.0f);
    EXPECT_FLOAT_EQ((*rot)[1], 0.0f);
    EXPECT_FLOAT_EQ((*rot)[2], -15.0f);
}

// A cylinder tipped onto its side (a real wheel rolling around a horizontal
// axle) -- previously addCylinder had no rotation parameter at all.
TEST_F(RotationBindingFixture, AddCylinderAcceptsRxForASidewaysWheel) {
    constexpr const char* src = R"lua(
local M = {}
function M.generate(ctx, scene)
    scene:addCylinder("wheel", {
        position = {0, 0.3, 0}, radius = 0.3, height = 0.15, material = "rubber_tire",
        rx = 90
    })
end
return M
)lua";
    std::string err;
    const auto xml = sandbox.execute(src, make_ctx(), &err);
    ASSERT_TRUE(err.empty()) << "Lua error: " << err;

    const auto rot = find_rotation(xml, "wheel");
    ASSERT_TRUE(rot.has_value()) << xml;
    EXPECT_FLOAT_EQ((*rot)[0], 90.0f);
}

TEST_F(RotationBindingFixture, AddInstanceAcceptsRxRz) {
    constexpr const char* src = R"lua(
local M = {}
function M.generate(ctx, scene)
    scene:addInstance("prop", {
        definition = "bench_park", position = {2, 0, 2}, ry = 45, rx = 5, rz = -5
    })
end
return M
)lua";
    std::string err;
    const auto xml = sandbox.execute(src, make_ctx(), &err);
    ASSERT_TRUE(err.empty()) << "Lua error: " << err;

    const auto rot = find_rotation(xml, "prop");
    ASSERT_TRUE(rot.has_value()) << xml;
    EXPECT_FLOAT_EQ((*rot)[0], 5.0f);
    EXPECT_FLOAT_EQ((*rot)[1], 45.0f);
    EXPECT_FLOAT_EQ((*rot)[2], -5.0f);
}

// ── Regression: building/simple_house.lua's gable roof ───────────────────

// Before this fix, roof_l/roof_r used `ry` for a computed slope angle,
// which only spins a box flat in the horizontal plane -- the roof visually
// rendered as flat rotated planks, not a sloped gable. Now uses `rz`
// (the ridge runs along Z, so pitching the panel from flat to sloped is a
// rotation around Z). This proves the fix landed on the REAL generator
// file, not just the binding in isolation.
TEST_F(RotationBindingFixture, SimpleHouseGableRoofUsesRzNotRy) {
    std::string err;
    const auto xml = sandbox.execute(
        LuaGeneratorRegistry::instance().get("lua.building.simple_house.standard"),
        make_ctx(), &err);
    ASSERT_TRUE(err.empty()) << "Lua error: " << err;

    for (const char* id : {"roof_l", "roof_r"}) {
        const auto rot = find_rotation(xml, id);
        ASSERT_TRUE(rot.has_value()) << id << " should have a real slope rotation:\n" << xml;
        EXPECT_FLOAT_EQ((*rot)[0], 0.0f) << id << ": rx should be 0";
        EXPECT_FLOAT_EQ((*rot)[1], 0.0f) << id << ": ry should be 0 (not the old yaw bug)";
        EXPECT_NE((*rot)[2], 0.0f)       << id << ": rz should be the real, non-zero slope angle";
    }

    // roof_l and roof_r slope in opposite directions (a symmetric ridge).
    const auto rot_l = find_rotation(xml, "roof_l");
    const auto rot_r = find_rotation(xml, "roof_r");
    ASSERT_TRUE(rot_l.has_value() && rot_r.has_value());
    EXPECT_FLOAT_EQ((*rot_l)[2], -(*rot_r)[2]);
}
