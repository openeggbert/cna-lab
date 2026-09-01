// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// R113 v1 (docs/world-composer-design.md) -- AssetRegistry/StyleProfile/
// Parcel/BuildingComposer, and their wiring into ChunkPipeline via
// WorldConfig::use_world_composer.

#include <gtest/gtest.h>

#include <MeshCraft/Mc3/Mc3Object.hpp>

#include "AssetRegistry.hpp"
#include "BuildingComposer.hpp"
#include "BuiltinMaterials.hpp"
#include "ChunkCache.hpp"
#include "ChunkPipeline.hpp"
#include "ComposerAssets.hpp"
#include "ObjectDefinitionLibrary.hpp"
#include "Parcel.hpp"
#include "RegionType.hpp"
#include "StyleProfile.hpp"
#include "WorldConfig.hpp"
#include "WorldMap.hpp"
#include "ZoneType.hpp"

#include <algorithm>
#include <filesystem>

using namespace MeshWorld;

namespace {

ChunkContext make_ctx(RegionType region, uint64_t seed = 7) {
    ChunkContext ctx;
    ctx.seed         = seed;
    ctx.zone         = ZoneType::city;
    ctx.region       = region;
    ctx.chunk_size_m = 64.0f;
    ctx.coord        = {0, 0};
    // R113 v3 -- derive_parcels() is now exits-aware (real roads must
    // border a side before a row is placed there); default to the same
    // "streets to the north and south" shape v1/v2 always assumed, so
    // every pre-existing test built around that shape keeps working
    // unchanged. Tests exercising v3's own new east/west/none-adjacent
    // behavior set ctx.exits explicitly instead of using this default.
    ctx.exits.north_road = true;
    ctx.exits.south_road = true;
    return ctx;
}

// R113's own singletons (AssetRegistry, StyleProfileRegistry) are
// process-wide, same convention as MaterialRegistry/LuaGeneratorRegistry
// -- populate once here via register_composer_assets() (idempotent,
// mirrors register_builtin_materials()'s own contract), and ALSO ensure
// ObjectDefinitionLibrary is loaded first (its usual lazy-load trigger,
// ObjectBoundingBox.cpp's object_height_m(), may not have run yet in an
// isolated test binary invocation).
//
// R106 (investigated 2026-07-13) -- this file's own generated output
// (house_gable_default, simple_house-derived content, the composer's own
// "grass_garden" ground plane, etc.) references real MaterialRegistry
// ids, but this fixture never called register_builtin_materials() itself
// -- it only ever "worked" (no warnings) when some OTHER test file
// happened to run first in the same `MeshWorldTests` binary and primed
// the shared singleton first. That's exactly the class of accidental,
// run-order-dependent test coupling that produced this project's own
// long-standing "~389 spurious 'not registered' warnings for materials
// that ARE genuinely registered" observation (plan.md's R106 entry) --
// they were never spurious, this fixture's own tests were the real,
// if harmless (materials are a warning, not an error -- see MC3Validator
// R106 decision), gap. register_builtin_materials() is idempotent (a
// plain map assignment per id), so calling it here too is a safe,
// order-independent fix.
struct BuildingComposerFixture : ::testing::Test {
    static void SetUpTestSuite() {
        register_builtin_materials();
        ObjectDefinitionLibrary::instance().load_all();
        register_composer_assets();
    }
};

std::string tmp_dir(const std::string& suffix) {
    const auto dir = std::filesystem::temp_directory_path() /
                     ("meshworld_composer_test_" + suffix);
    std::filesystem::remove_all(dir);
    return dir.string();
}

} // namespace

// ── AssetRegistry ─────────────────────────────────────────────────────────

TEST_F(BuildingComposerFixture, RegistryHasHouseGableDefaultAfterPopulation) {
    const auto* entry = AssetRegistry::instance().get("house_gable_default");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->meta.category, "house");
    EXPECT_NE(entry->def, nullptr);
}

// R126 (BuildingComposer v2, apartment_block)
TEST_F(BuildingComposerFixture, RegistryHasApartmentBlockAfterPopulation) {
    const auto* entry = AssetRegistry::instance().get("apartment.block.wide_01");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->meta.category, "apartment");
    EXPECT_NE(entry->def, nullptr);
    EXPECT_FLOAT_EQ(entry->meta.nominalSize[0], 14.6f);
}

// R127 (BuildingComposer v2, shop_street)
TEST_F(BuildingComposerFixture, RegistryHasShopBuildingAfterPopulation) {
    const auto* entry = AssetRegistry::instance().get("shop.building.storefront_01");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->meta.category, "shop");
    EXPECT_NE(entry->def, nullptr);
    EXPECT_FLOAT_EQ(entry->meta.nominalSize[0], 8.6f);
}

// R128 (city showcase completion)
TEST_F(BuildingComposerFixture, RegistryHasLandmarkClocktowerAfterPopulation) {
    const auto* entry = AssetRegistry::instance().get("landmark.clocktower_01");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->meta.category, "landmark");
    EXPECT_NE(entry->def, nullptr);
}

// ── R103/R104: modular house (imports + sockets + script) ──────────────────

TEST_F(BuildingComposerFixture, ModularHouseRegisteredAsSecondHouseCandidate) {
    const auto* entry = AssetRegistry::instance().get("house.gable.modular_01");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->meta.category, "house");
    EXPECT_NE(entry->def, nullptr);
}

TEST_F(BuildingComposerFixture, ModularHouseHasRealWindowDoorRoofChildrenFromScript) {
    // register_composer_assets() (SetUpTestSuite) already ran
    // register_modular_buildings(), which loads urban-buildings, merges
    // its own imports, and runs house.gable.modular_01's script -- so by
    // the time AssetRegistry has this entry, its def should already
    // carry the placed window/door/roof instances as real children, not
    // just bare walls.
    const auto* entry = AssetRegistry::instance().get("house.gable.modular_01");
    ASSERT_NE(entry, nullptr);
    ASSERT_NE(entry->def, nullptr);

    std::vector<std::string> child_definitions;
    for (const auto& child : entry->def->children) {
        if (child && child->type == MeshCraft::Mc3::ObjectType::Instance)
            child_definitions.push_back(child->definition);
    }
    EXPECT_NE(std::find(child_definitions.begin(), child_definitions.end(),
                         "windows:window.residential.double.classic_01"),
              child_definitions.end());
    EXPECT_NE(std::find(child_definitions.begin(), child_definitions.end(),
                         "doors:door.residential.wood_panel_01"),
              child_definitions.end());
    EXPECT_NE(std::find(child_definitions.begin(), child_definitions.end(),
                         "roofs:roof.gable_clay_04"),
              child_definitions.end());
    // Two windows placed (window_front_l and window_front_r).
    EXPECT_EQ(std::count(child_definitions.begin(), child_definitions.end(),
                         "windows:window.residential.double.classic_01"),
              2);
}

// R112 facade_module consumption: house.rowhouse.modular_01 tiles 6 real
// facade bay modules along its own front wall via
// Mc3ScriptRunner::place_at() (computed positions, not fixed sockets).

TEST_F(BuildingComposerFixture, RowhouseIsARealHouseCategoryCandidateWithWideNominalSize) {
    // R113 (size-aware matching) -- now safely a real AssetRegistry
    // "house" candidate: BuildingComposer's own frontage_extent-based
    // filter means this wider (15.6m) house only ever gets picked for a
    // wide-class parcel, never one that would make it overlap a
    // standard-width neighbor.
    EXPECT_TRUE(ObjectDefinitionLibrary::instance().has("house.rowhouse.modular_01"));

    const auto* entry = AssetRegistry::instance().get("house.rowhouse.modular_01");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->meta.category, "house");
    EXPECT_NEAR(entry->meta.nominalSize[0], 15.6f, 1e-3f);

    auto house_results = AssetRegistry::instance().query("house");
    bool found = false;
    for (const auto* r : house_results)
        if (r->id == "house.rowhouse.modular_01") found = true;
    EXPECT_TRUE(found);
}

