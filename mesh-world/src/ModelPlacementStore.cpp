// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "ModelPlacementStore.hpp"

#include <sqlite3.h>

#include <filesystem>
#include <stdexcept>

namespace MeshWorld {

ModelPlacementStore::ModelPlacementStore(const std::string& world_dir, const RegionId& region)
    : region_(region) {
    path_ = region_.shard_path(world_dir);
    std::filesystem::create_directories(std::filesystem::path(path_).parent_path());

    if (sqlite3_open(path_.c_str(), &db_) != SQLITE_OK) {
        std::string msg = db_ ? sqlite3_errmsg(db_) : "out of memory";
        if (db_) { sqlite3_close(db_); db_ = nullptr; }
        throw std::runtime_error("ModelPlacementStore: cannot open DB " + path_ + ": " + msg);
    }

    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);

    // M028/M029 — per-placement table + the index query_box() (M031) needs.
    // chunk_x/chunk_z are the horizontal chunk coord (ChunkCoord::x/y, NOT
    // the region id); alt_band is the vertical analogue (alt_band_for()).
    // `id` is a synthetic primary key (map.md §10.1's own schema has one) --
    // unlike MapTileStore's `tile` table, (chunk_x, chunk_z) alone isn't
    // unique: one chunk holds many placements.
    const char* ddl =
        "CREATE TABLE IF NOT EXISTS placement ("
        "  id         INTEGER PRIMARY KEY,"
        "  chunk_x    INTEGER NOT NULL,"
        "  chunk_z    INTEGER NOT NULL,"
        "  alt_band   INTEGER NOT NULL,"
        "  y_min      REAL    NOT NULL,"
        "  y_max      REAL    NOT NULL,"
        "  pos_x      REAL    NOT NULL,"
        "  pos_y      REAL    NOT NULL,"
        "  pos_z      REAL    NOT NULL,"
        "  rot_y      REAL    NOT NULL DEFAULT 0,"
        "  scale      REAL    NOT NULL DEFAULT 1,"
        "  lod_min    INTEGER NOT NULL DEFAULT 0,"
        "  definition TEXT    NOT NULL"
        ");";
    sqlite3_exec(db_, ddl, nullptr, nullptr, nullptr);

    sqlite3_exec(db_,
                 "CREATE INDEX IF NOT EXISTS idx_place_xz_alt "
                 "ON placement (chunk_x, chunk_z, alt_band);",
                 nullptr, nullptr, nullptr);

    // M178 (optional) -- one cached compiled-scene BLOB per chunk, a
    // separate table since a chunk blob is a cached rendering artifact, not
    // placement data.
    sqlite3_exec(db_,
                 "CREATE TABLE IF NOT EXISTS chunk_blob ("
                 "  chunk_x INTEGER NOT NULL,"
                 "  chunk_z INTEGER NOT NULL,"
                 "  blob    BLOB    NOT NULL,"
                 "  PRIMARY KEY (chunk_x, chunk_z)"
                 ");",
                 nullptr, nullptr, nullptr);
}

ModelPlacementStore::~ModelPlacementStore() {
    if (db_) sqlite3_close(db_);
}

