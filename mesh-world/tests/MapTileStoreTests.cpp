// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP2 tests. M018: open/create lifecycle. M019: tile schema.
// M020/M021/M034: store/load/has CRUD + round-trip.

#include <gtest/gtest.h>
#include <sqlite3.h>
#include <chrono>
#include <filesystem>
#include <thread>
#include "Map/MapPayloadCodec.hpp"
#include "Map/MapTilePayload.hpp"
#include "Map/MapTileStore.hpp"

// M224 (MAP14) — crash-safety test needs to abruptly kill a real process
// mid-write (fork()/SIGKILL), the only way to genuinely interrupt an
// in-flight SQLite write rather than just simulating one. POSIX-only,
// intentionally: this project has no existing platform-specific code, and
// introducing it for one test is only worth it on the platform this project
// actually develops/CI's on (Linux) -- gated out entirely (not even
// compiled) elsewhere rather than partially working.
#if defined(__unix__) || defined(__APPLE__)
#define MESH_WORLD_HAS_FORK_KILL_TEST 1
#include <sys/wait.h>
#include <unistd.h>
#endif

using namespace MeshWorld::Map;
namespace fs = std::filesystem;

namespace {

// A small but non-trivial payload anchored at the given tile.
MapTilePayload make_payload(const TileCoord& tile) {
    MapTilePayload p;
    p.tile      = tile;
    p.entropy   = 0xABCDEF0123456789ULL ^ static_cast<std::uint64_t>(tile.x * 31 + tile.y);
    p.culture   = "test-culture";
    p.generator = "test-gen";
    p.elevation = FieldGrid{2, 2, {1.0f, 2.0f, 3.0f, 4.0f}};
    p.biome     = BiomeGrid{2, 2, {0, 1, 2, 3}};

    MapFeature city;
    city.type = FeatureType::City;
    city.name = "Testville";
    city.points = {{static_cast<double>(tile.x), static_cast<double>(tile.y)}};
    p.features.push_back(city);
    p.labels.push_back(PlaceLabel{"Testville", {{0.0, 0.0}}, "city"});
    p.edges[0].elevation = {1.0f, 2.0f};
    return p;
}

#ifdef MESH_WORLD_HAS_FORK_KILL_TEST
// M224 — brand-new, independent, read-only connection; returns the single
// "ok" row PRAGMA integrity_check reports, or the first problem line found.
std::string sqlite_integrity_check(const std::string& path) {
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        const std::string err = db ? sqlite3_errmsg(db) : "cannot open";
        if (db) sqlite3_close(db);
        return "OPEN FAILED: " + err;
    }
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, "PRAGMA integrity_check;", -1, &stmt, nullptr);
    std::string result = "no rows";
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return result;
}
#endif

} // namespace

// M018 — constructing the store opens/creates <dir>/map_level{level}.db.
TEST(MapTileStoreTest, OpensAndCreatesDbFile) {
    const fs::path dir = fs::temp_directory_path() / "mw_maptilestore_test_M018";
    fs::remove_all(dir);

    {
        MapTileStore store(dir.string(), 3);
        EXPECT_EQ(store.level(), 3);
        EXPECT_TRUE(store.is_open());
        EXPECT_TRUE(fs::exists(dir / "map_level3.db"));
        EXPECT_EQ(fs::path(store.path()).filename().string(), "map_level3.db");
    }

    // file persists after the store is closed
    EXPECT_TRUE(fs::exists(dir / "map_level3.db"));
    fs::remove_all(dir);
}

// M018 — different levels use different DB files.
TEST(MapTileStoreTest, PerLevelDbFiles) {
    const fs::path dir = fs::temp_directory_path() / "mw_maptilestore_test_M018b";
    fs::remove_all(dir);

    {
        MapTileStore l0(dir.string(), 0);
        MapTileStore l5(dir.string(), 5);
        EXPECT_NE(l0.path(), l5.path());
    }
    EXPECT_TRUE(fs::exists(dir / "map_level0.db"));
    EXPECT_TRUE(fs::exists(dir / "map_level5.db"));
    fs::remove_all(dir);
}

