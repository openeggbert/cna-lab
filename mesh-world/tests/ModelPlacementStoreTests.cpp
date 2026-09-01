// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP2 tests (brought forward as a MAP11 M169 prerequisite -- see NEXT.md
// section 8). M027/M028: open/create lifecycle (mirrors Map::MapTileStore's
// own M018 test).

#include <gtest/gtest.h>
#include <sqlite3.h>

#include <filesystem>
#include <string>
#include <vector>

#include "ModelPlacementStore.hpp"

using namespace MeshWorld;
namespace fs = std::filesystem;

namespace {

// True if `name` (a table or index) exists in sqlite_master for the DB at
// `path`. Opens its own connection -- SQLite allows concurrent readers.
bool sqlite_master_has(const std::string& path, const std::string& name) {
    sqlite3* db = nullptr;
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return false;
    }
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, "SELECT 1 FROM sqlite_master WHERE name = ?;", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    const bool found = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return found;
}

// A minimal ModelPlacement with distinguishable field values, for
// insert_batch() round-trip checks.
ModelPlacement make_placement(const std::string& id, double pos_y) {
    ModelPlacement p;
    p.definition_id = id;
    p.pos_x = 10.0;
    p.pos_y = pos_y;
    p.pos_z = 30.0;
    p.y_min = pos_y;
    p.y_max = pos_y + 5.0;
    p.rot_y = 90.0f;
    p.scale = 1.5f;
    p.lod_min = 2;
    return p;
}

int row_count(const std::string& path) {
    sqlite3* db = nullptr;
    sqlite3_open(path.c_str(), &db);
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM placement;", -1, &stmt, nullptr);
    sqlite3_step(stmt);
    const int count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return count;
}

int row_count_for_chunk(const std::string& path, int chunk_x, int chunk_z) {
    sqlite3* db = nullptr;
    sqlite3_open(path.c_str(), &db);
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM placement WHERE chunk_x = ? AND chunk_z = ?;", -1,
                       &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, chunk_x);
    sqlite3_bind_int(stmt, 2, chunk_z);
    sqlite3_step(stmt);
    const int count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return count;
}

} // namespace

TEST(ModelPlacementStoreTest, OpensAndCreatesShardDbFile) {
    const fs::path dir = fs::temp_directory_path() / "mw_modelplacementstore_test_M027";
    fs::remove_all(dir);

    const RegionId region{3, -2};
    {
        ModelPlacementStore store(dir.string(), region);
        EXPECT_EQ(store.region(), region);
        EXPECT_TRUE(store.is_open());
        EXPECT_TRUE(fs::exists(dir / "models" / "3_-2.db"));
    }

    // Reopening an existing shard must not fail or wipe it.
    {
        ModelPlacementStore store(dir.string(), region);
        EXPECT_TRUE(store.is_open());
    }

    fs::remove_all(dir);
}

TEST(ModelPlacementStoreTest, DifferentRegionsUseDifferentShardFiles) {
    const fs::path dir = fs::temp_directory_path() / "mw_modelplacementstore_test_regions";
    fs::remove_all(dir);

    ModelPlacementStore a(dir.string(), RegionId{0, 0});
    ModelPlacementStore b(dir.string(), RegionId{1, 0});
    EXPECT_NE(a.path(), b.path());
    EXPECT_TRUE(fs::exists(dir / "models" / "0_0.db"));
    EXPECT_TRUE(fs::exists(dir / "models" / "1_0.db"));

    fs::remove_all(dir);
}

TEST(ModelPlacementStoreTest, ConstructingCreatesThePlacementTableAndIndex) {
    const fs::path dir = fs::temp_directory_path() / "mw_modelplacementstore_test_M028";
    fs::remove_all(dir);

    ModelPlacementStore store(dir.string(), RegionId{0, 0});
    EXPECT_TRUE(sqlite_master_has(store.path(), "placement"));
    EXPECT_TRUE(sqlite_master_has(store.path(), "idx_place_xz_alt"));

    fs::remove_all(dir);
}

TEST(ModelPlacementStoreTest, ReopeningAnExistingShardIsIdempotent) {
    const fs::path dir = fs::temp_directory_path() / "mw_modelplacementstore_test_reopen";
    fs::remove_all(dir);

    { ModelPlacementStore store(dir.string(), RegionId{5, 5}); }
    // A second construction against the same shard must not throw (a naive
    // "CREATE TABLE" without IF NOT EXISTS would fail here).
    EXPECT_NO_THROW({ ModelPlacementStore store(dir.string(), RegionId{5, 5}); });

    fs::remove_all(dir);
}