TEST_F(BuildingComposerFixture, RowhouseHasSixTiledFacadeBaysFromScript) {
    auto def = ObjectDefinitionLibrary::instance().get("house.rowhouse.modular_01");
    ASSERT_NE(def, nullptr);

    std::vector<std::pair<std::string, float>> bays; // (definition, x position)
    for (const auto& child : def->children) {
        if (child && child->type == MeshCraft::Mc3::ObjectType::Instance)
            bays.emplace_back(child->definition, child->transform.position[0]);
    }
    ASSERT_EQ(bays.size(), 6u);

    // bay_w=2.5, count=6 -> half_total=7.5 -> positions -6.25,-3.75,...,6.25
    const std::array<float, 6> expected_x{-6.25f, -3.75f, -1.25f, 1.25f, 3.75f, 6.25f};
    for (std::size_t i = 0; i < 6; ++i)
        EXPECT_NEAR(bays[i].second, expected_x[i], 1e-4f) << "bay " << i;

    // bay 0 is the door; the rest alternate window_02 (odd i)/window_01
    // (even i) -- matches the script's own `i % 2 == 0 -> window_01,
    // else -> window_02` branch order exactly (i=1,3,5 are odd -> _02).
    EXPECT_EQ(bays[0].first, "facades:facade.residential_bay_door_01");
    EXPECT_EQ(bays[1].first, "facades:facade.residential_bay_window_02");
    EXPECT_EQ(bays[2].first, "facades:facade.residential_bay_window_01");
    EXPECT_EQ(bays[3].first, "facades:facade.residential_bay_window_02");
    EXPECT_EQ(bays[4].first, "facades:facade.residential_bay_window_01");
    EXPECT_EQ(bays[5].first, "facades:facade.residential_bay_window_02");

    // All bays sit at the same Z (the front wall line) and Y (base).
    for (const auto& child : def->children) {
        if (!child || child->type != MeshCraft::Mc3::ObjectType::Instance) continue;
        EXPECT_NEAR(child->transform.position[1], 0.0f, 1e-4f);
        EXPECT_NEAR(child->transform.position[2], 4.151f, 1e-3f);
    }
}

// R113 size-aware matching follow-up: house.gable.wide_01, a SECOND wide
// (15.6m) house, this one with a real gable roof (unlike
// house.rowhouse.modular_01's genuine flat roof), so wide-class parcels
// have a style-profile-compatible candidate too.

TEST_F(BuildingComposerFixture, WideGableHouseIsARealGableRoofTaggedHouseCandidate) {
    const auto* entry = AssetRegistry::instance().get("house.gable.wide_01");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->meta.category, "house");
    EXPECT_NEAR(entry->meta.nominalSize[0], 15.6f, 1e-3f);
    EXPECT_NE(std::find(entry->meta.styleTags.begin(), entry->meta.styleTags.end(), "gable_roof"),
              entry->meta.styleTags.end());

    // Unlike the rowhouse, this one DOES match the strict
    // central_europe_default profile's own required tags.
    auto results = AssetRegistry::instance().query("house", {"central_europe", "gable_roof"});
    bool found = false;
    for (const auto* r : results)
        if (r->id == "house.gable.wide_01") found = true;
    EXPECT_TRUE(found);
}

TEST_F(BuildingComposerFixture, WideGableHouseHasRealWindowDoorRoofChildrenFromScript) {
    const auto* entry = AssetRegistry::instance().get("house.gable.wide_01");
    ASSERT_NE(entry, nullptr);
    ASSERT_NE(entry->def, nullptr);

    std::vector<std::string> child_definitions;
    for (const auto& child : entry->def->children) {
        if (child && child->type == MeshCraft::Mc3::ObjectType::Instance)
            child_definitions.push_back(child->definition);
    }
    EXPECT_EQ(std::count(child_definitions.begin(), child_definitions.end(),
                         "windows:window.residential.double.classic_01"),
              2);
    EXPECT_EQ(std::count(child_definitions.begin(), child_definitions.end(),
                         "windows:window.residential.double.classic_02"),
              2);
    EXPECT_NE(std::find(child_definitions.begin(), child_definitions.end(),
                         "doors:door.residential.wood_panel_02"),
              child_definitions.end());
    EXPECT_NE(std::find(child_definitions.begin(), child_definitions.end(),
                         "roofs:roof.gable_clay_wide_01"),
              child_definitions.end());
}

TEST_F(BuildingComposerFixture, QueryFindsHouseByCategoryAndStyleTags) {
    // R103/R104 v1 added a second house candidate
    // (house.gable.modular_01, same styleTags) alongside
    // house_gable_default -- both must match this filter now; check by
    // membership, not a specific index/count, so a future third house
    // candidate doesn't break this test again.
    auto results = AssetRegistry::instance().query("house", {"central_europe", "gable_roof"});
    ASSERT_GE(results.size(), 1u);
    std::vector<std::string> ids;
    for (const auto* r : results) ids.push_back(r->id);
    EXPECT_NE(std::find(ids.begin(), ids.end(), "house_gable_default"), ids.end());
}

TEST_F(BuildingComposerFixture, QueryReturnsEmptyForMismatchedStyleTag) {
    auto results = AssetRegistry::instance().query("house", {"definitely_not_a_real_style_tag"});
    EXPECT_TRUE(results.empty());
}

TEST_F(BuildingComposerFixture, QueryReturnsEmptyForUnknownCategory) {
    auto results = AssetRegistry::instance().query("spaceship");
    EXPECT_TRUE(results.empty());
}

// ── R112: real mc3lib content batch (data/mc3lib/*.mc3lib.json), loaded
// through the actual Mc3ImportResolver mechanism by
// register_mc3lib_batch() in ComposerAssets.cpp -- NOT hand-registered
// C++ like house_gable_default. If any of the 7 library files fails to
// parse/resolve, register_mc3lib_batch() skips it with a stderr warning
// rather than crashing, so these category-count checks are the real
// regression guard for "the content batch still loads correctly".
//
// Counts below reflect the DEEPER variant coverage pass (up from 2 per
// category): window 5 (classic_01/02, modern_01, single.classic_01,
// shopfront.large.urban_01), door 4 (wood_panel_01/02, wood_glass_01,
// apartment.shared_entry_01), street_furniture 4 (streetlamp
// classic_01/02, modern_01, bench.classic_01), prop 4 (mailbox
// classic_01/02, modern_01, trash_bin.classic_01), roof 4 (gable_clay
// 04/05, flat_modern_01/02), facade_module 4 (bay_window_01/02,
// bay_door_01, bay_plain_01), vehicle 4 (hatchback compact_01/02,
// sedan.family_01, van.delivery_01).

TEST_F(BuildingComposerFixture, WindowsLibraryResolvesAllVariants) {
    auto results = AssetRegistry::instance().query("window");
    EXPECT_EQ(results.size(), 5u);
    for (const auto* r : results) {
        EXPECT_EQ(r->meta.category, "window");
        EXPECT_NE(r->def, nullptr);
    }
}

TEST_F(BuildingComposerFixture, DoorsLibraryResolvesAllVariants) {
    auto results = AssetRegistry::instance().query("door");
    EXPECT_EQ(results.size(), 4u);
}

TEST_F(BuildingComposerFixture, StreetFurnitureLibraryResolvesAllVariants) {
    auto results = AssetRegistry::instance().query("street_furniture");
    EXPECT_EQ(results.size(), 4u);
}

TEST_F(BuildingComposerFixture, PropsLibraryResolvesAllVariants) {
    auto results = AssetRegistry::instance().query("prop");
    EXPECT_EQ(results.size(), 4u);
}

TEST_F(BuildingComposerFixture, NewSubcategoriesFromDeeperCoverageExist) {
    // Deeper coverage added genuinely new subcategories, not just
    // recolors of existing ones -- prove each one actually resolved
    // (present in AssetRegistry with the right category/subcategory),
    // not just that the category's total count went up.
    auto has = [](const std::string& id) {
        return AssetRegistry::instance().get(id) != nullptr;
    };
    EXPECT_TRUE(has("window.residential.single.classic_01"));
    EXPECT_TRUE(has("window.shopfront.large.urban_01"));
    EXPECT_TRUE(has("door.apartment.shared_entry_01"));
    EXPECT_TRUE(has("bench.classic_01"));
    EXPECT_TRUE(has("prop.trash_bin.classic_01"));
    EXPECT_TRUE(has("car.van.delivery_01"));

    const auto* bench = AssetRegistry::instance().get("bench.classic_01");
    ASSERT_NE(bench, nullptr);
    EXPECT_EQ(bench->meta.category, "street_furniture");
    EXPECT_EQ(bench->meta.subcategory, "bench");

    const auto* van = AssetRegistry::instance().get("car.van.delivery_01");
    ASSERT_NE(van, nullptr);
    EXPECT_EQ(van->meta.category, "vehicle");
    EXPECT_EQ(van->meta.subcategory, "van");
}

