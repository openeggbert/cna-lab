#include "WorldStore.hpp"

#include <cstdio>
#include <utility>
#include <vector>

#include <sqlite3.h>

#include "../Worlds/Sign.hpp"
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

// Signs (CRAFT_PARITY.md §4.3) — adapted from Craft's real
// `sign(p,q,x,y,z,face,text)` schema (src/db.c), same p,q-column drop as
// `block` above.
constexpr const char* kCreateSignTableSql =
    "CREATE TABLE IF NOT EXISTS sign ("
    "  x INTEGER NOT NULL,"
    "  y INTEGER NOT NULL,"
    "  z INTEGER NOT NULL,"
    "  face INTEGER NOT NULL,"
    "  text TEXT NOT NULL"
    ");";
constexpr const char* kCreateSignIndexSql =
    "CREATE UNIQUE INDEX IF NOT EXISTS sign_xyzface_idx ON sign (x, y, z, face);";
constexpr const char* kDeleteAllSignsSql = "DELETE FROM sign;";
constexpr const char* kInsertSignSql = "INSERT INTO sign (x, y, z, face, text) VALUES (?, ?, ?, ?, ?);";
constexpr const char* kSelectAllSignsSql = "SELECT x, y, z, face, text FROM sign;";
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
        sqlite3_exec(db_, kCreateIndexSql, nullptr, nullptr, &errMsg) != SQLITE_OK ||
        sqlite3_exec(db_, kCreateSignTableSql, nullptr, nullptr, &errMsg) != SQLITE_OK ||
        sqlite3_exec(db_, kCreateSignIndexSql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
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

void WorldStore::LoadSignsInto(Worlds::SignStore& store) {
    if (!db_) return;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, kSelectAllSignsSql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::fprintf(stderr, "WorldStore: failed to prepare sign load query: %s\n", sqlite3_errmsg(db_));
        return;
    }

    std::vector<Worlds::Sign> signs;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Worlds::Sign sign;
        sign.x = sqlite3_column_int(stmt, 0);
        sign.y = sqlite3_column_int(stmt, 1);
        sign.z = sqlite3_column_int(stmt, 2);
        sign.face = sqlite3_column_int(stmt, 3);
        const unsigned char* text = sqlite3_column_text(stmt, 4);
        sign.text = text ? reinterpret_cast<const char*>(text) : "";
        signs.push_back(std::move(sign));
    }
    sqlite3_finalize(stmt);

    if (!signs.empty()) {
        std::printf("WorldStore: loaded %d saved sign(s)\n", static_cast<int>(signs.size()));
        std::fflush(stdout);
    }
    store.ReplaceAll(std::move(signs));
}

void WorldStore::SaveSigns(const Worlds::SignStore& store) {
    if (!db_) return;

    sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, kDeleteAllSignsSql, nullptr, nullptr, nullptr);

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, kInsertSignSql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::fprintf(stderr, "WorldStore: failed to prepare sign save statement: %s\n", sqlite3_errmsg(db_));
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return;
    }

    for (const Worlds::Sign& sign : store.Signs()) {
        sqlite3_bind_int(stmt, 1, sign.x);
        sqlite3_bind_int(stmt, 2, sign.y);
        sqlite3_bind_int(stmt, 3, sign.z);
        sqlite3_bind_int(stmt, 4, sign.face);
        sqlite3_bind_text(stmt, 5, sign.text.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);

    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
}

}
