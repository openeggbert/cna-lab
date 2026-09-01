// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#include "SqliteContentPack.hpp"
#include <sqlite3.h>
#include <stdexcept>
#include <string>

namespace MeshWorld {

namespace {

void check(int rc, const char* ctx) {
    if (rc != SQLITE_OK && rc != SQLITE_ROW && rc != SQLITE_DONE)
        throw std::runtime_error(std::string("SQLite error in ") + ctx +
                                 ": " + sqlite3_errstr(rc));
}

} // namespace

SqliteContentPack::SqliteContentPack(const std::filesystem::path& db_path) {
    int rc = sqlite3_open(db_path.string().c_str(), &db_);
    if (rc != SQLITE_OK || !db_)
        throw std::runtime_error("SqliteContentPack: cannot open " + db_path.string());
    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA foreign_keys=ON;");
    create_tables();
}

SqliteContentPack::~SqliteContentPack() {
    if (db_) sqlite3_close(db_);
}

void SqliteContentPack::exec(const char* sql) const {
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string msg = err ? err : "unknown error";
        sqlite3_free(err);
        throw std::runtime_error(std::string("SqliteContentPack::exec: ") + msg);
    }
}

void SqliteContentPack::create_tables() {
    exec(R"sql(
        CREATE TABLE IF NOT EXISTS lua_generator (
            id       TEXT PRIMARY KEY,
            source   TEXT NOT NULL,
            category TEXT NOT NULL DEFAULT '',
            version  TEXT NOT NULL DEFAULT ''
        );
    )sql");
    exec(R"sql(
        CREATE TABLE IF NOT EXISTS taxonomy_node (
            id   TEXT PRIMARY KEY,
            kind TEXT NOT NULL,
            name TEXT NOT NULL
        );
    )sql");
    exec(R"sql(
        CREATE TABLE IF NOT EXISTS containment_rule (
            parent      TEXT NOT NULL,
            child       TEXT NOT NULL,
            probability REAL NOT NULL DEFAULT 1.0,
            min_count   INTEGER NOT NULL DEFAULT 0,
            max_count   INTEGER NOT NULL DEFAULT 1,
            lod_max     INTEGER NOT NULL DEFAULT 0,
            PRIMARY KEY (parent, child)
        );
    )sql");
    exec(R"sql(
        CREATE TABLE IF NOT EXISTS material (
            id           TEXT PRIMARY KEY,
            r            REAL NOT NULL DEFAULT 0.8,
            g            REAL NOT NULL DEFAULT 0.8,
            b            REAL NOT NULL DEFAULT 0.8,
            roughness    REAL NOT NULL DEFAULT 0.8,
            metallic     REAL NOT NULL DEFAULT 0.0,
            spdx_license TEXT NOT NULL DEFAULT '',
            author       TEXT NOT NULL DEFAULT ''
        );
    )sql");
}

// ── lua_generator ──────────────────────────────────────────────────────────

void SqliteContentPack::upsert_lua_generator(const LuaGeneratorRow& row) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT OR REPLACE INTO lua_generator(id,source,category,version) "
        "VALUES(?,?,?,?)";
    check(sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr), "prepare upsert_lua_generator");
    sqlite3_bind_text(stmt, 1, row.id.c_str(),       -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, row.source.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, row.category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, row.version.c_str(),  -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        throw std::runtime_error("upsert_lua_generator step failed: " + std::string(sqlite3_errmsg(db_)));
}

std::vector<LuaGeneratorRow> SqliteContentPack::all_lua_generators() const {
    sqlite3_stmt* stmt = nullptr;
    check(sqlite3_prepare_v2(db_,
        "SELECT id,source,category,version FROM lua_generator",
        -1, &stmt, nullptr), "prepare all_lua_generators");
    std::vector<LuaGeneratorRow> result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        LuaGeneratorRow r;
        r.id       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        r.source   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        r.category = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        r.version  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        result.push_back(std::move(r));
    }
    sqlite3_finalize(stmt);
    return result;
}

LuaGeneratorRow SqliteContentPack::get_lua_generator(const std::string& id) const {
    sqlite3_stmt* stmt = nullptr;
    check(sqlite3_prepare_v2(db_,
        "SELECT id,source,category,version FROM lua_generator WHERE id=?",
        -1, &stmt, nullptr), "prepare get_lua_generator");
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        throw std::out_of_range("SqliteContentPack: lua_generator '" + id + "' not found");
    }
    LuaGeneratorRow r;
    r.id       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    r.source   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    r.category = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    r.version  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    sqlite3_finalize(stmt);
    return r;
}

bool SqliteContentPack::has_lua_generator(const std::string& id) const {
    sqlite3_stmt* stmt = nullptr;
    check(sqlite3_prepare_v2(db_,
        "SELECT 1 FROM lua_generator WHERE id=? LIMIT 1",
        -1, &stmt, nullptr), "prepare has_lua_generator");
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return found;
}

