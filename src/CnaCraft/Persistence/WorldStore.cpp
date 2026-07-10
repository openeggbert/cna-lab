#include "WorldStore.hpp"

#include <cstdio>
#include <utility>
#include <vector>

#include <sqlite3.h>

#include "../Worlds/Sign.hpp"
#include "../Worlds/World.hpp"

namespace CnaCraft::Persistence {

namespace {
// Matches Craft's real schema exactly (src/db.c: `create table if not
// exists block (p int, q int, x int, y int, z int, w int)`, unique index on
// (p, q, x, y, z)) — plan.md §12.1 item 19, user decision 2026-07-10: add
// the p,q chunk-address columns this project's earlier fixed-size world had
// no use for, now that the world streams by chunk column. p,q are
// redundant with x,z (always `ChunkCoordOf(x)`/`ChunkCoordOf(z)`, see
// Chunk.hpp) but stored anyway, matching Craft exactly, so `WHERE p=? AND
// q=?` can use the leading columns of this one composite index for a fast
// per-chunk-column load query without a second index (see kUpsertSql).
//
// Requires deleting any pre-existing `world.db` on upgrade (same precedent
// as the terrain-formula session) -- an old-schema `block`/`sign` table
// missing p,q lacks the columns CREATE INDEX below references, so schema
// creation itself fails loudly once at startup (WorldStore becomes the
// same harmless no-op store as any other open failure) rather than each
// per-edit INSERT silently failing one at a time.
constexpr const char* kCreateTableSql =
    "CREATE TABLE IF NOT EXISTS block ("
    "  p INTEGER NOT NULL,"
    "  q INTEGER NOT NULL,"
    "  x INTEGER NOT NULL,"
    "  y INTEGER NOT NULL,"
    "  z INTEGER NOT NULL,"
    "  w INTEGER NOT NULL"
    ");";
constexpr const char* kCreateIndexSql =
    "CREATE UNIQUE INDEX IF NOT EXISTS block_pqxyz_idx ON block (p, q, x, y, z);";
constexpr const char* kUpsertSql = "INSERT OR REPLACE INTO block (p, q, x, y, z, w) VALUES (?, ?, ?, ?, ?, ?);";
constexpr const char* kSelectAllSql = "SELECT x, y, z, w FROM block;";
constexpr const char* kSelectColumnSql = "SELECT x, y, z, w FROM block WHERE p = ? AND q = ?;";

// Signs (CRAFT_PARITY.md §4.3) — matches Craft's real
// `sign(p,q,x,y,z,face,text)` schema (src/db.c) exactly, including its
// asymmetry vs. `block` above: uniqueness stays on (x,y,z,face) (a sign's
// real-world identity is "one sign per block face," independent of which
// chunk that face happens to fall in), with p,q only backing a separate
// non-unique index for the fast per-chunk load query -- exactly matching
// Craft's own separate `sign_pq_idx`, not folded into the unique index the
// way `block`'s is.
constexpr const char* kCreateSignTableSql =
    "CREATE TABLE IF NOT EXISTS sign ("
    "  p INTEGER NOT NULL,"
    "  q INTEGER NOT NULL,"
    "  x INTEGER NOT NULL,"
    "  y INTEGER NOT NULL,"
    "  z INTEGER NOT NULL,"
    "  face INTEGER NOT NULL,"
    "  text TEXT NOT NULL"
    ");";
constexpr const char* kCreateSignIndexSql =
    "CREATE UNIQUE INDEX IF NOT EXISTS sign_xyzface_idx ON sign (x, y, z, face);";
constexpr const char* kCreateSignPqIndexSql = "CREATE INDEX IF NOT EXISTS sign_pq_idx ON sign (p, q);";
constexpr const char* kSelectAllSignsSql = "SELECT x, y, z, face, text FROM sign;";
constexpr const char* kSelectColumnSignsSql = "SELECT x, y, z, face, text FROM sign WHERE p = ? AND q = ?;";
// Incremental sign writes (mirrors Craft's db_insert_sign/db_delete_sign/
// db_delete_signs, src/db.c) — see WorldStore.hpp's class comment. Delete
// statements don't bind p,q, matching Craft's own db_delete_sign/
// db_delete_signs exactly -- (x,y,z[,face]) alone already identifies the
// row(s) uniquely, p,q would be redundant there.
constexpr const char* kUpsertSignSql =
    "INSERT OR REPLACE INTO sign (p, q, x, y, z, face, text) VALUES (?, ?, ?, ?, ?, ?, ?);";
constexpr const char* kDeleteSignSql = "DELETE FROM sign WHERE x = ? AND y = ? AND z = ? AND face = ?;";
constexpr const char* kDeleteSignsAtSql = "DELETE FROM sign WHERE x = ? AND y = ? AND z = ?;";

// Player-position persistence (CRAFT_PARITY.md §4.1, plan.md §12.1 item 17
// follow-up, user decision 2026-07-10) -- matches Craft's real single-row
// `state(x,y,z,rx,ry)` table exactly (src/db.c): x,y,z is the player's
// real EYE position (Craft has no separate feet/eye split, unlike
// PlayerController's own feet-based storage -- see WorldStore.hpp's
// SavePlayerState/LoadPlayerState doc comments for the conversion), rx/ry
// are yaw/pitch in radians. `DELETE FROM state` then `INSERT` (not
// `INSERT OR REPLACE` with a unique key) matches Craft's own
// db_save_state exactly -- there is no primary key to replace on, the
// table is just always emptied and re-filled with the one current row.
constexpr const char* kCreateStateTableSql =
    "CREATE TABLE IF NOT EXISTS state ("
    "  x REAL NOT NULL,"
    "  y REAL NOT NULL,"
    "  z REAL NOT NULL,"
    "  rx REAL NOT NULL,"
    "  ry REAL NOT NULL"
    ");";
constexpr const char* kDeleteStateSql = "DELETE FROM state;";
constexpr const char* kInsertStateSql = "INSERT INTO state (x, y, z, rx, ry) VALUES (?, ?, ?, ?, ?);";
constexpr const char* kSelectStateSql = "SELECT x, y, z, rx, ry FROM state;";
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
        sqlite3_exec(db_, kCreateSignIndexSql, nullptr, nullptr, &errMsg) != SQLITE_OK ||
        sqlite3_exec(db_, kCreateSignPqIndexSql, nullptr, nullptr, &errMsg) != SQLITE_OK ||
        sqlite3_exec(db_, kCreateStateTableSql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
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

void WorldStore::LoadColumnInto(Worlds::World& world, int cx, int cz) {
    // Plain SetBlock -- loaded edits must not be re-recorded as new pending
    // edits (see WorldStore.hpp's class comment).
    for (const Worlds::BlockEdit& edit : LoadColumnEdits(cx, cz)) {
        world.SetBlock(edit.x, edit.y, edit.z, edit.type);
    }
}

std::vector<Worlds::BlockEdit> WorldStore::LoadColumnEdits(int cx, int cz) {
    std::vector<Worlds::BlockEdit> edits;
    if (!db_) return edits;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, kSelectColumnSql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::fprintf(stderr, "WorldStore: failed to prepare column load query: %s\n", sqlite3_errmsg(db_));
        return edits;
    }
    sqlite3_bind_int(stmt, 1, cx);
    sqlite3_bind_int(stmt, 2, cz);

    // No "loaded N edit(s)" print here, unlike LoadInto -- once
    // CnaCraftGame actually streams columns on demand this fires once per
    // newly-loaded column (potentially many per second while flying), not
    // once at startup; per-column load noise isn't worth logging.
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Worlds::BlockEdit edit;
        edit.x = sqlite3_column_int(stmt, 0);
        edit.y = sqlite3_column_int(stmt, 1);
        edit.z = sqlite3_column_int(stmt, 2);
        edit.type = static_cast<Worlds::BlockType>(sqlite3_column_int(stmt, 3));
        edits.push_back(edit);
    }
    sqlite3_finalize(stmt);
    return edits;
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
        sqlite3_bind_int(stmt, 1, Worlds::ChunkCoordOf(edit.x));
        sqlite3_bind_int(stmt, 2, Worlds::ChunkCoordOf(edit.z));
        sqlite3_bind_int(stmt, 3, edit.x);
        sqlite3_bind_int(stmt, 4, edit.y);
        sqlite3_bind_int(stmt, 5, edit.z);
        sqlite3_bind_int(stmt, 6, static_cast<int>(edit.type));
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

void WorldStore::LoadColumnSignsInto(Worlds::SignStore& store, int cx, int cz) {
    if (!db_) return;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, kSelectColumnSignsSql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::fprintf(stderr, "WorldStore: failed to prepare column sign load query: %s\n", sqlite3_errmsg(db_));
        return;
    }
    sqlite3_bind_int(stmt, 1, cx);
    sqlite3_bind_int(stmt, 2, cz);