TEST(ModelPlacementStoreTest, InsertBatchWritesAllPlacements) {
    const fs::path dir = fs::temp_directory_path() / "mw_modelplacementstore_test_M030";
    fs::remove_all(dir);

    ModelPlacementStore store(dir.string(), RegionId{0, 0});
    const ChunkCoord chunk{7, 9};
    const std::vector<ModelPlacement> placements{
        make_placement("tree_oak", 10.0), make_placement("tree_pine", 20.0),
        make_placement("tree_birch", 30.0)};

    store.insert_batch(chunk, placements);
    EXPECT_EQ(row_count(store.path()), 3);
    EXPECT_EQ(row_count_for_chunk(store.path(), 7, 9), 3);

    fs::remove_all(dir);
}

TEST(ModelPlacementStoreTest, InsertBatchWithEmptyVectorIsANoOp) {
    const fs::path dir = fs::temp_directory_path() / "mw_modelplacementstore_test_M030_empty";
    fs::remove_all(dir);

    ModelPlacementStore store(dir.string(), RegionId{0, 0});
    EXPECT_NO_THROW(store.insert_batch(ChunkCoord{0, 0}, {}));
    EXPECT_EQ(row_count(store.path()), 0);

    fs::remove_all(dir);
}

TEST(ModelPlacementStoreTest, InsertBatchForDifferentChunksDoesNotDisturbEachOther) {
    const fs::path dir = fs::temp_directory_path() / "mw_modelplacementstore_test_M030_multi";
    fs::remove_all(dir);

    ModelPlacementStore store(dir.string(), RegionId{0, 0});
    store.insert_batch(ChunkCoord{1, 1}, {make_placement("tree_oak", 10.0)});
    store.insert_batch(ChunkCoord{2, 2}, {make_placement("tree_pine", 20.0),
                                           make_placement("tree_birch", 30.0)});

    EXPECT_EQ(row_count_for_chunk(store.path(), 1, 1), 1);
    EXPECT_EQ(row_count_for_chunk(store.path(), 2, 2), 2);
    EXPECT_EQ(row_count(store.path()), 3);

    fs::remove_all(dir);
}

TEST(ModelPlacementStoreTest, InsertBatchStoresCorrectAltBand) {
    const fs::path dir = fs::temp_directory_path() / "mw_modelplacementstore_test_M030_altband";
    fs::remove_all(dir);

    ModelPlacementStore store(dir.string(), RegionId{0, 0});
    store.insert_batch(ChunkCoord{0, 0}, {make_placement("tree_oak", 130.0)});  // alt_band_for(130) == 2

    sqlite3* db = nullptr;
    sqlite3_open(store.path().c_str(), &db);
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, "SELECT alt_band, definition FROM placement;", -1, &stmt, nullptr);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    EXPECT_EQ(sqlite3_column_int(stmt, 0), alt_band_for(130.0));
    EXPECT_STREQ(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)), "tree_oak");
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    fs::remove_all(dir);
}

TEST(ModelPlacementStoreTest, QueryBoxReturnsOnlyPlacementsInsideTheBoxBothAxes) {
    const fs::path dir = fs::temp_directory_path() / "mw_modelplacementstore_test_M031";
    fs::remove_all(dir);

    ModelPlacementStore store(dir.string(), RegionId{0, 0});
    // Inside the box (chunk (5,5), alt_band_for(10) == 0).
    store.insert_batch(ChunkCoord{5, 5}, {make_placement("inside", 10.0)});
    // Outside horizontally: chunk_x=20 is past chunk_max.x=10.
    store.insert_batch(ChunkCoord{20, 5}, {make_placement("outside_x", 10.0)});
    // Outside vertically: pos_y=1000 -> a far alt_band, well past alt_band_max.
    store.insert_batch(ChunkCoord{5, 5}, {make_placement("outside_alt", 1000.0)});

    const auto results = store.query_box(ChunkCoord{0, 0}, ChunkCoord{10, 10}, -1, 1);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].definition_id, "inside");

    fs::remove_all(dir);
}

