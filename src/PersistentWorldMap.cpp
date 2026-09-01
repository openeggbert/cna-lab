// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "PersistentWorldMap.hpp"
#include "ProceduralWorldGen.hpp"
#include "ZoneType.hpp"
#include "RegionType.hpp"

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace MeshWorld {

PersistentWorldMap::PersistentWorldMap(const std::string& world_dir,
                                       WorldMap&          map,
                                       int                level)
    : map_(map)
{
    session_entropy_ = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());

    std::filesystem::create_directories(world_dir);
    std::string db_path = world_dir + "/map_level" + std::to_string(level) + ".db";

    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK)
        throw std::runtime_error("PersistentWorldMap: cannot open DB: " + db_path);

    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);

    const char* ddl =
        "CREATE TABLE IF NOT EXISTS chunks ("
        "  x      INTEGER NOT NULL,"
        "  y      INTEGER NOT NULL,"
        "  zone   TEXT    NOT NULL,"
        "  region TEXT    NOT NULL,"
        "  PRIMARY KEY (x, y)"
        ");";
    sqlite3_exec(db_, ddl, nullptr, nullptr, nullptr);
}

PersistentWorldMap::~PersistentWorldMap() {
    if (db_) sqlite3_close(db_);
}

ChunkInfo PersistentWorldMap::ensure_chunk(int x, int y) {
    if (!map_.in_bounds(x, y)) return {};
    ChunkCoord cc{x, y};
    if (populated_.count(cc)) return map_.info(x, y);

    ChunkInfo info;
    if (!db_load(x, y, info)) {
        info = generate_new(x, y);
        db_store(x, y, info);
    }
    map_.set_info(x, y, info);
    populated_.insert(cc);
    return info;
}

void PersistentWorldMap::ensure_region(int cx, int cy, int radius) {
    sqlite3_exec(db_, "BEGIN;", nullptr, nullptr, nullptr);
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy > radius * radius) continue;
            ensure_chunk(cx + dx, cy + dy);
        }
    }
    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
}

bool PersistentWorldMap::db_load(int x, int y, ChunkInfo& out) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT zone, region FROM chunks WHERE x=? AND y=?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, x);
    sqlite3_bind_int(stmt, 2, y);
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* zs = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* rs = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        try {
            out.zone   = zone_from_string(zs   ? zs : "empty");
            out.region = region_from_string(rs ? rs : "empty");
            found = true;
        } catch (...) {}
    }
    sqlite3_finalize(stmt);
    return found;
}

void PersistentWorldMap::db_store(int x, int y, const ChunkInfo& info) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT OR REPLACE INTO chunks (x, y, zone, region) VALUES (?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_int(stmt, 1, x);
    sqlite3_bind_int(stmt, 2, y);
    sqlite3_bind_text(stmt, 3, to_string(info.zone).c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, to_string(info.region).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

ChunkInfo PersistentWorldMap::generate_new(int x, int y) const {
    // Mix session entropy with position — result differs each session.
    // Region variation is per chunk, whereas the biome is intentionally
    // shared by a small planning block. The old per-chunk biome seed made
    // neighbouring city cells statistically unrelated, so no real street
    // grid could exist for composer parcels to face.
    const auto mixed_entropy = [this](int px, int py) {
        uint64_t value = session_entropy_
            ^ (static_cast<uint64_t>(static_cast<uint32_t>(px)) * 2654435761ULL)
            ^ (static_cast<uint64_t>(static_cast<uint32_t>(py)) * 2246822519ULL);
        value ^= value >> 33;
        value *= 0xff51afd7ed558ccdULL;
        value ^= value >> 33;
        return value;
    };
    const uint64_t h = mixed_entropy(x, y);
    constexpr int kCityPlanningBlockSize = 4;
    const uint64_t zone_entropy = mixed_entropy(x / kCityPlanningBlockSize,
                                                y / kCityPlanningBlockSize);

    ProceduralWorldGen gen(zone_entropy, /*cell_size=*/8);
    ChunkInfo info;
    info.zone   = gen.zone_at(/*x=*/0, /*y=*/0);
    info.region = default_region_for_zone(info.zone, x, y, h);
    return info;
}

RegionType PersistentWorldMap::default_region_for_zone(ZoneType z, int x, int y,
                                                        std::uint64_t variation) {
    switch (z) {
        case ZoneType::city: {
            // Persistent worlds are intentionally non-reproducible only at
            // first visit, then stored in SQLite. Give each new city a small,
            // coordinate-stable street grid before selecting varied parcels;
            // this supplies the real road adjacency BuildingComposer needs.
            // Existing database rows are loaded unchanged.
            const int grid_x = x % 4;
            const int grid_y = y % 4;
            if (grid_x == 0 && grid_y == 0) return RegionType::crossroad;
            if (grid_x == 0 || grid_y == 0) return RegionType::road;

            switch (variation % 10U) {
                case 0: return RegionType::square;
                case 1:
                case 2: return RegionType::shop_street;
                case 3:
                case 4:
                case 5: return RegionType::apartment_block;
                default: return RegionType::small_house_block;
            }
        }
        case ZoneType::ocean: return RegionType::water;
        case ZoneType::cave:  return RegionType::cave_chamber;
        default:              return RegionType::open;
    }
}

} // namespace MeshWorld