void ModelPlacementStore::insert_batch(const ChunkCoord& chunk,
                                        const std::vector<ModelPlacement>& placements) {
    if (placements.empty()) return;

    sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO placement "
        "(chunk_x, chunk_z, alt_band, y_min, y_max, pos_x, pos_y, pos_z, "
        " rot_y, scale, lod_min, definition) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        const std::string msg = sqlite3_errmsg(db_);
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        throw std::runtime_error("ModelPlacementStore: insert_batch prepare failed: " + msg);
    }

    // ChunkCoord's own second field is named `y` but represents the world Z
    // axis (see ChunkCoord::world_z()) -- chunk.y is chunk_z here, not a
    // vertical coordinate.
    for (const auto& p : placements) {
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        sqlite3_bind_int(stmt, 1, chunk.x);
        sqlite3_bind_int(stmt, 2, chunk.y);
        sqlite3_bind_int(stmt, 3, alt_band_for(p.pos_y));
        sqlite3_bind_double(stmt, 4, p.y_min);
        sqlite3_bind_double(stmt, 5, p.y_max);
        sqlite3_bind_double(stmt, 6, p.pos_x);
        sqlite3_bind_double(stmt, 7, p.pos_y);
        sqlite3_bind_double(stmt, 8, p.pos_z);
        sqlite3_bind_double(stmt, 9, p.rot_y);
        sqlite3_bind_double(stmt, 10, p.scale);
        sqlite3_bind_int(stmt, 11, p.lod_min);
        sqlite3_bind_text(stmt, 12, p.definition_id.c_str(),
                           static_cast<int>(p.definition_id.size()), SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            const std::string msg = sqlite3_errmsg(db_);
            sqlite3_finalize(stmt);
            sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
            throw std::runtime_error("ModelPlacementStore: insert_batch step failed: " + msg);
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
}

std::vector<ModelPlacement> ModelPlacementStore::query_box(const ChunkCoord& chunk_min,
                                                             const ChunkCoord& chunk_max,
                                                             int alt_band_min,
                                                             int alt_band_max) const {
    std::vector<ModelPlacement> result;

    const char* sql =
        "SELECT definition, pos_x, pos_y, pos_z, y_min, y_max, rot_y, scale, lod_min "
        "FROM placement "
        "WHERE chunk_x BETWEEN ? AND ? AND chunk_z BETWEEN ? AND ? "
        "AND alt_band BETWEEN ? AND ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        const std::string msg = sqlite3_errmsg(db_);
        throw std::runtime_error("ModelPlacementStore: query_box prepare failed: " + msg);
    }

    // ChunkCoord's own second field is named `y` but represents the world Z
    // axis -- chunk_min.y/chunk_max.y are chunk_z bounds here, not a vertical
    // range (that's alt_band_min/alt_band_max).
    sqlite3_bind_int(stmt, 1, chunk_min.x);
    sqlite3_bind_int(stmt, 2, chunk_max.x);
    sqlite3_bind_int(stmt, 3, chunk_min.y);
    sqlite3_bind_int(stmt, 4, chunk_max.y);
    sqlite3_bind_int(stmt, 5, alt_band_min);
    sqlite3_bind_int(stmt, 6, alt_band_max);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ModelPlacement p;
        const auto* def = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        p.definition_id = def ? def : "";
        p.pos_x   = sqlite3_column_double(stmt, 1);
        p.pos_y   = sqlite3_column_double(stmt, 2);
        p.pos_z   = sqlite3_column_double(stmt, 3);
        p.y_min   = sqlite3_column_double(stmt, 4);
        p.y_max   = sqlite3_column_double(stmt, 5);
        p.rot_y   = static_cast<float>(sqlite3_column_double(stmt, 6));
        p.scale   = static_cast<float>(sqlite3_column_double(stmt, 7));
        p.lod_min = sqlite3_column_int(stmt, 8);
        result.push_back(std::move(p));
    }

    sqlite3_finalize(stmt);
    return result;
}

void ModelPlacementStore::store_chunk_blob(const ChunkCoord& chunk, const std::string& blob) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO chunk_blob (chunk_x, chunk_z, blob) VALUES (?, ?, ?) "
        "ON CONFLICT(chunk_x, chunk_z) DO UPDATE SET blob = excluded.blob;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        const std::string msg = sqlite3_errmsg(db_);
        throw std::runtime_error("ModelPlacementStore: store_chunk_blob prepare failed: " + msg);
    }
    sqlite3_bind_int(stmt, 1, chunk.x);
    sqlite3_bind_int(stmt, 2, chunk.y);
    sqlite3_bind_blob(stmt, 3, blob.data(), static_cast<int>(blob.size()), SQLITE_TRANSIENT);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        const std::string msg = sqlite3_errmsg(db_);
        throw std::runtime_error("ModelPlacementStore: store_chunk_blob failed: " + msg);
    }
}

std::optional<std::string> ModelPlacementStore::load_chunk_blob(const ChunkCoord& chunk) const {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT blob FROM chunk_blob WHERE chunk_x = ? AND chunk_z = ?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        const std::string msg = sqlite3_errmsg(db_);
        throw std::runtime_error("ModelPlacementStore: load_chunk_blob prepare failed: " + msg);
    }
    sqlite3_bind_int(stmt, 1, chunk.x);
    sqlite3_bind_int(stmt, 2, chunk.y);

    std::optional<std::string> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const auto* data = reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 0));
        const int   n    = sqlite3_column_bytes(stmt, 0);
        result = std::string(data, static_cast<std::size_t>(n));
    }
    sqlite3_finalize(stmt);
    return result;
}

} // namespace MeshWorld
