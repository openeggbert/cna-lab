// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Map/MapTileStore.hpp"

#include <sqlite3.h>

#include <cstddef>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <string>

#include "Map/MapPayloadCodec.hpp"

namespace MeshWorld::Map {

namespace {
// Serializes construction (specifically the CREATE TABLE IF NOT EXISTS
// below) across threads: WorldStreamer's map-layer wiring gives each worker
// thread its own MapTileStore, and several can target the same
// map_level{z}.db file concurrently on first touch. Without this, one
// connection's query can run before another connection's CREATE TABLE has
// committed, throwing "no such table: tile" — a real race caught by
// WorldStreamerTests.cpp's WithMapLayerLoadsChunksWithoutCrashing. A single
// process-wide mutex is enough: construction is a rare, one-time-per-level
// event, not a hot path.
std::mutex g_construction_mutex;
} // namespace

MapTileStore::MapTileStore(const std::string& world_dir, int level)
    : level_(level)
{
    std::lock_guard<std::mutex> construction_lock(g_construction_mutex);
    std::filesystem::create_directories(world_dir);
    path_ = world_dir + "/map_level" + std::to_string(level) + ".db";

    if (sqlite3_open(path_.c_str(), &db_) != SQLITE_OK) {
        std::string msg = db_ ? sqlite3_errmsg(db_) : "out of memory";
        if (db_) { sqlite3_close(db_); db_ = nullptr; }
        throw std::runtime_error("MapTileStore: cannot open DB " + path_ + ": " + msg);
    }

    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    // WAL allows multiple connections (one per WorldStreamer worker thread,
    // see g_construction_mutex's own comment above) to read/write the same
    // file, but two connections' writes can still collide -- without a busy
    // timeout, the loser gets SQLITE_BUSY ("database is locked") immediately
    // instead of waiting; store()/load()/has() would then throw under
    // concurrent access. 5 s comfortably covers a single tile write.
    sqlite3_busy_timeout(db_, 5000);

    // M019 — per-tile payload table. (x, y) identifies the tile within this
    // level's DB; the level itself is implied by the file. payload holds the
    // serialized MapTilePayload (JSON via MapPayloadCodec, M023/M024).
    const char* ddl =
        "CREATE TABLE IF NOT EXISTS tile ("
        "  x       INTEGER NOT NULL,"
        "  y       INTEGER NOT NULL,"
        "  payload TEXT,"
        "  PRIMARY KEY (x, y)"
        ");";
    sqlite3_exec(db_, ddl, nullptr, nullptr, nullptr);
}

MapTileStore::~MapTileStore() {
    if (db_) sqlite3_close(db_);
}

namespace {

// Prepare a statement or throw with the sqlite error message.
sqlite3_stmt* prepare_or_throw(sqlite3* db, const char* sql) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        const std::string msg = sqlite3_errmsg(db);
        throw std::runtime_error("MapTileStore: prepare failed: " + msg);
    }
    return stmt;
}

// The tile's level must match the store's level — see header.
void require_level(int store_level, const TileCoord& tile) {
    if (tile.level != store_level)
        throw std::invalid_argument(
            "MapTileStore: tile level " + std::to_string(tile.level) +
            " does not match store level " + std::to_string(store_level));
}

} // namespace

void MapTileStore::store(const TileCoord& tile, const MapTilePayload& payload) {
    require_level(level_, tile);

    const std::string text = MapPayloadCodec::encode(payload);

    sqlite3_stmt* stmt = prepare_or_throw(
        db_,
        "INSERT INTO tile (x, y, payload) VALUES (?, ?, ?) "
        "ON CONFLICT(x, y) DO UPDATE SET payload = excluded.payload;");
    sqlite3_bind_int64(stmt, 1, tile.x);
    sqlite3_bind_int64(stmt, 2, tile.y);
    sqlite3_bind_text(stmt, 3, text.c_str(), static_cast<int>(text.size()), SQLITE_TRANSIENT);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        const std::string msg = sqlite3_errmsg(db_);
        throw std::runtime_error("MapTileStore: store failed: " + msg);
    }
}

std::optional<MapTilePayload> MapTileStore::load(const TileCoord& tile) const {
    require_level(level_, tile);

    sqlite3_stmt* stmt = prepare_or_throw(
        db_, "SELECT payload FROM tile WHERE x = ? AND y = ?;");
    sqlite3_bind_int64(stmt, 1, tile.x);
    sqlite3_bind_int64(stmt, 2, tile.y);

    std::optional<MapTilePayload> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const auto* txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const int   n   = sqlite3_column_bytes(stmt, 0);
        result = MapPayloadCodec::decode(std::string(txt, static_cast<std::size_t>(n)));
    }
    sqlite3_finalize(stmt);
    return result;
}

bool MapTileStore::has(const TileCoord& tile) const {
    require_level(level_, tile);

    sqlite3_stmt* stmt = prepare_or_throw(
        db_, "SELECT 1 FROM tile WHERE x = ? AND y = ? LIMIT 1;");
    sqlite3_bind_int64(stmt, 1, tile.x);
    sqlite3_bind_int64(stmt, 2, tile.y);

    const bool found = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return found;
}

std::vector<TileCoord> MapTileStore::list_tiles() const {
    sqlite3_stmt* stmt = prepare_or_throw(db_, "SELECT x, y FROM tile;");

    std::vector<TileCoord> tiles;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const int64_t x = sqlite3_column_int64(stmt, 0);
        const int64_t y = sqlite3_column_int64(stmt, 1);
        tiles.push_back(TileCoord{level_, x, y});
    }
    sqlite3_finalize(stmt);
    return tiles;
}

} // namespace MeshWorld::Map
