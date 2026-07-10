#include "WorldStore.hpp"

#include <cstdio>

#include <sqlite3.h>

#include "../Worlds/World.hpp"

namespace CnaCraft::Persistence {

namespace {
// Adapted from Craft's real schema (src/db.c): `create table if not exists
// block (p int, q int, x int, y int, z int, w int)` + a unique index on
// (p, q, x, y, z). This project has no per-chunk (p, q) addressing at the
// World level, so those two columns are dropped and the unique index is
// just on (x, y, z) directly.
constexpr const char* kCreateTableSql =
    "CREATE TABLE IF NOT EXISTS block ("
    "  x INTEGER NOT NULL,"
    "  y INTEGER NOT NULL,"
    "  z INTEGER NOT NULL,"
    "  w INTEGER NOT NULL"
    ");";
constexpr const char* kCreateIndexSql = "CREATE UNIQUE INDEX IF NOT EXISTS block_xyz_idx ON block (x, y, z);";
constexpr const char* kUpsertSql = "INSERT OR REPLACE INTO block (x, y, z, w) VALUES (?, ?, ?, ?);";
constexpr const char* kSelectAllSql = "SELECT x, y, z, w FROM block;";
}

WorldStore::WorldStore(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        std::fprintf(stderr, "WorldStore: failed to open '%s': %s\n", path.c_str(),
                     db_ ? sqlite3_errmsg(db_) : "unknown error");
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return;
    }

    char* errMsg = nullptr;
    if (sqlite3_exec(db_, kCreateTableSql, nullptr, nullptr, &errMsg) != SQLITE_OK ||
        sqlite3_exec(db_, kCreateIndexSql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::fprintf(stderr, "WorldStore: failed to create schema: %s\n", errMsg ? errMsg : "unknown error");
        sqlite3_free(errMsg);
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

WorldStore::~WorldStore() {
    if (db_) sqlite3_close(db_);
}

void WorldStore::LoadInto(Worlds::World& world) {
    if (!db_) return;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, kSelectAllSql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::fprintf(stderr, "WorldStore: failed to prepare load query: %s\n", sqlite3_errmsg(db_));
        return;
    }

    int loadedCount = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const int x = sqlite3_column_int(stmt, 0);
        const int y = sqlite3_column_int(stmt, 1);
        const int z = sqlite3_column_int(stmt, 2);
        const auto type = static_cast<Worlds::BlockType>(sqlite3_column_int(stmt, 3));
        // Plain SetBlock -- loaded edits must not be re-recorded as new
        // pending edits (see WorldStore.hpp's class comment).
        world.SetBlock(x, y, z, type);
        ++loadedCount;
    }
    sqlite3_finalize(stmt);

    if (loadedCount > 0) {
        std::printf("WorldStore: loaded %d saved block edit(s)\n", loadedCount);
        std::fflush(stdout);
    }
}

void WorldStore::SaveEdits(Worlds::World& world) {
    if (!db_) return;
    const auto& edits = world.RecordedEdits();
    if (edits.empty()) return;

    sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, kUpsertSql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::fprintf(stderr, "WorldStore: failed to prepare save statement: %s\n", sqlite3_errmsg(db_));
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return;
    }

    for (const Worlds::BlockEdit& edit : edits) {
        sqlite3_bind_int(stmt, 1, edit.x);
        sqlite3_bind_int(stmt, 2, edit.y);
        sqlite3_bind_int(stmt, 3, edit.z);
        sqlite3_bind_int(stmt, 4, static_cast<int>(edit.type));
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);

    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    world.ClearRecordedEdits();
}

}
