// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <unordered_set>
#include "TaxonomyRegistry.hpp"
#include "ContainmentRuleRegistry.hpp"

static const std::filesystem::path TAX_JSON   = "data/taxonomy/taxonomy.json";
static const std::filesystem::path CONT_JSON  = "data/taxonomy/containment.json";

// T099 — TaxonomyRegistry loads all nodes without error
TEST(TaxonomyTests, LoadsAllNodes) {
    MeshWorld::TaxonomyRegistry reg;
    ASSERT_NO_THROW(reg.load(TAX_JSON));

    auto all = reg.all();
    EXPECT_GE(all.size(), 30u) << "Expected at least 30 taxonomy nodes";

    EXPECT_TRUE(reg.has("zone.park"));
    EXPECT_TRUE(reg.has("object.fridge"));
    EXPECT_TRUE(reg.has("room.kitchen"));
    EXPECT_TRUE(reg.has("building.simple_house"));

    const auto& park = reg.get("zone.park");
    EXPECT_EQ(park.kind, "zone");
    EXPECT_FALSE(park.name.empty());
}

TEST(TaxonomyTests, ByKindFilters) {
    MeshWorld::TaxonomyRegistry reg;
    reg.load(TAX_JSON);

    auto zones   = reg.by_kind("zone");
    auto objects = reg.by_kind("object");
    auto rooms   = reg.by_kind("room");

    EXPECT_GE(zones.size(),   4u);
    EXPECT_GE(objects.size(), 10u);
    EXPECT_GE(rooms.size(),   4u);

    for (const auto& n : zones)
        EXPECT_EQ(n.kind, "zone");
}

TEST(TaxonomyTests, GetThrowsForUnknown) {
    MeshWorld::TaxonomyRegistry reg;
    reg.load(TAX_JSON);
    EXPECT_THROW(reg.get("nonexistent.node"), std::out_of_range);
}

// T100 — children_of("room.kitchen") returns fridge entry
TEST(TaxonomyTests, ChildrenOfKitchenReturnsFridge) {
    MeshWorld::ContainmentRuleRegistry reg;
    ASSERT_NO_THROW(reg.load(CONT_JSON));

    auto children = reg.children_of("room.kitchen");
    EXPECT_FALSE(children.empty());

    bool found_fridge = false;
    for (const auto& r : children)
        if (r.child == "object.fridge") {
            found_fridge = true;
            EXPECT_GE(r.min_count, 1);
            EXPECT_GE(r.lod_max, 3);
        }
    EXPECT_TRUE(found_fridge) << "Expected object.fridge in room.kitchen containment rules";
}

TEST(TaxonomyTests, CanContainWorks) {
    MeshWorld::ContainmentRuleRegistry reg;
    reg.load(CONT_JSON);

    EXPECT_TRUE(reg.can_contain("zone.park",    "object.tree"));
    EXPECT_TRUE(reg.can_contain("zone.park",    "object.bench"));
    EXPECT_FALSE(reg.can_contain("zone.park",   "object.fridge"));
    EXPECT_FALSE(reg.can_contain("nonexistent", "object.tree"));
}

// T101 — children_at_lod("object.fridge", 0) returns empty (lod_max=4 requires LOD 4+ to appear)
// LOD convention: 0 = coarsest (far), higher = finer detail.
// Rule with lod_max=N appears only when queried lod >= N.
TEST(TaxonomyTests, ChildrenAtLodFilters) {
    MeshWorld::ContainmentRuleRegistry reg;
    reg.load(CONT_JSON);

    // object.fridge food items have lod_max=4 → not visible at LOD 0
    auto fridge_at_0 = reg.children_at_lod("object.fridge", 0);
    EXPECT_TRUE(fridge_at_0.empty()) << "fridge food (lod_max=4) must not appear at LOD 0";

    // At lod=4 all fridge children (lod_max=4) should appear
    auto fridge_at_4 = reg.children_at_lod("object.fridge", 4);
    EXPECT_FALSE(fridge_at_4.empty()) << "Expected fridge contents at lod=4";
    for (const auto& r : fridge_at_4)
        EXPECT_LE(r.lod_max, 4);

    // Zone park children have lod_max=2 → appear at lod=2 but NOT at lod=1
    auto park_at_2 = reg.children_at_lod("zone.park", 2);
    EXPECT_FALSE(park_at_2.empty()) << "park objects (lod_max=2) should appear at lod=2";

    auto park_at_1 = reg.children_at_lod("zone.park", 1);
    EXPECT_TRUE(park_at_1.empty()) << "park objects (lod_max=2) must NOT appear at lod=1";

    // Region children have lod_max=0 → always visible
    auto region_at_0 = reg.children_at_lod("region.city", 0);
    EXPECT_FALSE(region_at_0.empty()) << "region zone rules (lod_max=0) must appear at lod=0";
}

