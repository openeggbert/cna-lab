// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include <gtest/gtest.h>
#include "ObjectDefinitionLibrary.hpp"
#include "BuiltinMaterials.hpp"
#include "Mc3DependencyPruner.hpp"
#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>

using namespace MeshWorld;
using namespace MeshCraft::Mc3;

class ObjectDefinitionLibraryTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        register_builtin_materials();
        ObjectDefinitionLibrary::instance().load_all();
    }
};

TEST_F(ObjectDefinitionLibraryTest, HasAllTreeSpecies) {
    auto& lib = ObjectDefinitionLibrary::instance();
    EXPECT_TRUE(lib.has("tree_oak"));
    EXPECT_TRUE(lib.has("tree_lime"));
    EXPECT_TRUE(lib.has("tree_birch"));
    EXPECT_TRUE(lib.has("tree_chestnut"));
    EXPECT_TRUE(lib.has("tree_apple"));
    EXPECT_TRUE(lib.has("tree_cherry"));
    EXPECT_TRUE(lib.has("tree_pear"));
    EXPECT_TRUE(lib.has("tree_willow"));
    EXPECT_TRUE(lib.has("tree_palm"));
    EXPECT_TRUE(lib.has("tree_pine_mountain"));
    EXPECT_TRUE(lib.has("tree_dead_gnarled"));
    EXPECT_TRUE(lib.has("tree_bare_winter"));
    // Real bug fix, 2026-07-11 (T-series backlog triage): ForestGenerator.cpp
    // has always instanced "tree_pine"/"tree_beech", but neither was ever
    // registered here -- silently invisible in every rendered forest chunk.
    EXPECT_TRUE(lib.has("tree_pine"));
    EXPECT_TRUE(lib.has("tree_beech"));
}

TEST_F(ObjectDefinitionLibraryTest, HasBenchVariants) {
    auto& lib = ObjectDefinitionLibrary::instance();
    EXPECT_TRUE(lib.has("bench_park"));
    EXPECT_TRUE(lib.has("bench_stone"));
    EXPECT_TRUE(lib.has("bench_street"));
}

TEST_F(ObjectDefinitionLibraryTest, HasLampPostVariants) {
    auto& lib = ObjectDefinitionLibrary::instance();
    EXPECT_TRUE(lib.has("lamp_post_ornate"));
    EXPECT_TRUE(lib.has("lamp_post_simple"));
}

TEST_F(ObjectDefinitionLibraryTest, LampPostHasPoleAndHead) {
    auto def = ObjectDefinitionLibrary::instance().get("lamp_post_ornate");
    ASSERT_NE(def, nullptr);
    EXPECT_EQ(def->type, ObjectType::Group);
    // base + pole + head = 3 children
    EXPECT_EQ(def->children.size(), 3u);
}

TEST_F(ObjectDefinitionLibraryTest, HasGroundCoverAndNature) {
    auto& lib = ObjectDefinitionLibrary::instance();
    EXPECT_TRUE(lib.has("shrub_round"));
    EXPECT_TRUE(lib.has("mushroom_brown"));
    EXPECT_TRUE(lib.has("grass_tuft_tall"));
    EXPECT_TRUE(lib.has("cactus_saguaro"));
    EXPECT_TRUE(lib.has("crystal_blue"));
    EXPECT_TRUE(lib.has("rock_grey_small"));
    EXPECT_TRUE(lib.has("rock_mossy"));
    EXPECT_TRUE(lib.has("rock_pile_small"));
    EXPECT_TRUE(lib.has("shell_seashell"));
    EXPECT_TRUE(lib.has("flower_red"));
    EXPECT_TRUE(lib.has("flower_yellow"));
    EXPECT_TRUE(lib.has("flower_daisy"));
    EXPECT_TRUE(lib.has("plant_desert_scrub"));
    EXPECT_TRUE(lib.has("plant_sea_weed"));
    EXPECT_TRUE(lib.has("plant_sea_grass"));
    EXPECT_TRUE(lib.has("plant_marsh_grass"));
    EXPECT_TRUE(lib.has("plant_lichen"));
}

TEST_F(ObjectDefinitionLibraryTest, TreeOakIsGroupWithChildren) {
    auto def = ObjectDefinitionLibrary::instance().get("tree_oak");
    ASSERT_NE(def, nullptr);
    EXPECT_EQ(def->type, ObjectType::Group);
    EXPECT_FALSE(def->children.empty());
    // Trunk + two canopy levels
    EXPECT_GE(def->children.size(), 3u);
}