TEST_F(BuildingComposerFixture, RoofsLibraryResolvesAllVariants) {
    // R113 size-aware matching follow-up added a 5th roof variant
    // (roof.gable_clay_wide_01, sized for house.gable.wide_01).
    auto results = AssetRegistry::instance().query("roof");
    EXPECT_EQ(results.size(), 5u);
}

TEST_F(BuildingComposerFixture, FacadeModulesLibraryResolvesAllVariants) {
    auto results = AssetRegistry::instance().query("facade_module");
    EXPECT_EQ(results.size(), 4u);
}

TEST_F(BuildingComposerFixture, VehiclesLibraryResolvesAllVariants) {
    auto results = AssetRegistry::instance().query("vehicle");
    EXPECT_EQ(results.size(), 4u);
}

TEST_F(BuildingComposerFixture, GableRoofStyleTagMatchesAllThreeGableVariants) {
    // gable_clay_04/_05 (same-family color variant, per the "_02" style
    // precedent) and gable_clay_wide_01 (R113 size-aware matching
    // follow-up, sized for house.gable.wide_01) all carry
    // {"central_europe","gable_roof"} -- deliberately the SAME
    // roofFamily tag central_europe_default's own StyleProfile already
    // requires of house_gable_default, so all three are plausible (if
    // not yet wired) drop-ins for that profile.
    auto results = AssetRegistry::instance().query("roof", {"gable_roof"});
    ASSERT_EQ(results.size(), 3u);
    std::vector<std::string> ids;
    for (const auto* r : results) ids.push_back(r->id);
    EXPECT_NE(std::find(ids.begin(), ids.end(), "roof.gable_clay_04"), ids.end());
    EXPECT_NE(std::find(ids.begin(), ids.end(), "roof.gable_clay_05"), ids.end());
    EXPECT_NE(std::find(ids.begin(), ids.end(), "roof.gable_clay_wide_01"), ids.end());
}

TEST_F(BuildingComposerFixture, StreetFurnitureModernTagFilterSelectsExactlyOneVariant) {
    // Only streetlamp.modern_01 carries {"modern"} -- classic_01/02 and
    // bench.classic_01 all carry {"central_europe","classic"} instead --
    // proving style-coherent filtering still isolates a single variant
    // even as the category's candidate pool grows.
    auto results = AssetRegistry::instance().query("street_furniture", {"modern"});
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0]->id, "streetlamp.modern_01");
}

TEST_F(BuildingComposerFixture, Mc3lLibLowLodProxiesAreInObjectDefinitionLibraryButNotAssetRegistry) {
    // Low-LOD proxies carry no assetMetadata of their own (by design --
    // they're reachable only via their parent's assetMetadata.lods map),
    // so they must never appear as independently queryable top-level
    // assets, even though they ARE real, instantiable definitions.
    EXPECT_TRUE(ObjectDefinitionLibrary::instance().has("streetlamp.classic_01.lod_low"));
    EXPECT_EQ(AssetRegistry::instance().get("streetlamp.classic_01.lod_low"), nullptr);
}

TEST_F(BuildingComposerFixture, StreetFurnitureAssetLodsMetadataReferencesRealDefinition) {
    // Find streetlamp.classic_01 specifically by id (not by index into a
    // filtered query) -- robust regardless of how many other entries
    // share its style tags.
    const auto results = AssetRegistry::instance().query("street_furniture");
    const auto it_entry = std::find_if(results.begin(), results.end(),
        [](const AssetEntry* e) { return e->id == "streetlamp.classic_01"; });
    ASSERT_NE(it_entry, results.end());
    const auto it_lod = (*it_entry)->meta.lods.find("low");
    ASSERT_NE(it_lod, (*it_entry)->meta.lods.end());
    EXPECT_TRUE(ObjectDefinitionLibrary::instance().has(it_lod->second));
}

namespace {
// Robust against a growing candidate pool: true if ANY of the given
// definition ids appears in the composed XML, rather than pinning one
// specific id a deterministic hash roll might not have picked for this
// seed once more candidates exist to choose from.
bool any_definition_present(const std::string& xml, const std::vector<std::string>& ids) {
    for (const auto& id : ids)
        if (xml.find("definition=\"" + id + "\"") != std::string::npos) return true;
    return false;
}
} // namespace

TEST_F(BuildingComposerFixture, ComposedChunkPlacesStreetlampAndMailboxAlongsideHouses) {
    auto composed = BuildingComposer{}.compose_chunk(make_ctx(RegionType::small_house_block));
    ASSERT_TRUE(composed.has_value());
    EXPECT_TRUE(any_definition_present(*composed, {
        "streetlamp.classic_01", "streetlamp.classic_02", "streetlamp.modern_01", "bench.classic_01"
    })) << *composed;
    EXPECT_TRUE(any_definition_present(*composed, {
        "prop.mailbox.classic_01", "prop.mailbox.classic_02", "prop.mailbox.modern_01", "prop.trash_bin.classic_01"
    })) << *composed;

    // One of each per parcel -- R113 v2's parcel count is a real function
    // of chunk_size_m/seed (see ParcelTest), not a fixed literal, so
    // compare against derive_parcels()'s own actual count rather than
    // hardcoding one.
    const auto expected_parcels = derive_parcels(make_ctx(RegionType::small_house_block)).size();
    auto count_of = [&](const std::string& needle) {
        std::size_t n = 0, pos = 0;
        while ((pos = composed->find(needle, pos)) != std::string::npos) { ++n; pos += 1; }
        return n;
    };
    EXPECT_EQ(count_of("id=\"streetlamp_"), expected_parcels);
    EXPECT_EQ(count_of("id=\"mailbox_"), expected_parcels);
}

TEST_F(BuildingComposerFixture, ComposedChunkPlacesOneVehiclePerParcel) {
    auto composed = BuildingComposer{}.compose_chunk(make_ctx(RegionType::small_house_block));
    ASSERT_TRUE(composed.has_value());

    const auto expected_parcels = derive_parcels(make_ctx(RegionType::small_house_block)).size();
    std::size_t n = 0, pos = 0;
    while ((pos = composed->find("id=\"vehicle_", pos)) != std::string::npos) { ++n; pos += 1; }
    EXPECT_EQ(n, expected_parcels);

    // All 4 variants are real candidates (no style-tag filter for
    // vehicles) -- at least one must actually appear across 4
    // placements, though which one(s) is a deterministic function of
    // ctx.seed, not asserted exactly here.
    EXPECT_TRUE(any_definition_present(*composed, {
        "car.hatchback.compact_01", "car.hatchback.compact_02",
        "car.sedan.family_01", "car.van.delivery_01"
    })) << *composed;
}

TEST_F(BuildingComposerFixture, ComposedChunkPlacesOneYardTreePerParcel) {
    // R114 (city showcase) -- real tree coverage for composer-driven
    // residential blocks (previously composer output had no trees at
    // all, unlike the legacy C++ chain's own per-house garden dressing).
    auto composed = BuildingComposer{}.compose_chunk(make_ctx(RegionType::small_house_block));
    ASSERT_TRUE(composed.has_value());

    const auto expected_parcels = derive_parcels(make_ctx(RegionType::small_house_block)).size();
    std::size_t n = 0, pos = 0;
    while ((pos = composed->find("id=\"tree_", pos)) != std::string::npos) { ++n; pos += 1; }
    EXPECT_EQ(n, expected_parcels);

    EXPECT_TRUE(any_definition_present(*composed, {
        "tree_oak", "tree_lime", "tree_birch", "tree_apple"
    })) << *composed;
}

