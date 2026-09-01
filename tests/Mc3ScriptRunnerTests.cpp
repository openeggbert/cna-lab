// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// R103/R104 v1 -- Mc3ScriptRunner: MeshWorld's own sol2-based execution
// engine for a definition's embedded script (Mc3Object::scriptId ->
// Mc3Document::scripts, mesh-craft/mc3), placing imported (R101/R102)
// definitions at the definition's own Mc3AssetMetadata.sockets.

#include <gtest/gtest.h>

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>

#include <array>

#include "Mc3ScriptRunner.hpp"

using namespace MeshCraft::Mc3;
using namespace MeshWorld;

namespace {

// A minimal target with one socket and one importable definition already
// present in doc.definitions (as if resolveAndMergeInto() had already
// run) -- exactly the precondition Mc3ScriptRunner::run() documents.
struct Fixture {
    Mc3Document doc;
    std::shared_ptr<Mc3Object> target;

    Fixture() {
        auto imported = Mc3Object::makeBox("window", {1.0f, 1.0f, 0.1f}, "glass_clear");
        doc.definitions["lib:window.simple"] = imported;

        target = std::make_shared<Mc3Object>();
        target->type = ObjectType::Group;
        target->id   = "house";
        Mc3AssetMetadata meta;
        meta.category = "house";
        meta.sockets["window_front"] = {1.0f, 2.0f, 3.0f};
        target->assetMetadata = meta;
    }

    void set_script(const std::string& source) {
        Mc3Script sc;
        sc.id     = "facade";
        sc.type   = "lua";
        sc.source = source;
        doc.addScript(sc);
        target->scriptId = "facade";
    }
};

} // namespace

TEST(Mc3ScriptRunnerTest, EmptyScriptIdIsNoOpSuccess) {
    Fixture f;
    // target.scriptId left empty (default).
    EXPECT_EQ(Mc3ScriptRunner{}.run(*f.target, f.doc), "");
    EXPECT_TRUE(f.target->children.empty());
}

TEST(Mc3ScriptRunnerTest, UnknownScriptIdIsAnError) {
    Fixture f;
    f.target->scriptId = "does_not_exist";
    const auto err = Mc3ScriptRunner{}.run(*f.target, f.doc);
    EXPECT_NE(err.find("unknown script id"), std::string::npos) << err;
}

TEST(Mc3ScriptRunnerTest, EmptyScriptSourceIsNoOpSuccess) {
    Fixture f;
    f.set_script("");
    EXPECT_EQ(Mc3ScriptRunner{}.run(*f.target, f.doc), "");
    EXPECT_TRUE(f.target->children.empty());
}

TEST(Mc3ScriptRunnerTest, PlacesInstanceAtSocketPosition) {
    Fixture f;
    f.set_script(R"(def:place("w", "lib:window.simple", "window_front"))");

    const auto err = Mc3ScriptRunner{}.run(*f.target, f.doc);
    EXPECT_EQ(err, "");
    ASSERT_EQ(f.target->children.size(), 1u);

    const auto& child = f.target->children[0];
    EXPECT_EQ(child->type, ObjectType::Instance);
    EXPECT_EQ(child->id, "w");
    EXPECT_EQ(child->definition, "lib:window.simple");
    EXPECT_FLOAT_EQ(child->transform.position[0], 1.0f);
    EXPECT_FLOAT_EQ(child->transform.position[1], 2.0f);
    EXPECT_FLOAT_EQ(child->transform.position[2], 3.0f);
}

TEST(Mc3ScriptRunnerTest, MultiplePlacementsAppendMultipleChildren) {
    Fixture f;
    f.doc.definitions["lib:door.simple"] = Mc3Object::makeBox("door", {1.f, 2.f, 0.1f}, "wood_door_panel");
    f.target->assetMetadata->sockets["door_front"] = {0.f, 0.f, 0.f};
    f.set_script(
        "def:place(\"w\", \"lib:window.simple\", \"window_front\")\n"
        "def:place(\"d\", \"lib:door.simple\", \"door_front\")\n");

    EXPECT_EQ(Mc3ScriptRunner{}.run(*f.target, f.doc), "");
    ASSERT_EQ(f.target->children.size(), 2u);
    EXPECT_EQ(f.target->children[0]->id, "w");
    EXPECT_EQ(f.target->children[1]->id, "d");
}

// R112 facade_module consumption -- place_at() takes raw coordinates
// instead of a pre-authored named socket, so a script can compute
// positions itself (tiling N modules along a wall span).

TEST(Mc3ScriptRunnerTest, PlaceAtUsesRawCoordinatesNotASocket) {
    Fixture f;
    f.set_script(R"(def:place_at("w", "lib:window.simple", 4.5, 0.0, -2.5))");

    const auto err = Mc3ScriptRunner{}.run(*f.target, f.doc);
    EXPECT_EQ(err, "");
    ASSERT_EQ(f.target->children.size(), 1u);

    const auto& child = f.target->children[0];
    EXPECT_EQ(child->type, ObjectType::Instance);
    EXPECT_EQ(child->definition, "lib:window.simple");
    EXPECT_FLOAT_EQ(child->transform.position[0], 4.5f);
    EXPECT_FLOAT_EQ(child->transform.position[1], 0.0f);
    EXPECT_FLOAT_EQ(child->transform.position[2], -2.5f);
}

