// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once
#include <filesystem>
#include <string>
#include <vector>

struct sqlite3;

namespace MeshWorld {

struct LuaGeneratorRow {
    std::string id;
    std::string source;
    std::string category;
    std::string version;
};

struct TaxonomyNodeRow {
    std::string id;
    std::string kind;
    std::string name;
};

struct ContainmentRuleRow {
    std::string parent;
    std::string child;
    float       probability = 1.0f;
    int         min_count   = 0;
    int         max_count   = 1;
    int         lod_max     = 0;
};

struct MaterialRow {
    std::string id;
    float       r         = 0.8f;
    float       g         = 0.8f;
    float       b         = 0.8f;
    float       roughness = 0.8f;
    float       metallic  = 0.0f;
    std::string spdx_license;
    std::string author;
};

class SqliteContentPack {
public:
    explicit SqliteContentPack(const std::filesystem::path& db_path);
    ~SqliteContentPack();

    SqliteContentPack(const SqliteContentPack&)            = delete;
    SqliteContentPack& operator=(const SqliteContentPack&) = delete;

    // lua_generator table
    void upsert_lua_generator(const LuaGeneratorRow& row);
    std::vector<LuaGeneratorRow>   all_lua_generators() const;
    LuaGeneratorRow                get_lua_generator(const std::string& id) const;
    bool                           has_lua_generator(const std::string& id) const;

    // taxonomy_node table
    void upsert_taxonomy_node(const TaxonomyNodeRow& row);
    std::vector<TaxonomyNodeRow>   all_taxonomy_nodes() const;

    // containment_rule table
    void upsert_containment_rule(const ContainmentRuleRow& row);
    std::vector<ContainmentRuleRow> all_containment_rules() const;

    // material table
    void upsert_material(const MaterialRow& row);
    std::vector<MaterialRow>       all_materials() const;

private:
    void create_tables();
    void exec(const char* sql) const;

    sqlite3* db_ = nullptr;
};

} // namespace MeshWorld