TEST(BuildingComposerUnitTest, MissingStreetFurnitureOrPropDoesNotBlockHousePlacement) {
    // A registry with ONLY a house asset (no street_furniture/prop
    // categories at all) must still compose the houses -- street
    // furniture/props are a per-item addition, not a whole-chunk
    // fallback trigger.
    AssetRegistry::instance().clear_for_tests();
    ObjectDefinitionLibrary::instance().load_all();
    if (auto def = ObjectDefinitionLibrary::instance().get("house_gable_default");
        def && def->assetMetadata.has_value()) {
        AssetRegistry::instance().register_asset(AssetEntry{"house_gable_default", def, *def->assetMetadata});
    }

    // R113 (size-aware matching) -- this test's registry deliberately has
    // only a standard-width house, so it needs a ctx whose rows are
    // actually standard-width themselves (derive_parcels()'s own per-row
    // width roll depends on both axis and seed) -- the shared default
    // seed (7, see make_ctx()) rolls BOTH its north and south rows wide,
    // which would leave every parcel here with no size-matching candidate
    // at all and isn't what this test means to exercise. Seed 0 was
    // checked to roll both rows standard-width, so this test stays about
    // its own actual point (missing categories don't block placement),
    // not an incidental width roll.
    auto composed = BuildingComposer{}.compose_chunk(make_ctx(RegionType::small_house_block, /*seed=*/0));
    ASSERT_TRUE(composed.has_value());
    EXPECT_NE(composed->find("definition=\"house_gable_default\""), std::string::npos);
    EXPECT_EQ(composed->find("streetlamp"), std::string::npos);
    EXPECT_EQ(composed->find("mailbox"), std::string::npos);

    register_composer_assets();
}

TEST(AssetRegistryUnitTest, ClearForTestsActuallyClears) {
    AssetRegistry reg_check; // local instance, not the singleton -- just
                              // exercises AssetEntry/register_asset/get
                              // directly, no shared-state concern.
    MeshCraft::Mc3::Mc3AssetMetadata meta;
    meta.category = "test_category";
    reg_check.register_asset(AssetEntry{"x", MeshCraft::Mc3::Mc3Object::makeGroup("x", {}), meta});
    ASSERT_NE(reg_check.get("x"), nullptr);
    reg_check.clear_for_tests();
    EXPECT_EQ(reg_check.get("x"), nullptr);
}

// ── StyleProfile ──────────────────────────────────────────────────────────

TEST_F(BuildingComposerFixture, DefaultStyleProfileIsRegistered) {
    const auto* p = StyleProfileRegistry::instance().get("central_europe_default");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->facadeFamily, "central_europe");
    EXPECT_EQ(p->roofFamily, "gable_roof");
}

TEST_F(BuildingComposerFixture, PickForIsDeterministicForSameSeed) {
    const auto* a = StyleProfileRegistry::instance().pick_for(12345);
    const auto* b = StyleProfileRegistry::instance().pick_for(12345);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(a->id, b->id);
}

TEST(StyleProfileUnitTest, PickForReturnsNullptrWhenEmpty) {
    StyleProfileRegistry reg;
    EXPECT_EQ(reg.pick_for(1), nullptr);
}

// ── Parcel ────────────────────────────────────────────────────────────────

TEST(ParcelTest, DerivesParcelsOnlyForSmallHouseBlock) {
    // R113 v2's parcel count is a real function of chunk_size_m/seed
    // (see ParcelCountIsDeterministicForSameSeed/
    // ParcelCountScalesWithChunkSize below), not a fixed literal -- this
    // just proves parcels exist and all carry the right kind.
    const auto parcels = derive_parcels(make_ctx(RegionType::small_house_block));
    ASSERT_GE(parcels.size(), 2u);
    for (const auto& p : parcels)
        EXPECT_EQ(p.kind, RegionType::small_house_block);
}

TEST(ParcelTest, DerivesExactlySixParcelsForDefaultTestSeedAndChunkSize) {
    // Pins the ACTUAL deterministic output for this test suite's own
    // default make_ctx() (seed=7, chunk_size_m=64) -- a real regression
    // guard for the algorithm itself, not just "some parcels exist".
    // (Size-aware matching's own per-row width-class roll changed this
    // from v2/v3's own pinned value of 10 to 6 for this specific seed --
    // at least one row now rolls "wide", which packs fewer, bigger
    // parcels into the same row -- expected, not a regression.)
    const auto parcels = derive_parcels(make_ctx(RegionType::small_house_block));
    EXPECT_EQ(parcels.size(), 6u);
}

TEST(ParcelTest, ParcelCountIsDeterministicForSameSeed) {
    const auto a = derive_parcels(make_ctx(RegionType::small_house_block, 123));
    const auto b = derive_parcels(make_ctx(RegionType::small_house_block, 123));
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        EXPECT_FLOAT_EQ(a[i].center_x, b[i].center_x);
        EXPECT_FLOAT_EQ(a[i].center_z, b[i].center_z);
    }
}

// ── R113 v3: exits-aware parcel derivation ──────────────────────────────

TEST(ParcelTest, NoAdjacentRoadProducesNoParcels) {
    // The honest "nothing to compose here" case -- a landlocked
    // small_house_block chunk with no real road on any side must not
    // fabricate a street, unlike v1/v2's own unconditional north+south
    // assumption.
    auto ctx = make_ctx(RegionType::small_house_block);
    ctx.exits = EdgeExits{}; // all false
    EXPECT_TRUE(derive_parcels(ctx).empty());
}

TEST(ParcelTest, OnlyNorthRoadProducesOneNorthFacingRow) {
    auto ctx = make_ctx(RegionType::small_house_block);
    ctx.exits = EdgeExits{};
    ctx.exits.north_road = true;
    const auto parcels = derive_parcels(ctx);
    ASSERT_FALSE(parcels.empty());
    for (const auto& p : parcels) {
        EXPECT_FLOAT_EQ(p.rotation_y, 0.0f);
        // Faces toward -Z (north, lower z than center).
        EXPECT_LT(p.street_z, p.center_z);
        EXPECT_NEAR(p.normal_x, 0.0f, 1e-5f);
        EXPECT_LT(p.normal_z, 0.0f);
    }
}

TEST(ParcelTest, OnlySouthRoadProducesOneSouthFacingRow) {
    auto ctx = make_ctx(RegionType::small_house_block);
    ctx.exits = EdgeExits{};
    ctx.exits.south_road = true;
    const auto parcels = derive_parcels(ctx);
    ASSERT_FALSE(parcels.empty());
    for (const auto& p : parcels) {
        EXPECT_FLOAT_EQ(p.rotation_y, 180.0f);
        EXPECT_GT(p.street_z, p.center_z);
        EXPECT_NEAR(p.normal_x, 0.0f, 1e-5f);
        EXPECT_GT(p.normal_z, 0.0f);
    }
}

TEST(ParcelTest, OnlyEastRoadProducesOneEastFacingRow) {
    auto ctx = make_ctx(RegionType::small_house_block);
    ctx.exits = EdgeExits{};
    ctx.exits.east_road = true;
    const auto parcels = derive_parcels(ctx);
    ASSERT_FALSE(parcels.empty());
    for (const auto& p : parcels) {
        EXPECT_FLOAT_EQ(p.rotation_y, 270.0f);
        // Faces toward +X (east, higher x than center).
        EXPECT_GT(p.street_x, p.center_x);
        EXPECT_GT(p.normal_x, 0.0f);
        EXPECT_NEAR(p.normal_z, 0.0f, 1e-5f);
    }
}

TEST(ParcelTest, OnlyWestRoadProducesOneWestFacingRow) {
    auto ctx = make_ctx(RegionType::small_house_block);
    ctx.exits = EdgeExits{};
    ctx.exits.west_road = true;
    const auto parcels = derive_parcels(ctx);
    ASSERT_FALSE(parcels.empty());
    for (const auto& p : parcels) {
        EXPECT_FLOAT_EQ(p.rotation_y, 90.0f);
        EXPECT_LT(p.street_x, p.center_x);
        EXPECT_LT(p.normal_x, 0.0f);
        EXPECT_NEAR(p.normal_z, 0.0f, 1e-5f);
    }
}