// ── taxonomy_node ──────────────────────────────────────────────────────────

void SqliteContentPack::upsert_taxonomy_node(const TaxonomyNodeRow& row) {
    sqlite3_stmt* stmt = nullptr;
    check(sqlite3_prepare_v2(db_,
        "INSERT OR REPLACE INTO taxonomy_node(id,kind,name) VALUES(?,?,?)",
        -1, &stmt, nullptr), "prepare upsert_taxonomy_node");
    sqlite3_bind_text(stmt, 1, row.id.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, row.kind.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, row.name.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        throw std::runtime_error("upsert_taxonomy_node step failed");
}

std::vector<TaxonomyNodeRow> SqliteContentPack::all_taxonomy_nodes() const {
    sqlite3_stmt* stmt = nullptr;
    check(sqlite3_prepare_v2(db_,
        "SELECT id,kind,name FROM taxonomy_node",
        -1, &stmt, nullptr), "prepare all_taxonomy_nodes");
    std::vector<TaxonomyNodeRow> result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        TaxonomyNodeRow r;
        r.id   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        r.kind = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        r.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        result.push_back(std::move(r));
    }
    sqlite3_finalize(stmt);
    return result;
}

// ── containment_rule ───────────────────────────────────────────────────────

void SqliteContentPack::upsert_containment_rule(const ContainmentRuleRow& row) {
    sqlite3_stmt* stmt = nullptr;
    check(sqlite3_prepare_v2(db_,
        "INSERT OR REPLACE INTO containment_rule"
        "(parent,child,probability,min_count,max_count,lod_max) VALUES(?,?,?,?,?,?)",
        -1, &stmt, nullptr), "prepare upsert_containment_rule");
    sqlite3_bind_text(stmt, 1, row.parent.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, row.child.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, row.probability);
    sqlite3_bind_int(stmt, 4, row.min_count);
    sqlite3_bind_int(stmt, 5, row.max_count);
    sqlite3_bind_int(stmt, 6, row.lod_max);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        throw std::runtime_error("upsert_containment_rule step failed");
}

std::vector<ContainmentRuleRow> SqliteContentPack::all_containment_rules() const {
    sqlite3_stmt* stmt = nullptr;
    check(sqlite3_prepare_v2(db_,
        "SELECT parent,child,probability,min_count,max_count,lod_max FROM containment_rule",
        -1, &stmt, nullptr), "prepare all_containment_rules");
    std::vector<ContainmentRuleRow> result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ContainmentRuleRow r;
        r.parent      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        r.child       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        r.probability = static_cast<float>(sqlite3_column_double(stmt, 2));
        r.min_count   = sqlite3_column_int(stmt, 3);
        r.max_count   = sqlite3_column_int(stmt, 4);
        r.lod_max     = sqlite3_column_int(stmt, 5);
        result.push_back(std::move(r));
    }
    sqlite3_finalize(stmt);
    return result;
}

// ── material ───────────────────────────────────────────────────────────────

void SqliteContentPack::upsert_material(const MaterialRow& row) {
    sqlite3_stmt* stmt = nullptr;
    check(sqlite3_prepare_v2(db_,
        "INSERT OR REPLACE INTO material"
        "(id,r,g,b,roughness,metallic,spdx_license,author) VALUES(?,?,?,?,?,?,?,?)",
        -1, &stmt, nullptr), "prepare upsert_material");
    sqlite3_bind_text(stmt, 1, row.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, row.r);
    sqlite3_bind_double(stmt, 3, row.g);
    sqlite3_bind_double(stmt, 4, row.b);
    sqlite3_bind_double(stmt, 5, row.roughness);
    sqlite3_bind_double(stmt, 6, row.metallic);
    sqlite3_bind_text(stmt, 7, row.spdx_license.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, row.author.c_str(),       -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        throw std::runtime_error("upsert_material step failed");
}

std::vector<MaterialRow> SqliteContentPack::all_materials() const {
    sqlite3_stmt* stmt = nullptr;
    check(sqlite3_prepare_v2(db_,
        "SELECT id,r,g,b,roughness,metallic,spdx_license,author FROM material",
        -1, &stmt, nullptr), "prepare all_materials");
    std::vector<MaterialRow> result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MaterialRow r;
        r.id           = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        r.r            = static_cast<float>(sqlite3_column_double(stmt, 1));
        r.g            = static_cast<float>(sqlite3_column_double(stmt, 2));
        r.b            = static_cast<float>(sqlite3_column_double(stmt, 3));
        r.roughness    = static_cast<float>(sqlite3_column_double(stmt, 4));
        r.metallic     = static_cast<float>(sqlite3_column_double(stmt, 5));
        r.spdx_license = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        r.author       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        result.push_back(std::move(r));
    }
    sqlite3_finalize(stmt);
    return result;
}

} // namespace MeshWorld