// R125 — object.sqlite3 -> taxonomy/containment curation regression checks.
// These read the raw JSON files directly (not just through the registries)
// so a duplicate id/edge or an orphan reference is caught even if it would
// not otherwise surface through TaxonomyRegistry::load()'s own behavior.
TEST(TaxonomyTests, R125NoDuplicateTaxonomyIds) {
    std::ifstream f(TAX_JSON);
    ASSERT_TRUE(f.is_open());
    auto j = nlohmann::json::parse(f);

    std::unordered_set<std::string> seen;
    for (const auto& node : j) {
        const std::string id = node.at("id").get<std::string>();
        EXPECT_TRUE(seen.insert(id).second) << "Duplicate taxonomy id: " << id;
    }
}

TEST(TaxonomyTests, R125NoDuplicateContainmentEdges) {
    std::ifstream f(CONT_JSON);
    ASSERT_TRUE(f.is_open());
    auto j = nlohmann::json::parse(f);

    std::unordered_set<std::string> seen;
    for (const auto& rule : j) {
        const std::string key = rule.at("parent").get<std::string>() + "->" +
                                 rule.at("child").get<std::string>();
        EXPECT_TRUE(seen.insert(key).second) << "Duplicate containment edge: " << key;
    }
}

TEST(TaxonomyTests, R125NoOrphanContainmentReferences) {
    std::ifstream tf(TAX_JSON);
    ASSERT_TRUE(tf.is_open());
    auto taxonomy_json = nlohmann::json::parse(tf);

    std::unordered_set<std::string> ids;
    for (const auto& node : taxonomy_json)
        ids.insert(node.at("id").get<std::string>());

    std::ifstream cf(CONT_JSON);
    ASSERT_TRUE(cf.is_open());
    auto containment_json = nlohmann::json::parse(cf);

    for (const auto& rule : containment_json) {
        const std::string parent = rule.at("parent").get<std::string>();
        const std::string child  = rule.at("child").get<std::string>();
        EXPECT_TRUE(ids.count(parent)) << "Containment rule references unknown parent id: " << parent;
        EXPECT_TRUE(ids.count(child))  << "Containment rule references unknown child id: " << child;
    }
}

// R125 — spot-check the curated additions themselves (a bounded slice of
// object.sqlite3's house/facade/street-furniture/vehicle-adjacent rows,
// see object.md and plan.md's R125 entry).
TEST(TaxonomyTests, R125CuratedAdditionsLoad) {
    MeshWorld::TaxonomyRegistry treg;
    treg.load(TAX_JSON);

    EXPECT_TRUE(treg.has("object.roof"));
    EXPECT_TRUE(treg.has("object.chimney"));
    EXPECT_TRUE(treg.has("object.bay_window"));
    EXPECT_TRUE(treg.has("object.gambrel_roof"));
    EXPECT_TRUE(treg.has("object.bike_rack"));
    EXPECT_TRUE(treg.has("object.cargo_van"));

    MeshWorld::ContainmentRuleRegistry creg;
    creg.load(CONT_JSON);

    EXPECT_TRUE(creg.can_contain("object.window", "object.bay_window"));
    EXPECT_TRUE(creg.can_contain("object.roof", "object.gambrel_roof"));
    EXPECT_TRUE(creg.can_contain("object.chimney", "object.chimney_cap"));
    EXPECT_TRUE(creg.can_contain("place.parking_lot", "object.cargo_van"));
    EXPECT_FALSE(creg.can_contain("object.window", "object.cargo_van"));
}