TEST(ParcelTest, AllFourRoadsProduceFourIndependentRows) {
    auto ctx = make_ctx(RegionType::small_house_block);
    ctx.exits.east_road = true;
    ctx.exits.west_road = true; // north/south already true via make_ctx()

    const auto parcels = derive_parcels(ctx);
    int north = 0, south = 0, east = 0, west = 0;
    for (const auto& p : parcels) {
        if (p.rotation_y == 0.0f)   ++north;
        if (p.rotation_y == 180.0f) ++south;
        if (p.rotation_y == 270.0f) ++east;
        if (p.rotation_y == 90.0f)  ++west;
    }
    EXPECT_GT(north, 0);
    EXPECT_GT(south, 0);
    EXPECT_GT(east, 0);
    EXPECT_GT(west, 0);
    EXPECT_EQ(static_cast<std::size_t>(north + south + east + west), parcels.size());
}

TEST(ParcelTest, ParcelsStayWithinChunkBoundsAcrossOrientationsAndSizes) {
    // Generalizes the existing north/south-only sweep to cover every
    // single-direction orientation too (east/west rows swap width/depth
    // and use a different rotation/normal derivation -- exactly the kind
    // of change that needs its own dedicated bounds sweep, not just
    // trusting the north/south case still holds).
    const std::vector<EdgeExits> exit_combos = [] {
        std::vector<EdgeExits> v;
        EdgeExits n; n.north_road = true; v.push_back(n);
        EdgeExits s; s.south_road = true; v.push_back(s);
        EdgeExits e; e.east_road  = true; v.push_back(e);
        EdgeExits w; w.west_road  = true; v.push_back(w);
        EdgeExits all; all.north_road = all.south_road = all.east_road = all.west_road = true;
        v.push_back(all);
        return v;
    }();

    for (const auto& exits : exit_combos) {
        for (const float chunk_size : {40.0f, 64.0f, 80.0f, 128.0f}) {
            for (std::uint64_t seed = 0; seed < 10; ++seed) {
                auto ctx = make_ctx(RegionType::small_house_block, seed);
                ctx.chunk_size_m = chunk_size;
                ctx.exits = exits;
                const auto parcels = derive_parcels(ctx);
                for (const auto& p : parcels) {
                    EXPECT_GE(p.center_x - p.width / 2.0f, 0.0f) << "seed=" << seed << " size=" << chunk_size;
                    EXPECT_LE(p.center_x + p.width / 2.0f, chunk_size) << "seed=" << seed << " size=" << chunk_size;
                    EXPECT_GE(p.center_z - p.depth / 2.0f, 0.0f) << "seed=" << seed << " size=" << chunk_size;
                    EXPECT_LE(p.center_z + p.depth / 2.0f, chunk_size) << "seed=" << seed << " size=" << chunk_size;
                }
            }
        }
    }
}

namespace {
bool parcels_aabb_overlap(const Parcel& a, const Parcel& b) {
    const float ax0 = a.center_x - a.width / 2.0f, ax1 = a.center_x + a.width / 2.0f;
    const float az0 = a.center_z - a.depth / 2.0f, az1 = a.center_z + a.depth / 2.0f;
    const float bx0 = b.center_x - b.width / 2.0f, bx1 = b.center_x + b.width / 2.0f;
    const float bz0 = b.center_z - b.depth / 2.0f, bz1 = b.center_z + b.depth / 2.0f;
    return ax0 < bx1 && ax1 > bx0 && az0 < bz1 && az1 > bz0;
}
} // namespace

TEST(ParcelTest, AdjacentActiveRowsNeverProduceOverlappingParcels) {
    // R113 (corner-aware layout) -- before this fix, north/south rows
    // (spaced along X) and east/west rows (spaced along Z) were computed
    // fully independently, always spanning the full chunk minus a fixed
    // side margin -- verified by direct computation that EVERY adjacent-
    // side combination (north+east, north+west, south+east, south+west)
    // overlapped near the shared corner for 100% of 200 sampled seeds
    // (opposite pairs, north+south or east+west, never did -- they sit on
    // opposite ends of the chunk). This is the real regression guard for
    // that fix: sweep every side combination, many seeds and chunk sizes,
    // and assert no two parcels' axis-aligned footprints ever overlap.
    const std::vector<EdgeExits> combos = [] {
        std::vector<EdgeExits> v;
        for (int mask = 1; mask < 16; ++mask) {
            EdgeExits e;
            e.north_road = mask & 1;
            e.south_road = mask & 2;
            e.east_road  = mask & 4;
            e.west_road  = mask & 8;
            v.push_back(e);
        }
        return v;
    }();

    for (const auto& exits : combos) {
        for (const float chunk_size : {40.0f, 64.0f, 80.0f, 128.0f}) {
            for (std::uint64_t seed = 0; seed < 30; ++seed) {
                auto ctx = make_ctx(RegionType::small_house_block, seed);
                ctx.chunk_size_m = chunk_size;
                ctx.exits = exits;
                const auto parcels = derive_parcels(ctx);
                for (std::size_t i = 0; i < parcels.size(); ++i)
                    for (std::size_t j = i + 1; j < parcels.size(); ++j)
                        ASSERT_FALSE(parcels_aabb_overlap(parcels[i], parcels[j]))
                            << "seed=" << seed << " size=" << chunk_size
                            << " mask=(" << exits.north_road << exits.south_road
                            << exits.east_road << exits.west_road << ")"
                            << " parcels " << i << "/" << j;
            }
        }
    }
}

TEST(ParcelTest, ParcelCountScalesWithChunkSize) {
    // v1's own x=13/x=51 literals only ever worked for exactly a 64m
    // chunk -- v2 must produce meaningfully MORE parcels for a larger
    // chunk and fewer for a smaller one, not the same fixed count
    // regardless of chunk_size_m.
    auto ctx_small = make_ctx(RegionType::small_house_block);
    ctx_small.chunk_size_m = 40.0f;
    auto ctx_large = make_ctx(RegionType::small_house_block);
    ctx_large.chunk_size_m = 96.0f;

    const auto small = derive_parcels(ctx_small);
    const auto large = derive_parcels(ctx_large);
    EXPECT_LT(small.size(), large.size());
}

TEST(ParcelTest, ParcelsStayWithinChunkBoundsAcrossManySeedsAndChunkSizes) {
    // ParcelsStayWithinChunkBounds only ever checked the one default
    // (seed=7, chunk_size_m=64) fixture -- the seed-driven column-count
    // roll and the chunk_size_m-parametric row math are both new in v2,
    // so sweep a real range of both rather than trusting a single point.
    for (const float chunk_size : {40.0f, 64.0f, 80.0f, 128.0f}) {
        for (std::uint64_t seed = 0; seed < 20; ++seed) {
            auto ctx = make_ctx(RegionType::small_house_block, seed);
            ctx.chunk_size_m = chunk_size;
            const auto parcels = derive_parcels(ctx);
            for (const auto& p : parcels) {
                EXPECT_GE(p.center_x - p.width / 2.0f, 0.0f)
                    << "chunk_size=" << chunk_size << " seed=" << seed;
                EXPECT_LE(p.center_x + p.width / 2.0f, chunk_size)
                    << "chunk_size=" << chunk_size << " seed=" << seed;
                EXPECT_GE(p.center_z - p.depth / 2.0f, 0.0f)
                    << "chunk_size=" << chunk_size << " seed=" << seed;
                EXPECT_LE(p.center_z + p.depth / 2.0f, chunk_size)
                    << "chunk_size=" << chunk_size << " seed=" << seed;
            }
        }
    }
}

TEST(ParcelTest, ReturnsEmptyForOtherRegions) {
    // R126/R127 -- apartment_block/shop_street now DO produce parcels
    // (their own dedicated coverage is DerivesParcelsForApartmentBlock/
    // DerivesParcelsForShopStreet below); every other region must still
    // return empty.
    EXPECT_TRUE(derive_parcels(make_ctx(RegionType::park)).empty());
    EXPECT_TRUE(derive_parcels(make_ctx(RegionType::square)).empty());
}

TEST(ParcelTest, DerivesParcelsForApartmentBlock) {
    // R126 -- apartment_block reuses the exact same street-first row
    // algorithm as small_house_block, just with its own, wider/deeper
    // size class (see Parcel.cpp's kApartmentParcelWidth/Depth).
    const auto parcels = derive_parcels(make_ctx(RegionType::apartment_block));
    ASSERT_FALSE(parcels.empty());
    for (const auto& p : parcels) {
        EXPECT_EQ(p.kind, RegionType::apartment_block);
        EXPECT_FLOAT_EQ(p.frontage_extent, 14.6f);
    }
}