// M019 — constructing the store creates the `tile` table with (x, y) keys.
TEST(MapTileStoreTest, CreatesTileSchema) {
    const fs::path dir = fs::temp_directory_path() / "mw_maptilestore_test_M019";
    fs::remove_all(dir);

    {
        MapTileStore store(dir.string(), 2);
        ASSERT_TRUE(store.is_open());
    }

    // Re-open the file directly and confirm the schema landed.
    sqlite3* db = nullptr;
    ASSERT_EQ(sqlite3_open((dir / "map_level2.db").string().c_str(), &db), SQLITE_OK);

    sqlite3_stmt* stmt = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(
                  db,
                  "SELECT name FROM sqlite_master "
                  "WHERE type='table' AND name='tile';",
                  -1, &stmt, nullptr),
              SQLITE_OK);
    EXPECT_EQ(sqlite3_step(stmt), SQLITE_ROW);  // table exists
    sqlite3_finalize(stmt);

    // Inserting and reading back a row through the schema works.
    EXPECT_EQ(sqlite3_exec(
                  db, "INSERT INTO tile (x, y, payload) VALUES (7, -3, 'hello');",
                  nullptr, nullptr, nullptr),
              SQLITE_OK);

    ASSERT_EQ(sqlite3_prepare_v2(
                  db, "SELECT payload FROM tile WHERE x=7 AND y=-3;",
                  -1, &stmt, nullptr),
              SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    EXPECT_STREQ(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)), "hello");
    sqlite3_finalize(stmt);

    sqlite3_close(db);
    fs::remove_all(dir);
}

// M020 — a stored payload loads back identically (compared via canonical JSON).
TEST(MapTileStoreTest, StoreThenLoadRoundTrip) {
    const fs::path dir = fs::temp_directory_path() / "mw_maptilestore_test_M020";
    fs::remove_all(dir);

    const TileCoord    tile{4, 11, 6};
    const MapTilePayload p = make_payload(tile);

    {
        MapTileStore store(dir.string(), 4);
        store.store(tile, p);

        const std::optional<MapTilePayload> loaded = store.load(tile);
        ASSERT_TRUE(loaded.has_value());
        EXPECT_EQ(MapPayloadCodec::encode(*loaded), MapPayloadCodec::encode(p));
    }

    // Survives reopening the DB file.
    {
        MapTileStore store(dir.string(), 4);
        const std::optional<MapTilePayload> loaded = store.load(tile);
        ASSERT_TRUE(loaded.has_value());
        EXPECT_EQ(loaded->tile, tile);
        EXPECT_EQ(loaded->culture, "test-culture");
    }
    fs::remove_all(dir);
}

// M020 — loading a tile that was never stored yields nullopt.
TEST(MapTileStoreTest, LoadMissingReturnsNullopt) {
    const fs::path dir = fs::temp_directory_path() / "mw_maptilestore_test_M020b";
    fs::remove_all(dir);

    MapTileStore store(dir.string(), 1);
    EXPECT_FALSE(store.load(TileCoord{1, 0, 0}).has_value());
    fs::remove_all(dir);
}

// M020 — storing the same (x, y) again overwrites the previous payload.
TEST(MapTileStoreTest, StoreOverwrites) {
    const fs::path dir = fs::temp_directory_path() / "mw_maptilestore_test_M020c";
    fs::remove_all(dir);

    const TileCoord tile{3, 2, 5};
    MapTileStore    store(dir.string(), 3);

    MapTilePayload first = make_payload(tile);
    first.culture = "first";
    store.store(tile, first);

    MapTilePayload second = make_payload(tile);
    second.culture = "second";
    store.store(tile, second);

    const std::optional<MapTilePayload> loaded = store.load(tile);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->culture, "second");
    fs::remove_all(dir);
}

// M021 — has() is false before a store and true after, without decoding.
TEST(MapTileStoreTest, HasReflectsStore) {
    const fs::path dir = fs::temp_directory_path() / "mw_maptilestore_test_M021";
    fs::remove_all(dir);

    const TileCoord tile{6, 40, 12};
    MapTileStore    store(dir.string(), 6);

    EXPECT_FALSE(store.has(tile));
    store.store(tile, make_payload(tile));
    EXPECT_TRUE(store.has(tile));
    EXPECT_FALSE(store.has(TileCoord{6, 41, 12}));  // neighbor untouched
    fs::remove_all(dir);
}