TEST(ModelPlacementStoreTest, QueryBoxReturnsEmptyWhenNothingMatches) {
    const fs::path dir = fs::temp_directory_path() / "mw_modelplacementstore_test_M031_empty";
    fs::remove_all(dir);

    ModelPlacementStore store(dir.string(), RegionId{0, 0});
    store.insert_batch(ChunkCoord{5, 5}, {make_placement("somewhere", 10.0)});

    const auto results = store.query_box(ChunkCoord{100, 100}, ChunkCoord{200, 200}, -1, 1);
    EXPECT_TRUE(results.empty());

    fs::remove_all(dir);
}

TEST(ModelPlacementStoreTest, QueryBoxRoundTripsFieldValuesUnchanged) {
    const fs::path dir = fs::temp_directory_path() / "mw_modelplacementstore_test_M031_roundtrip";
    fs::remove_all(dir);

    ModelPlacementStore store(dir.string(), RegionId{0, 0});
    ModelPlacement original = make_placement("tree_oak", 42.5);
    original.pos_x = 123.25;
    original.pos_z = -77.5;
    original.rot_y = 180.0f;
    original.scale = 2.25f;
    original.lod_min = 3;
    store.insert_batch(ChunkCoord{1, 1}, {original});

    const auto results = store.query_box(ChunkCoord{1, 1}, ChunkCoord{1, 1}, -1, 1);
    ASSERT_EQ(results.size(), 1u);
    const ModelPlacement& p = results[0];
    EXPECT_EQ(p.definition_id, original.definition_id);
    EXPECT_DOUBLE_EQ(p.pos_x, original.pos_x);
    EXPECT_DOUBLE_EQ(p.pos_y, original.pos_y);
    EXPECT_DOUBLE_EQ(p.pos_z, original.pos_z);
    EXPECT_DOUBLE_EQ(p.y_min, original.y_min);
    EXPECT_DOUBLE_EQ(p.y_max, original.y_max);
    EXPECT_FLOAT_EQ(p.rot_y, original.rot_y);
    EXPECT_FLOAT_EQ(p.scale, original.scale);
    EXPECT_EQ(p.lod_min, original.lod_min);

    fs::remove_all(dir);
}

TEST(ModelPlacementStoreTest, ChunkBlobRoundTripsUnchanged) {
    const fs::path dir = fs::temp_directory_path() / "mw_modelplacementstore_test_blob";
    fs::remove_all(dir);

    ModelPlacementStore store(dir.string(), RegionId{0, 0});
    // Includes an embedded NUL byte to prove the BLOB round-trip doesn't
    // truncate at the first zero the way a naive C-string read would.
    const std::string blob = std::string("<mc3xml>fake compiled scene, incl a ") +
                              std::string(1, '\0') + std::string(" byte");
    store.store_chunk_blob(ChunkCoord{3, 4}, blob);

    const auto loaded = store.load_chunk_blob(ChunkCoord{3, 4});
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(*loaded, blob);

    fs::remove_all(dir);
}

TEST(ModelPlacementStoreTest, LoadChunkBlobReturnsNulloptWhenNeverStored) {
    const fs::path dir = fs::temp_directory_path() / "mw_modelplacementstore_test_blob_missing";
    fs::remove_all(dir);

    ModelPlacementStore store(dir.string(), RegionId{0, 0});
    EXPECT_FALSE(store.load_chunk_blob(ChunkCoord{9, 9}).has_value());

    fs::remove_all(dir);
}

TEST(ModelPlacementStoreTest, StoreChunkBlobOverwritesPreviousBlobForSameChunk) {
    const fs::path dir = fs::temp_directory_path() / "mw_modelplacementstore_test_blob_overwrite";
    fs::remove_all(dir);

    ModelPlacementStore store(dir.string(), RegionId{0, 0});
    store.store_chunk_blob(ChunkCoord{1, 1}, "first");
    store.store_chunk_blob(ChunkCoord{1, 1}, "second");

    const auto loaded = store.load_chunk_blob(ChunkCoord{1, 1});
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(*loaded, "second");

    fs::remove_all(dir);
}

TEST(AltBandForTest, FloorsPositionToBandIndex) {
    EXPECT_EQ(alt_band_for(0.0), 0);
    EXPECT_EQ(alt_band_for(63.9), 0);
    EXPECT_EQ(alt_band_for(64.0), 1);
    EXPECT_EQ(alt_band_for(-0.1), -1);
    EXPECT_EQ(alt_band_for(-64.0), -1);
    EXPECT_EQ(alt_band_for(-64.1), -2);
}