TEST(ParcelTest, ApartmentBlockParcelsStayWithinChunkBounds) {
    auto ctx = make_ctx(RegionType::apartment_block);
    const auto parcels = derive_parcels(ctx);
    ASSERT_FALSE(parcels.empty());
    for (const auto& p : parcels) {
        EXPECT_GE(p.center_x - p.width / 2.0f,  0.0f);
        EXPECT_LE(p.center_x + p.width / 2.0f,  ctx.chunk_size_m);
        EXPECT_GE(p.center_z - p.depth / 2.0f,  0.0f);
        EXPECT_LE(p.center_z + p.depth / 2.0f,  ctx.chunk_size_m);
    }
}

TEST(ParcelTest, DerivesParcelsForShopStreet) {
    // R127 -- shop_street reuses the same street-first row algorithm,
    // with its own narrow size class (see Parcel.cpp's
    // kShopParcelWidth/Depth).
    const auto parcels = derive_parcels(make_ctx(RegionType::shop_street));
    ASSERT_FALSE(parcels.empty());
    for (const auto& p : parcels) {
        EXPECT_EQ(p.kind, RegionType::shop_street);
        EXPECT_FLOAT_EQ(p.frontage_extent, 8.6f);
    }
}

TEST(ParcelTest, ShopStreetParcelsStayWithinChunkBounds) {
    auto ctx = make_ctx(RegionType::shop_street);
    const auto parcels = derive_parcels(ctx);
    ASSERT_FALSE(parcels.empty());
    for (const auto& p : parcels) {
        EXPECT_GE(p.center_x - p.width / 2.0f,  0.0f);
        EXPECT_LE(p.center_x + p.width / 2.0f,  ctx.chunk_size_m);
        EXPECT_GE(p.center_z - p.depth / 2.0f,  0.0f);
        EXPECT_LE(p.center_z + p.depth / 2.0f,  ctx.chunk_size_m);
    }
}

TEST(ParcelTest, ParcelsStayWithinChunkBounds) {
    auto ctx = make_ctx(RegionType::small_house_block);
    const auto parcels = derive_parcels(ctx);
    for (const auto& p : parcels) {
        EXPECT_GE(p.center_x - p.width / 2.0f,  0.0f);
        EXPECT_LE(p.center_x + p.width / 2.0f,  ctx.chunk_size_m);
        EXPECT_GE(p.center_z - p.depth / 2.0f,  0.0f);
        EXPECT_LE(p.center_z + p.depth / 2.0f,  ctx.chunk_size_m);
    }
}

// ── BuildingComposer ──────────────────────────────────────────────────────

TEST_F(BuildingComposerFixture, ComposesRealInstancesForSmallHouseBlock) {
    // R113 (size-aware matching) -- house.rowhouse.modular_01 has a real
    // FLAT roof (row_roof_slab, see build_mc3lib_content.cpp), so it
    // legitimately does NOT carry the "gable_roof" style tag the way
    // house_gable_default/house.gable.modular_01 do -- under the default
    // style profile (roofFamily "gable_roof"), it is correctly never a
    // candidate at all, regardless of a parcel's width class. That means
    // a ctx whose rows roll wide-class (derive_parcels()'s own per-row
    // width roll, which depends on both axis and seed) would leave those
    // parcels with NO compatible candidate under this profile -- a real,
    // correct "no style-compatible match" skip, not a bug, but not what
    // this test means to exercise either. Seed 0 was checked to roll
    // both rows (north/south, per make_ctx()'s default exits) standard-
    // width, so every parcel here has a real gable-tagged match. The
    // wide/rowhouse path has its own dedicated coverage (see
    // RowhouseIsARealHouseCategoryCandidateWithWideNominalSize and
    // RowhouseHasSixTiledFacadeBaysFromScript).
    auto composed = BuildingComposer{}.compose_chunk(make_ctx(RegionType::small_house_block, /*seed=*/0));
    ASSERT_TRUE(composed.has_value());

    // R103/R104 v1 added a second house candidate
    // (house.gable.modular_01) alongside house_gable_default -- each
    // parcel deterministically picks one of the two, so check that a
    // known house definition appears somewhere (not pinning exactly
    // one) and that there are exactly as many house instance slots as
    // R113 v2's derive_parcels() actually produces (not a fixed literal).
    EXPECT_TRUE(any_definition_present(*composed, {"house_gable_default", "house.gable.modular_01"}))
        << *composed;

    const auto expected_parcels = derive_parcels(make_ctx(RegionType::small_house_block, /*seed=*/0)).size();
    std::size_t count = 0, pos = 0;
    while ((pos = composed->find("id=\"house_", pos)) != std::string::npos) { ++count; pos += 1; }
    EXPECT_EQ(count, expected_parcels);
}

TEST_F(BuildingComposerFixture, ReturnsNulloptForRegionWithNoParcels) {
    auto composed = BuildingComposer{}.compose_chunk(make_ctx(RegionType::park));
    EXPECT_FALSE(composed.has_value());
}

// R126 (BuildingComposer v2, apartment_block)
TEST_F(BuildingComposerFixture, ComposesRealInstancesForApartmentBlock) {
    auto composed = BuildingComposer{}.compose_chunk(make_ctx(RegionType::apartment_block));
    ASSERT_TRUE(composed.has_value());

    EXPECT_NE(composed->find("definition=\"apartment.block.wide_01\""), std::string::npos) << *composed;

    const auto expected_parcels = derive_parcels(make_ctx(RegionType::apartment_block)).size();
    std::size_t count = 0, pos = 0;
    while ((pos = composed->find("id=\"apartment_", pos)) != std::string::npos) { ++count; pos += 1; }
    EXPECT_EQ(count, expected_parcels);

    // Street furniture/vehicles/trees are placed the same, unchanged way
    // for apartment_block too (shared logic, not duplicated).
    EXPECT_NE(composed->find("id=\"streetlamp_"), std::string::npos) << *composed;
    EXPECT_NE(composed->find("id=\"vehicle_"), std::string::npos) << *composed;
}

// R127 (BuildingComposer v2, shop_street)
TEST_F(BuildingComposerFixture, ComposesRealInstancesForShopStreet) {
    auto composed = BuildingComposer{}.compose_chunk(make_ctx(RegionType::shop_street));
    ASSERT_TRUE(composed.has_value());

    EXPECT_NE(composed->find("definition=\"shop.building.storefront_01\""), std::string::npos) << *composed;

    const auto expected_parcels = derive_parcels(make_ctx(RegionType::shop_street)).size();
    std::size_t count = 0, pos = 0;
    while ((pos = composed->find("id=\"shop_", pos)) != std::string::npos) { ++count; pos += 1; }
    EXPECT_EQ(count, expected_parcels);

    // Paved ground surface, cross-checked against ShopStreetGenerator's
    // own cobblestone ground (not the residential grass_garden).
    EXPECT_NE(composed->find("cobblestone"), std::string::npos) << *composed;
}

// R128 (city showcase completion) -- landmark placement, independent of
// the parcel/AssetRegistry query mechanism entirely.
TEST_F(BuildingComposerFixture, ComposesLandmarkInstanceWhenConfigured) {
    auto ctx = make_ctx(RegionType::small_house_block);
    ctx.landmark.definition_id = "landmark.clocktower_01";
    ctx.landmark.x = 10.0f;
    ctx.landmark.z = 20.0f;
    ctx.landmark.rotation_y = 30.0f;

    auto composed = BuildingComposer{}.compose_chunk(ctx);
    ASSERT_TRUE(composed.has_value());
    // rotation is only emitted at all when non-zero, as "rx ry rz"
    // (Mc3XmlWriter's own convention -- see RotationBindingTests.cpp).
    EXPECT_NE(composed->find("<instance id=\"landmark\" position=\"10 0 20\" "
                             "rotation=\"0 30 0\" definition=\"landmark.clocktower_01\""),
              std::string::npos) << *composed;
}