    // PlaceSign, not ReplaceAll -- adds this column's signs to whatever's
    // already loaded (other columns), rather than wiping them (see
    // WorldStore.hpp's doc comment).
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const int x = sqlite3_column_int(stmt, 0);
        const int y = sqlite3_column_int(stmt, 1);
        const int z = sqlite3_column_int(stmt, 2);
        const int face = sqlite3_column_int(stmt, 3);
        const unsigned char* text = sqlite3_column_text(stmt, 4);
        store.PlaceSign(x, y, z, face, text ? reinterpret_cast<const char*>(text) : "");
    }
    sqlite3_finalize(stmt);
}

void WorldStore::UpsertSign(const Worlds::Sign& sign) {
    if (!db_) return;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, kUpsertSignSql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::fprintf(stderr, "WorldStore: failed to prepare sign upsert statement: %s\n", sqlite3_errmsg(db_));
        return;
    }
    sqlite3_bind_int(stmt, 1, Worlds::ChunkCoordOf(sign.x));
    sqlite3_bind_int(stmt, 2, Worlds::ChunkCoordOf(sign.z));
    sqlite3_bind_int(stmt, 3, sign.x);
    sqlite3_bind_int(stmt, 4, sign.y);
    sqlite3_bind_int(stmt, 5, sign.z);
    sqlite3_bind_int(stmt, 6, sign.face);
    sqlite3_bind_text(stmt, 7, sign.text.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void WorldStore::DeleteSign(int x, int y, int z, int face) {
    if (!db_) return;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, kDeleteSignSql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::fprintf(stderr, "WorldStore: failed to prepare sign delete statement: %s\n", sqlite3_errmsg(db_));
        return;
    }
    sqlite3_bind_int(stmt, 1, x);
    sqlite3_bind_int(stmt, 2, y);
    sqlite3_bind_int(stmt, 3, z);
    sqlite3_bind_int(stmt, 4, face);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void WorldStore::DeleteSignsAt(int x, int y, int z) {
    if (!db_) return;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, kDeleteSignsAtSql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::fprintf(stderr, "WorldStore: failed to prepare sign delete-all-at statement: %s\n",
                     sqlite3_errmsg(db_));
        return;
    }
    sqlite3_bind_int(stmt, 1, x);
    sqlite3_bind_int(stmt, 2, y);
    sqlite3_bind_int(stmt, 3, z);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void WorldStore::SavePlayerState(float x, float y, float z, float rx, float ry) {
    if (!db_) return;

    // DELETE-then-INSERT, not INSERT OR REPLACE -- matches Craft's own
    // db_save_state exactly (src/db.c), since this single-row table has no
    // key to replace on.
    sqlite3_exec(db_, kDeleteStateSql, nullptr, nullptr, nullptr);

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, kInsertStateSql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::fprintf(stderr, "WorldStore: failed to prepare state insert statement: %s\n", sqlite3_errmsg(db_));
        return;
    }
    sqlite3_bind_double(stmt, 1, static_cast<double>(x));
    sqlite3_bind_double(stmt, 2, static_cast<double>(y));
    sqlite3_bind_double(stmt, 3, static_cast<double>(z));
    sqlite3_bind_double(stmt, 4, static_cast<double>(rx));
    sqlite3_bind_double(stmt, 5, static_cast<double>(ry));
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

bool WorldStore::LoadPlayerState(float& x, float& y, float& z, float& rx, float& ry) {
    if (!db_) return false;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, kSelectStateSql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::fprintf(stderr, "WorldStore: failed to prepare state select statement: %s\n", sqlite3_errmsg(db_));
        return false;
    }
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        x = static_cast<float>(sqlite3_column_double(stmt, 0));
        y = static_cast<float>(sqlite3_column_double(stmt, 1));
        z = static_cast<float>(sqlite3_column_double(stmt, 2));
        rx = static_cast<float>(sqlite3_column_double(stmt, 3));
        ry = static_cast<float>(sqlite3_column_double(stmt, 4));
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

}