TEST_F(ObjectDefinitionLibraryTest, BenchParkHasSeatAndLegs) {
    auto def = ObjectDefinitionLibrary::instance().get("bench_park");
    ASSERT_NE(def, nullptr);
    EXPECT_EQ(def->type, ObjectType::Group);
    // 2 legs + seat + backrest
    EXPECT_EQ(def->children.size(), 4u);
}

TEST_F(ObjectDefinitionLibraryTest, TrunkIsPositionedAboveGround) {
    auto def = ObjectDefinitionLibrary::instance().get("tree_oak");
    ASSERT_NE(def, nullptr);
    ASSERT_FALSE(def->children.empty());
    // First child is trunk cylinder, center y > 0
    auto& trunk = *def->children[0];
    EXPECT_EQ(trunk.type, ObjectType::Cylinder);
    EXPECT_GT(trunk.transform.position[1], 0.f);
}

TEST_F(ObjectDefinitionLibraryTest, GetReturnsNullForUnknownId) {
    auto def = ObjectDefinitionLibrary::instance().get("no_such_object_xyz");
    EXPECT_EQ(def, nullptr);
}

// R114 (city showcase) -- resolve_instance_definitions() is what makes a
// batch-exported chunk XML (MeshWorldGLB) actually contain real geometry
// for every house/vehicle/tree/street-furniture instance, instead of
// silently exporting nothing for any bare <instance ref="..."/>. Real bug
// found and fixed while wiring this up: MeshWorldGLB showed "unknown
// definition" for EVERY composer-placed instance, since the resolution
// this test suite exercises had only ever existed inline in
// WorldRenderer.cpp, private to the live renderer.

TEST_F(ObjectDefinitionLibraryTest, ResolveInstanceDefinitionsInjectsRealGeometryForBareId) {
    Mc3Document doc;
    doc.objects.push_back(Mc3Object::makeInstance("tree_0", "tree_oak"));

    ASSERT_FALSE(doc.definitions.count("tree_oak"));
    resolve_instance_definitions(doc);
    ASSERT_TRUE(doc.definitions.count("tree_oak"));
    EXPECT_NE(doc.definitions.at("tree_oak"), nullptr);
}

TEST_F(ObjectDefinitionLibraryTest, ResolveInstanceDefinitionsIsANoOpForUnknownId) {
    Mc3Document doc;
    doc.objects.push_back(Mc3Object::makeInstance("x", "definitely_not_a_real_id"));
    resolve_instance_definitions(doc);
    EXPECT_FALSE(doc.definitions.count("definitely_not_a_real_id"));
}

TEST_F(ObjectDefinitionLibraryTest, ResolveInstanceDefinitionsStripsAliasQualifierAsAFallback) {
    // Mirrors a real compiled-modular-building child instance: its own
    // `definition` field literally carries the qualified
    // "<alias>:<definitionId>" string passed to Mc3ScriptRunner's
    // def:place()/place_at() at compose time, but ObjectDefinitionLibrary
    // only ever registers the bare id (register_mc3lib_content.cpp's own
    // register_mc3lib_batch() strips the alias before registering).
    Mc3Document doc;
    doc.objects.push_back(Mc3Object::makeInstance("t", "some_alias:tree_oak"));

    resolve_instance_definitions(doc);
    ASSERT_TRUE(doc.definitions.count("some_alias:tree_oak"))
        << "must register under the ORIGINAL qualified key -- that's the "
           "exact string the real <instance ref=\"...\"/> tag carries";
    EXPECT_NE(doc.definitions.at("some_alias:tree_oak"), nullptr);
}

TEST_F(ObjectDefinitionLibraryTest, ResolveInstanceDefinitionsRecursesIntoInjectedDefinitions) {
    // A definition can itself carry nested instance children referencing
    // OTHER definitions (exactly how a compiled modular building's own
    // window/door/roof children work) -- resolving the outer id must also
    // resolve everything newly reachable through it, not just one pass.
    auto house_like = Mc3Object::makeGroup(
        "house_like", {Mc3Object::makeInstance("window", "tree_lime")});
    ObjectDefinitionLibrary::instance().register_definition("test.house_like", house_like);

    Mc3Document doc;
    doc.objects.push_back(Mc3Object::makeInstance("h", "test.house_like"));

    resolve_instance_definitions(doc);
    ASSERT_TRUE(doc.definitions.count("test.house_like"));
    EXPECT_TRUE(doc.definitions.count("tree_lime"))
        << "must recurse into the newly-injected definition's own children";
}