TEST_F(BuildingComposerFixture, DoesNotPlaceLandmarkWhenNotConfigured) {
    // Default make_ctx() leaves ctx.landmark.definition_id empty (the "no
    // landmark here" sentinel) -- must not add any "landmark" instance.
    auto composed = BuildingComposer{}.compose_chunk(make_ctx(RegionType::small_house_block));
    ASSERT_TRUE(composed.has_value());
    EXPECT_EQ(composed->find("id=\"landmark\""), std::string::npos) << *composed;
}

// R128 (city showcase LOD wiring) -- ctx.lod, once set to a coarse value,
// now actually gets consulted: a composer-placed instance whose asset has
// a real "low" Mc3AssetMetadata.lods entry (streetlamp.classic_01, an R112
// mc3lib asset) resolves to that low-detail proxy id instead of its own
// full-detail id.
TEST_F(BuildingComposerFixture, FarLodResolvesStreetlampToItsLowLodDefinition) {
    const auto* lamp = AssetRegistry::instance().get("streetlamp.classic_01");
    ASSERT_NE(lamp, nullptr);
    const auto it = lamp->meta.lods.find("low");
    ASSERT_NE(it, lamp->meta.lods.end())
        << "streetlamp.classic_01 must carry a real 'low' lods entry for this test to be meaningful";

    auto ctx = make_ctx(RegionType::small_house_block);
    ctx.lod = 0; // far/coarse
    auto composed = BuildingComposer{}.compose_chunk(ctx);
    ASSERT_TRUE(composed.has_value());
    EXPECT_NE(composed->find("definition=\"" + it->second + "\""), std::string::npos) << *composed;
    EXPECT_EQ(composed->find("definition=\"streetlamp.classic_01\""), std::string::npos) << *composed;
}

TEST_F(BuildingComposerFixture, NearLodUsesFullDetailStreetlamp) {
    auto ctx = make_ctx(RegionType::small_house_block);
    ctx.lod = 2; // default/near -- ChunkContext's own documented default
    auto composed = BuildingComposer{}.compose_chunk(ctx);
    ASSERT_TRUE(composed.has_value());
    EXPECT_NE(composed->find("definition=\"streetlamp.classic_01\""), std::string::npos) << *composed;
}

// R129 (BuildingComposer v3, square) -- ports SquareGenerator.cpp's own
// literal fountain/plinth/paths/monument geometry, but sources the 4
// corner lamps from a real AssetRegistry::query("street_furniture", ...)
// pick, and keeps benches/corner trees as direct-by-id references.
TEST_F(BuildingComposerFixture, ComposesRealInstancesForSquare) {
    auto composed = BuildingComposer{}.compose_chunk(make_ctx(RegionType::square));
    ASSERT_TRUE(composed.has_value());

    // Literal fountain/plinth geometry, unchanged from SquareGenerator.
    EXPECT_NE(composed->find("id=\"plinth\""), std::string::npos) << *composed;
    EXPECT_NE(composed->find("id=\"fountain_ring\""), std::string::npos) << *composed;
    EXPECT_NE(composed->find("id=\"water_bowl\""), std::string::npos) << *composed;
    EXPECT_NE(composed->find("id=\"jet\""), std::string::npos) << *composed;

    // 4 corner lamps, now real AssetRegistry-queried instances (NOT the
    // legacy hardcoded "metal_lamp_ornate" raw cylinder).
    std::size_t lamp_count = 0, pos = 0;
    while ((pos = composed->find("id=\"lamp_", pos)) != std::string::npos) { ++lamp_count; pos += 1; }
    EXPECT_EQ(lamp_count, 4u);
    EXPECT_EQ(composed->find("metal_lamp_ornate"), std::string::npos) << *composed;
    EXPECT_TRUE(any_definition_present(*composed, {
        "streetlamp.classic_01", "streetlamp.classic_02", "streetlamp.modern_01"
    })) << *composed;

    // 8 benches, direct-by-id (unchanged from SquareGenerator).
    std::size_t bench_count = 0;
    pos = 0;
    while ((pos = composed->find("id=\"bench_", pos)) != std::string::npos) { ++bench_count; pos += 1; }
    EXPECT_EQ(bench_count, 8u);
    EXPECT_NE(composed->find("definition=\"bench_stone\""), std::string::npos) << *composed;

    // 4 corner trees, direct-by-id (kYardTreeIds), same convention as the
    // residential yard trees.
    std::size_t tree_count = 0;
    pos = 0;
    while ((pos = composed->find("id=\"tree_", pos)) != std::string::npos) { ++tree_count; pos += 1; }
    EXPECT_EQ(tree_count, 4u);
    EXPECT_TRUE(any_definition_present(*composed, {
        "tree_oak", "tree_lime", "tree_birch", "tree_apple"
    })) << *composed;

    // Cardinal paths, unchanged from SquareGenerator.
    EXPECT_NE(composed->find("id=\"path_n\""), std::string::npos) << *composed;
    EXPECT_NE(composed->find("id=\"path_s\""), std::string::npos) << *composed;
    EXPECT_NE(composed->find("id=\"path_e\""), std::string::npos) << *composed;
    EXPECT_NE(composed->find("id=\"path_w\""), std::string::npos) << *composed;

    // No monument -- default ctx.map_context.available is false.
    EXPECT_EQ(composed->find("id=\"monument\""), std::string::npos) << *composed;
}

TEST_F(BuildingComposerFixture, SquareAddsMonumentWhenNearNamedSettlement) {
    auto ctx = make_ctx(RegionType::square);
    ctx.map_context.available          = true;
    ctx.map_context.nearest_place_name = "Testville";

    auto composed = BuildingComposer{}.compose_chunk(ctx);
    ASSERT_TRUE(composed.has_value());
    EXPECT_NE(composed->find("id=\"monument\""), std::string::npos) << *composed;
}

// Graceful std::nullopt fallback (same discipline as R126/R127's own
// ApartmentBlockReturnsNulloptWhenNoApartmentAssetRegistered/
// ShopStreetReturnsNulloptWhenNoShopAssetRegistered below) -- with no
// street_furniture asset registered at all, square must fall through to
// nullopt (so ChunkPipeline uses the untouched, legacy SquareGenerator),
// not compose a partial/broken plaza.
TEST(BuildingComposerUnitTest, SquareReturnsNulloptWhenNoStreetFurnitureAssetRegistered) {
    AssetRegistry::instance().clear_for_tests();
    ObjectDefinitionLibrary::instance().load_all();
    if (auto def = ObjectDefinitionLibrary::instance().get("house_gable_default");
        def && def->assetMetadata.has_value()) {
        AssetRegistry::instance().register_asset(AssetEntry{"house_gable_default", def, *def->assetMetadata});
    }

    auto composed = BuildingComposer{}.compose_chunk(make_ctx(RegionType::square));
    EXPECT_FALSE(composed.has_value());

    register_composer_assets();
}

// R126 -- v1 has only one apartment-tagged asset; with none registered at
// all, apartment_block must gracefully fall through to nullopt exactly
// like small_house_block's own ReturnsNulloptWhenRegistryHasNoMatchingAsset,
// not crash or compose with an empty/wrong asset.
TEST(BuildingComposerUnitTest, ApartmentBlockReturnsNulloptWhenNoApartmentAssetRegistered) {
    AssetRegistry::instance().clear_for_tests();
    ObjectDefinitionLibrary::instance().load_all();
    if (auto def = ObjectDefinitionLibrary::instance().get("house_gable_default");
        def && def->assetMetadata.has_value()) {
        AssetRegistry::instance().register_asset(AssetEntry{"house_gable_default", def, *def->assetMetadata});
    }

    auto composed = BuildingComposer{}.compose_chunk(make_ctx(RegionType::apartment_block));
    EXPECT_FALSE(composed.has_value());

    register_composer_assets();
}

// R127 -- same graceful-fallback discipline for shop_street: with only a
// house asset registered (no "shop" category asset), compose_chunk() must
// return nullopt, not crash or compose with the wrong category.
TEST(BuildingComposerUnitTest, ShopStreetReturnsNulloptWhenNoShopAssetRegistered) {
    AssetRegistry::instance().clear_for_tests();
    ObjectDefinitionLibrary::instance().load_all();
    if (auto def = ObjectDefinitionLibrary::instance().get("house_gable_default");
        def && def->assetMetadata.has_value()) {
        AssetRegistry::instance().register_asset(AssetEntry{"house_gable_default", def, *def->assetMetadata});
    }

    auto composed = BuildingComposer{}.compose_chunk(make_ctx(RegionType::shop_street));
    EXPECT_FALSE(composed.has_value());

    register_composer_assets();
}

