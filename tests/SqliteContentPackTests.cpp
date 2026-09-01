// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include <gtest/gtest.h>
#include "SqliteContentPack.hpp"
#include "LuaGeneratorRegistry.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static fs::path tmp_db(const char* name) {
    auto p = fs::temp_directory_path() / name;
    if (fs::exists(p)) fs::remove(p);
    return p;
}

// T123 — pack chair.lua into SQLite, retrieve and verify source matches
TEST(SqliteContentPackTests, PackAndRetrieveLuaGenerator) {
    auto db_path = tmp_db("mw_test_pack.sqlite");

    // Load chair.lua from disk
    MeshWorld::LuaGeneratorRegistry lua_reg;
    lua_reg.load_from_dir("generators/lua");
    ASSERT_TRUE(lua_reg.has("lua.object.chair.simple"))
        << "lua.object.chair.simple not found — check generators/lua directory";
    const std::string original_src = lua_reg.get("lua.object.chair.simple");

    // Pack into SQLite
    {
        MeshWorld::SqliteContentPack pack(db_path);
        MeshWorld::LuaGeneratorRow row;
        row.id       = "lua.object.chair.simple";
        row.source   = original_src;
        row.category = "object";
        row.version  = "0.1.0";
        pack.upsert_lua_generator(row);

        EXPECT_TRUE(pack.has_lua_generator("lua.object.chair.simple"));
        EXPECT_FALSE(pack.has_lua_generator("nonexistent.xyz"));
    }

    // Reopen and verify
    {
        MeshWorld::SqliteContentPack pack(db_path);
        ASSERT_TRUE(pack.has_lua_generator("lua.object.chair.simple"));

        auto row = pack.get_lua_generator("lua.object.chair.simple");
        EXPECT_EQ(row.id,       "lua.object.chair.simple");
        EXPECT_EQ(row.source,   original_src);
        EXPECT_EQ(row.category, "object");
        EXPECT_EQ(row.version,  "0.1.0");

        auto all = pack.all_lua_generators();
        EXPECT_EQ(all.size(), 1u);
    }

    fs::remove(db_path);
}

TEST(SqliteContentPackTests, TaxonomyNodeRoundTrip) {
    auto db_path = tmp_db("mw_test_tax.sqlite");
    {
        MeshWorld::SqliteContentPack pack(db_path);
        MeshWorld::TaxonomyNodeRow row;
        row.id   = "zone.park";
        row.kind = "zone";
        row.name = "Park";
        pack.upsert_taxonomy_node(row);
    }
    {
        MeshWorld::SqliteContentPack pack(db_path);
        auto all = pack.all_taxonomy_nodes();
        ASSERT_EQ(all.size(), 1u);
        EXPECT_EQ(all[0].id,   "zone.park");
        EXPECT_EQ(all[0].kind, "zone");
        EXPECT_EQ(all[0].name, "Park");
    }
    fs::remove(db_path);
}

TEST(SqliteContentPackTests, ContainmentRuleRoundTrip) {
    auto db_path = tmp_db("mw_test_cont.sqlite");
    {
        MeshWorld::SqliteContentPack pack(db_path);
        MeshWorld::ContainmentRuleRow row;
        row.parent      = "zone.park";
        row.child       = "object.bench";
        row.probability = 1.0f;
        row.min_count   = 4;
        row.max_count   = 12;
        row.lod_max     = 2;
        pack.upsert_containment_rule(row);
    }
    {
        MeshWorld::SqliteContentPack pack(db_path);
        auto all = pack.all_containment_rules();
        ASSERT_EQ(all.size(), 1u);
        EXPECT_EQ(all[0].parent,    "zone.park");
        EXPECT_EQ(all[0].child,     "object.bench");
        EXPECT_FLOAT_EQ(all[0].probability, 1.0f);
        EXPECT_EQ(all[0].min_count, 4);
        EXPECT_EQ(all[0].max_count, 12);
        EXPECT_EQ(all[0].lod_max,   2);
    }
    fs::remove(db_path);
}

TEST(SqliteContentPackTests, MaterialRoundTrip) {
    auto db_path = tmp_db("mw_test_mat.sqlite");
    {
        MeshWorld::SqliteContentPack pack(db_path);
        MeshWorld::MaterialRow row;
        row.id           = "grass_park";
        row.r            = 0.35f;
        row.g            = 0.60f;
        row.b            = 0.22f;
        row.roughness    = 0.8f;
        row.metallic     = 0.0f;
        row.spdx_license = "MIT";
        row.author       = "MeshWorld procedural";
        pack.upsert_material(row);
    }
    {
        MeshWorld::SqliteContentPack pack(db_path);
        auto all = pack.all_materials();
        ASSERT_EQ(all.size(), 1u);
        EXPECT_EQ(all[0].id,     "grass_park");
        EXPECT_NEAR(all[0].r,    0.35f, 0.001f);
        EXPECT_NEAR(all[0].g,    0.60f, 0.001f);
        EXPECT_NEAR(all[0].b,    0.22f, 0.001f);
        EXPECT_EQ(all[0].spdx_license, "MIT");
    }
    fs::remove(db_path);
}

TEST(SqliteContentPackTests, GetThrowsForMissing) {
    auto db_path = tmp_db("mw_test_throw.sqlite");
    MeshWorld::SqliteContentPack pack(db_path);
    EXPECT_THROW(pack.get_lua_generator("nonexistent.gen"), std::out_of_range);
    fs::remove(db_path);
}