// M020/M021 — a TileCoord whose level differs from the store's is rejected.
TEST(MapTileStoreTest, RejectsLevelMismatch) {
    const fs::path dir = fs::temp_directory_path() / "mw_maptilestore_test_M020d";
    fs::remove_all(dir);

    MapTileStore    store(dir.string(), 2);
    const TileCoord wrong{3, 0, 0};  // level 3 ≠ store level 2
    EXPECT_THROW(store.store(wrong, make_payload(wrong)), std::invalid_argument);
    EXPECT_THROW(store.load(wrong), std::invalid_argument);
    EXPECT_THROW(store.has(wrong), std::invalid_argument);
    fs::remove_all(dir);
}

// M034 — store/load round-trips across several levels in their own DB files.
TEST(MapTileStoreTest, RoundTripAcrossLevels) {
    const fs::path dir = fs::temp_directory_path() / "mw_maptilestore_test_M034";
    fs::remove_all(dir);

    for (int level = 0; level <= 8; ++level) {
        const int64_t   span = int64_t{1} << level;  // valid index range [0, 2^level)
        const TileCoord tile{level, span / 2, span / 3};
        const MapTilePayload p = make_payload(tile);

        MapTileStore store(dir.string(), level);
        EXPECT_FALSE(store.has(tile));
        store.store(tile, p);
        ASSERT_TRUE(store.has(tile));

        const std::optional<MapTilePayload> loaded = store.load(tile);
        ASSERT_TRUE(loaded.has_value());
        EXPECT_EQ(MapPayloadCodec::encode(*loaded), MapPayloadCodec::encode(p));
    }
    fs::remove_all(dir);
}

#ifdef MESH_WORLD_HAS_FORK_KILL_TEST
// M224 (MAP14) — crash safety: a child process hammers store() in a tight
// loop against the same map_level{N}.db, is killed with SIGKILL (no
// cleanup, no chance to close the connection) mid-run -- by construction,
// mid-write, since the child never voluntarily stops -- and a fresh
// connection afterward must still see a structurally sound database (WAL
// recovery, not corruption), and remain fully usable for further
// store()/load() calls.
TEST(MapTileStoreTest, SurvivesSimulatedMidWriteKill) {
    const fs::path dir = fs::temp_directory_path() / "mw_maptilestore_test_M224";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const std::string dir_str = dir.string();

    const pid_t pid = fork();
    ASSERT_NE(pid, -1) << "fork() failed";

    if (pid == 0) {
        // Child: hammer distinct-tile stores forever (until killed) against
        // the same level's DB file.
        MapTileStore store(dir_str, 5);
        for (int i = 0;; ++i) {
            const TileCoord tile{5, i % 1000, (i / 1000) % 1000};
            store.store(tile, make_payload(tile));
        }
        _exit(0);  // unreachable
    }

    // Parent: let the child hammer writes for a short window, then kill it
    // abruptly and reap it.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(kill(pid, SIGKILL), 0);
    int status = 0;
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFSIGNALED(status));
    ASSERT_EQ(WTERMSIG(status), SIGKILL);

    const std::string path = (dir / "map_level5.db").string();
    EXPECT_EQ(sqlite_integrity_check(path), "ok")
        << "database left structurally corrupt after a mid-write kill";

    // A brand-new MapTileStore must open cleanly and remain fully usable --
    // not just "not corrupt" but genuinely recovered.
    MapTileStore    recovered(dir_str, 5);
    const TileCoord probe{5, 777, 777};
    recovered.store(probe, make_payload(probe));
    const std::optional<MapTilePayload> loaded = recovered.load(probe);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(MapPayloadCodec::encode(*loaded), MapPayloadCodec::encode(make_payload(probe)));

    fs::remove_all(dir);
}
#endif  // MESH_WORLD_HAS_FORK_KILL_TEST