TEST(BuildingComposerUnitTest, ReturnsNulloptWhenRegistryHasNoMatchingAsset) {
    // A fresh AssetRegistry state (cleared) proves the fallback path for
    // real -- compose_chunk() must return nullopt, not crash or compose
    // with no asset, when nothing is registered.
    AssetRegistry::instance().clear_for_tests();
    auto ctx = make_ctx(RegionType::small_house_block);
    auto composed = BuildingComposer{}.compose_chunk(ctx);
    EXPECT_FALSE(composed.has_value());
    // Restore for any other test in this binary relying on the populated
    // singleton (idempotent re-population, same convention every other
    // process-wide-singleton test in this codebase already follows).
    register_composer_assets();
}

// ── ChunkPipeline integration ────────────────────────────────────────────

TEST(ChunkPipelineComposerIntegration, DisabledByDefaultProducesUnchangedOutput) {
    register_builtin_materials();
    ObjectDefinitionLibrary::instance().load_all();
    register_composer_assets();

    WorldConfig cfg;
    cfg.grid_w = 4; cfg.grid_h = 4;
    WorldMap map(cfg);
    map.set_info(0, 0, ChunkInfo{ZoneType::city, RegionType::small_house_block, EdgeExits{}});

    ChunkPipeline pipeline(cfg, map, ChunkCache{tmp_dir("disabled")});
    ChunkDiagnostics diag;
    const std::string xml = pipeline.get(0, 0, &diag);
    EXPECT_FALSE(xml.empty());
    EXPECT_NE(diag.source, ChunkDiagnostics::Source::Composer)
        << "use_world_composer defaults to false -- composer must never win";
}

TEST(ChunkPipelineComposerIntegration, EnabledFlagRoutesThroughComposerForSmallHouseBlock) {
    register_builtin_materials();
    ObjectDefinitionLibrary::instance().load_all();
    register_composer_assets();

    WorldConfig cfg;
    cfg.grid_w = 4; cfg.grid_h = 4;
    cfg.use_world_composer = true;
    WorldMap map(cfg);
    // R113 v3 -- derive_parcels() is exits-aware now; a chunk with no
    // real adjacent road produces no parcels at all, so this test must
    // provide real exits (matching make_ctx()'s own default "streets to
    // north and south" shape) for the composer to have anything to do.
    EdgeExits exits;
    exits.north_road = true;
    exits.south_road = true;
    map.set_info(0, 0, ChunkInfo{ZoneType::city, RegionType::small_house_block, exits});

    ChunkPipeline pipeline(cfg, map, ChunkCache{tmp_dir("enabled")});
    ChunkDiagnostics diag;
    const std::string xml = pipeline.get(0, 0, &diag);
    EXPECT_FALSE(xml.empty());
    EXPECT_EQ(diag.source, ChunkDiagnostics::Source::Composer);
    EXPECT_EQ(diag.generator_id, "cpp.chunk.composer.small_house_block");
    EXPECT_TRUE(any_definition_present(xml, {
        "house_gable_default", "house.gable.modular_01", "house.gable.wide_01"
    })) << xml;
}

// R126 (BuildingComposer v2, apartment_block)
TEST(ChunkPipelineComposerIntegration, EnabledFlagRoutesThroughComposerForApartmentBlock) {
    register_builtin_materials();
    ObjectDefinitionLibrary::instance().load_all();
    register_composer_assets();

    WorldConfig cfg;
    cfg.grid_w = 4; cfg.grid_h = 4;
    cfg.use_world_composer = true;
    WorldMap map(cfg);
    EdgeExits exits;
    exits.north_road = true;
    exits.south_road = true;
    map.set_info(0, 0, ChunkInfo{ZoneType::city, RegionType::apartment_block, exits});

    ChunkPipeline pipeline(cfg, map, ChunkCache{tmp_dir("enabled_apartment")});
    ChunkDiagnostics diag;
    const std::string xml = pipeline.get(0, 0, &diag);
    EXPECT_FALSE(xml.empty());
    EXPECT_EQ(diag.source, ChunkDiagnostics::Source::Composer);
    EXPECT_EQ(diag.generator_id, "cpp.chunk.composer.apartment_block");
    EXPECT_NE(xml.find("definition=\"apartment.block.wide_01\""), std::string::npos) << xml;
}

// R127 (BuildingComposer v2, shop_street)
TEST(ChunkPipelineComposerIntegration, EnabledFlagRoutesThroughComposerForShopStreet) {
    register_builtin_materials();
    ObjectDefinitionLibrary::instance().load_all();
    register_composer_assets();

    WorldConfig cfg;
    cfg.grid_w = 4; cfg.grid_h = 4;
    cfg.use_world_composer = true;
    WorldMap map(cfg);
    EdgeExits exits;
    exits.north_road = true;
    exits.south_road = true;
    map.set_info(0, 0, ChunkInfo{ZoneType::city, RegionType::shop_street, exits});

    ChunkPipeline pipeline(cfg, map, ChunkCache{tmp_dir("enabled_shop")});
    ChunkDiagnostics diag;
    const std::string xml = pipeline.get(0, 0, &diag);
    EXPECT_FALSE(xml.empty());
    EXPECT_EQ(diag.source, ChunkDiagnostics::Source::Composer);
    EXPECT_EQ(diag.generator_id, "cpp.chunk.composer.shop_street");
    EXPECT_NE(xml.find("definition=\"shop.building.storefront_01\""), std::string::npos) << xml;
}

// R129 (BuildingComposer v3, square) -- unlike apartment_block/shop_street,
// square needs no EdgeExits at all (it's not parcel/road-frontage-based),
// so this test deliberately does NOT set any exits, unlike the 3 tests
// above.
TEST(ChunkPipelineComposerIntegration, EnabledFlagRoutesThroughComposerForSquare) {
    register_builtin_materials();
    ObjectDefinitionLibrary::instance().load_all();
    register_composer_assets();

    WorldConfig cfg;
    cfg.grid_w = 4; cfg.grid_h = 4;
    cfg.use_world_composer = true;
    WorldMap map(cfg);
    map.set_info(0, 0, ChunkInfo{ZoneType::city, RegionType::square, EdgeExits{}});

    ChunkPipeline pipeline(cfg, map, ChunkCache{tmp_dir("enabled_square")});
    ChunkDiagnostics diag;
    const std::string xml = pipeline.get(0, 0, &diag);
    EXPECT_FALSE(xml.empty());
    EXPECT_EQ(diag.source, ChunkDiagnostics::Source::Composer);
    EXPECT_EQ(diag.generator_id, "cpp.chunk.composer.square");
    EXPECT_NE(xml.find("id=\"plinth\""), std::string::npos) << xml;
    EXPECT_TRUE(any_definition_present(xml, {
        "streetlamp.classic_01", "streetlamp.classic_02", "streetlamp.modern_01"
    })) << xml;
}

// Enabling the flag must NOT affect regions the composer has no
// dedicated path for at all (everything except small_house_block/
// apartment_block/shop_street/square) -- falls through to the existing
// chain exactly as if the flag were off.
TEST(ChunkPipelineComposerIntegration, EnabledFlagDoesNotAffectOtherRegions) {
    register_builtin_materials();
    ObjectDefinitionLibrary::instance().load_all();
    register_composer_assets();

    WorldConfig cfg;
    cfg.grid_w = 4; cfg.grid_h = 4;
    cfg.use_world_composer = true;
    WorldMap map(cfg);
    map.set_info(0, 0, ChunkInfo{ZoneType::city, RegionType::park, EdgeExits{}});

    ChunkPipeline pipeline(cfg, map, ChunkCache{tmp_dir("other_region")});
    ChunkDiagnostics diag;
    const std::string xml = pipeline.get(0, 0, &diag);
    EXPECT_FALSE(xml.empty());
    EXPECT_NE(diag.source, ChunkDiagnostics::Source::Composer);
}