// R132 -- standalone compilation must retain the selected definition graph
// and its document-owned render dependencies, but never copy an
// unrelated whole mc3lib into every compiled asset.
TEST(Mc3DependencyPrunerTests, KeepsOnlyTransitiveDefinitionAndResourceDependencies) {
    Mc3Document source;
    source.library = Mc3LibraryInfo{"test-library", "1.0.0", ""};
    source.imports.push_back(Mc3Import{"vendor", "mc3lib://vendor@1.0.0", ""});

    auto root = Mc3Object::makeGroup("root", {Mc3Object::makeInstance("leaf_instance", "leaf")});
    root->assetMetadata.emplace();
    root->assetMetadata->lods["low"] = "lod";
    auto& instance = *root->children.front();
    instance.variantDefinitions.push_back("vendor:external");
    instance.materialOverride = "mat_override";
    instance.states["night"].material = "mat_state";
    source.defineObject("root", root);
    source.defineObject("leaf", Mc3Object::makeBox("leaf", {1.f, 1.f, 1.f}, "mat_leaf"));
    source.defineObject("lod", Mc3Object::makeBox("lod", {1.f, 1.f, 1.f}, "mat_lod"));
    source.defineObject("vendor:external",
                        Mc3Object::makeBox("external", {1.f, 1.f, 1.f}, "mat_external"));
    source.defineObject("unused", Mc3Object::makeBox("unused", {1.f, 1.f, 1.f}, "mat_unused"));

    for (const std::string& id : {"mat_override", "mat_state", "mat_leaf", "mat_lod",
                                  "mat_external", "mat_unused"}) {
        Mc3Material material = Mc3Material::opaque(id, {1.f, 1.f, 1.f, 1.f});
        material.baseColorTexture = "texture_" + id;
        source.addMaterial(std::move(material));
    }
    source.materials.at("mat_state").normalTexture = "svg_state";
    source.textures.emplace("texture_mat_override", Mc3Texture{"texture_mat_override", "override.png"});
    source.textures.emplace("texture_mat_state", Mc3Texture{"texture_mat_state", "state.png"});
    source.textures.emplace("texture_mat_leaf", Mc3Texture{"texture_mat_leaf", "leaf.png"});
    source.textures.emplace("texture_mat_lod", Mc3Texture{"texture_mat_lod", "lod.png"});
    source.textures.emplace("texture_mat_external", Mc3Texture{"texture_mat_external", "external.png"});
    source.textures.emplace("texture_mat_unused", Mc3Texture{"texture_mat_unused", "unused.png"});
    source.svgTextures.emplace("svg_state", Mc3SvgTexture{"svg_state", "state.svg"});

    Mc3Document pruned = prune_mc3_dependencies(source, {"root"});

    EXPECT_FALSE(pruned.library.has_value());
    EXPECT_TRUE(pruned.imports.empty());
    EXPECT_EQ(pruned.definitions.size(), 4u);
    EXPECT_TRUE(pruned.definitions.contains("root"));
    EXPECT_TRUE(pruned.definitions.contains("leaf"));
    EXPECT_TRUE(pruned.definitions.contains("lod"));
    EXPECT_TRUE(pruned.definitions.contains("vendor:external"));
    EXPECT_FALSE(pruned.definitions.contains("unused"));
    EXPECT_EQ(pruned.materials.size(), 5u);
    EXPECT_FALSE(pruned.materials.contains("mat_unused"));
    EXPECT_EQ(pruned.textures.size(), 5u);
    EXPECT_FALSE(pruned.textures.contains("texture_mat_unused"));
    EXPECT_TRUE(pruned.svgTextures.contains("svg_state"));
    EXPECT_TRUE(pruned.scripts.empty());

    pruned.definitions.at("root")->children.front()->materialOverride = "changed";
    EXPECT_EQ(source.definitions.at("root")->children.front()->materialOverride, "mat_override")
        << "the standalone result must not share mutable object nodes with its source library";
}

TEST(Mc3DependencyPrunerTests, RejectsUnresolvedStandaloneDependencies) {
    Mc3Document source;
    source.defineObject("root", Mc3Object::makeInstance("missing", "not_in_source"));
    EXPECT_THROW(prune_mc3_dependencies(source, {"root"}), std::runtime_error);

    auto scripted = Mc3Object::makeBox("scripted", {1.f, 1.f, 1.f});
    scripted->scriptId = "runtime_script";
    source.definitions.clear();
    source.defineObject("scripted", scripted);
    source.scripts.emplace("runtime_script", Mc3Script{"runtime_script", "lua", "return true"});
    EXPECT_THROW(prune_mc3_dependencies(source, {"scripted"}), std::runtime_error);
    EXPECT_THROW(prune_mc3_dependencies(source, {}), std::invalid_argument);
}