TEST(Mc3ScriptRunnerTest, PlaceAtWorksEvenWithNoAssetMetadataAtAll) {
    // Unlike place() (which needs target.assetMetadata for the socket
    // lookup), place_at() needs no metadata on the target at all -- a
    // real, deliberate difference, not an oversight.
    Fixture f;
    f.target->assetMetadata.reset();
    f.set_script(R"(def:place_at("w", "lib:window.simple", 1.0, 2.0, 3.0))");

    EXPECT_EQ(Mc3ScriptRunner{}.run(*f.target, f.doc), "");
    EXPECT_EQ(f.target->children.size(), 1u);
}

TEST(Mc3ScriptRunnerTest, PlaceAtLoopComputesTiledPositions) {
    // The actual motivating use case: tile N modules along a wall span
    // computed entirely in Lua, not pre-authored per-position sockets.
    Fixture f;
    f.doc.definitions["lib:bay.plain"] = Mc3Object::makeBox("bay", {2.5f, 3.2f, 0.3f}, "plaster_white");
    f.set_script(
        "local bay_w = 2.5\n"
        "local count = 4\n"
        "local half_total = (bay_w * count) / 2.0\n"
        "for i = 0, count - 1 do\n"
        "  local x = -half_total + bay_w * (i + 0.5)\n"
        "  def:place_at(\"bay_\" .. i, \"lib:bay.plain\", x, 0.0, 4.0)\n"
        "end\n");

    EXPECT_EQ(Mc3ScriptRunner{}.run(*f.target, f.doc), "");
    ASSERT_EQ(f.target->children.size(), 4u);
    // bay_w=2.5, count=4 -> half_total=5.0 -> positions -3.75,-1.25,1.25,3.75
    const std::array<float, 4> expected_x{-3.75f, -1.25f, 1.25f, 3.75f};
    for (std::size_t i = 0; i < 4; ++i) {
        EXPECT_FLOAT_EQ(f.target->children[i]->transform.position[0], expected_x[i])
            << "bay index " << i;
        EXPECT_FLOAT_EQ(f.target->children[i]->transform.position[2], 4.0f);
    }
}

TEST(Mc3ScriptRunnerTest, PlaceAtUnknownDefinitionRefIsAnError) {
    Fixture f;
    f.set_script(R"(def:place_at("w", "lib:does.not.exist", 0.0, 0.0, 0.0))");
    const auto err = Mc3ScriptRunner{}.run(*f.target, f.doc);
    EXPECT_NE(err.find("unknown definition"), std::string::npos) << err;
    EXPECT_TRUE(f.target->children.empty());
}

TEST(Mc3ScriptRunnerTest, UnknownSocketIsAnError) {
    Fixture f;
    f.set_script(R"(def:place("w", "lib:window.simple", "no_such_socket"))");
    const auto err = Mc3ScriptRunner{}.run(*f.target, f.doc);
    EXPECT_NE(err.find("unknown socket"), std::string::npos) << err;
    EXPECT_TRUE(f.target->children.empty());
}

TEST(Mc3ScriptRunnerTest, UnknownDefinitionRefIsAnError) {
    Fixture f;
    f.set_script(R"(def:place("w", "lib:does.not.exist", "window_front"))");
    const auto err = Mc3ScriptRunner{}.run(*f.target, f.doc);
    EXPECT_NE(err.find("unknown definition"), std::string::npos) << err;
    EXPECT_TRUE(f.target->children.empty());
}

TEST(Mc3ScriptRunnerTest, TargetWithNoAssetMetadataIsAnError) {
    Fixture f;
    f.target->assetMetadata.reset();
    f.set_script(R"(def:place("w", "lib:window.simple", "window_front"))");
    const auto err = Mc3ScriptRunner{}.run(*f.target, f.doc);
    EXPECT_NE(err.find("no assetMetadata"), std::string::npos) << err;
}

TEST(Mc3ScriptRunnerTest, LuaSyntaxErrorIsAnError) {
    Fixture f;
    f.set_script("this is not valid lua (((");
    const auto err = Mc3ScriptRunner{}.run(*f.target, f.doc);
    EXPECT_NE(err.find("Lua"), std::string::npos) << err;
}

TEST(Mc3ScriptRunnerTest, HasSocketReflectsRealAvailability) {
    Fixture f;
    f.set_script(
        "assert(def:has_socket(\"window_front\") == true)\n"
        "assert(def:has_socket(\"nope\") == false)\n"
        "def:place(\"w\", \"lib:window.simple\", \"window_front\")\n");
    EXPECT_EQ(Mc3ScriptRunner{}.run(*f.target, f.doc), "");
    EXPECT_EQ(f.target->children.size(), 1u);
}

TEST(Mc3ScriptRunnerTest, SandboxBlocksIoAndOs) {
    Fixture f;
    f.set_script("io.open(\"/etc/passwd\")");
    const auto err = Mc3ScriptRunner{}.run(*f.target, f.doc);
    EXPECT_NE(err, "");
}
