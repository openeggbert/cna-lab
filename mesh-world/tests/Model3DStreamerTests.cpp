// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP11 tests. M172: src/Model3DStreamer.cpp -- query
// ModelPlacementStore::query_box around the player (x, y, z). M173:
// load/unload delta. M174: background thread pool (mirrors
// WorldStreamerTests.cpp's own wait_for() polling pattern for
// asynchronous assertions).

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <mutex>
#include <set>
#include <string>
#include <thread>

#include "Model3DStreamer.hpp"
#include "ModelPlacementWriter.hpp"

using namespace MeshWorld;
namespace fs = std::filesystem;

namespace {

ModelPlacement make_placement(const std::string& id, double pos_y = 0.0) {
    ModelPlacement p;
    p.definition_id = id;
    p.pos_y = pos_y;
    p.y_min = pos_y;
    p.y_max = pos_y;
    return p;
}

std::set<std::string> ids_of(const std::vector<ModelPlacement>& placements) {
    std::set<std::string> ids;
    for (const auto& p : placements) ids.insert(p.definition_id);
    return ids;
}

// Waits until predicate returns true or timeout_ms elapses (mirrors
// WorldStreamerTests.cpp's own wait_for() exactly).
template <typename Pred>
bool wait_for(Pred pred, int timeout_ms = 5000) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (!pred()) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return true;
}

// Accumulates every PlacementDelta the streamer's callback delivers, behind
// a mutex, so tests can poll for a specific delta showing up.
class DeltaRecorder {
public:
    void operator()(const PlacementDelta& delta) {
        std::lock_guard<std::mutex> lock(mutex_);
        deltas_.push_back(delta);
    }

    int count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<int>(deltas_.size());
    }

    bool any_added_contains(const std::string& id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& d : deltas_) {
            for (const auto& p : d.added) {
                if (p.definition_id == id) return true;
            }
        }
        return false;
    }

    bool any_removed_contains(const std::string& id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& d : deltas_) {
            for (const auto& p : d.removed) {
                if (p.definition_id == id) return true;
            }
        }
        return false;
    }

private:
    mutable std::mutex          mutex_;
    std::vector<PlacementDelta> deltas_;
};

} // namespace

TEST(Model3DStreamerTest, UpdateLoadsPlacementsWithinTheProximityBox) {
    const fs::path dir = fs::temp_directory_path() / "mw_model3dstreamer_test_basic";
    fs::remove_all(dir);

    write_chunk_placements(dir.string(), ChunkCoord{5, 5}, {make_placement("near")});
    write_chunk_placements(dir.string(), ChunkCoord{200, 200}, {make_placement("far")});

    Model3DStreamer streamer(dir.string(), /*horizontal_radius_chunks=*/10,
                              /*vertical_radius_m=*/100.0, /*thread_count=*/1,
                              /*chunk_size_m=*/64);
    streamer.update(320.0, 0.0, 320.0);

    ASSERT_TRUE(wait_for([&] { return !streamer.loaded_placements().empty(); }));
    const auto ids = ids_of(streamer.loaded_placements());
    EXPECT_NE(ids.find("near"), ids.end());
    EXPECT_EQ(ids.find("far"), ids.end());

    fs::remove_all(dir);
}

TEST(Model3DStreamerTest, ExcludesPlacementsOutsideTheVerticalRadius) {
    const fs::path dir = fs::temp_directory_path() / "mw_model3dstreamer_test_vertical";
    fs::remove_all(dir);

    write_chunk_placements(dir.string(), ChunkCoord{0, 0}, {make_placement("ground", 0.0)});
    write_chunk_placements(dir.string(), ChunkCoord{0, 0}, {make_placement("sky", 5000.0)});

    Model3DStreamer streamer(dir.string(), 5, /*vertical_radius_m=*/50.0, 1, 64);
    streamer.update(0.0, 0.0, 0.0);

    ASSERT_TRUE(wait_for([&] { return !streamer.loaded_placements().empty(); }));
    const auto ids = ids_of(streamer.loaded_placements());
    EXPECT_NE(ids.find("ground"), ids.end());
    EXPECT_EQ(ids.find("sky"), ids.end());

    fs::remove_all(dir);
}

TEST(Model3DStreamerTest, SpansMultipleRegionsNearABoundary) {
    const fs::path dir = fs::temp_directory_path() / "mw_model3dstreamer_test_regions";
    fs::remove_all(dir);

    // kRegionBlockChunks == 64, so chunk 63 is in region 0 and chunk 64 is in
    // region 1 -- a proximity box straddling that boundary must read both.
    write_chunk_placements(dir.string(), ChunkCoord{63, 0}, {make_placement("region_a")});
    write_chunk_placements(dir.string(), ChunkCoord{64, 0}, {make_placement("region_b")});
    ASSERT_NE(region_for_chunk(ChunkCoord{63, 0}), region_for_chunk(ChunkCoord{64, 0}));

    Model3DStreamer streamer(dir.string(), /*horizontal_radius_chunks=*/2, 100.0, 1, 64);
    streamer.update(63.5 * 64.0, 0.0, 0.0);

    ASSERT_TRUE(wait_for([&] { return ids_of(streamer.loaded_placements()).size() >= 2; }));
    const auto ids = ids_of(streamer.loaded_placements());
    EXPECT_NE(ids.find("region_a"), ids.end());
    EXPECT_NE(ids.find("region_b"), ids.end());

    fs::remove_all(dir);
}

TEST(Model3DStreamerTest, FarFromEverythingReturnsEmptySet) {
    const fs::path dir = fs::temp_directory_path() / "mw_model3dstreamer_test_empty";
    fs::remove_all(dir);

    write_chunk_placements(dir.string(), ChunkCoord{0, 0}, {make_placement("somewhere")});

    Model3DStreamer streamer(dir.string(), 2, 50.0, 1, 64);
    DeltaRecorder recorder;
    streamer.set_delta_callback(std::ref(recorder));
    streamer.update(1'000'000.0, 0.0, 1'000'000.0);

    ASSERT_TRUE(wait_for([&] { return recorder.count() >= 1; }));
    EXPECT_TRUE(streamer.loaded_placements().empty());

    fs::remove_all(dir);
}

TEST(Model3DStreamerTest, UpdateReplacesRatherThanAccumulatesLoadedSet) {
    const fs::path dir = fs::temp_directory_path() / "mw_model3dstreamer_test_replace";
    fs::remove_all(dir);

    write_chunk_placements(dir.string(), ChunkCoord{0, 0}, {make_placement("first_spot")});
    write_chunk_placements(dir.string(), ChunkCoord{500, 500}, {make_placement("second_spot")});

    Model3DStreamer streamer(dir.string(), 2, 50.0, 1, 64);
    streamer.update(0.0, 0.0, 0.0);
    ASSERT_TRUE(wait_for(
        [&] { return ids_of(streamer.loaded_placements()).count("first_spot") > 0; }));

    streamer.update(500.0 * 64.0, 0.0, 500.0 * 64.0);
    ASSERT_TRUE(wait_for(
        [&] { return ids_of(streamer.loaded_placements()).count("second_spot") > 0; }));
    const auto ids = ids_of(streamer.loaded_placements());
    EXPECT_EQ(ids.find("first_spot"), ids.end());
    EXPECT_NE(ids.find("second_spot"), ids.end());

    fs::remove_all(dir);
}

// R140 -- the app calls update every frame, but a proximity query's bounds do
// not change until the player crosses a chunk or altitude-band boundary.
// Repeating positions within one extent must not keep issuing SQLite queries.
TEST(Model3DStreamerTest, RepeatedUpdatesInsideSameQueryExtentDoNotRecompute) {
    const fs::path dir = fs::temp_directory_path() / "mw_model3dstreamer_test_throttle";
    fs::remove_all(dir);
    write_chunk_placements(dir.string(), ChunkCoord{0, 0}, {make_placement("near")});

    Model3DStreamer streamer(dir.string(), 2, 50.0, 1, 64);
    DeltaRecorder recorder;
    streamer.set_delta_callback(std::ref(recorder));
    streamer.update(1.0, 0.0, 1.0);
    ASSERT_TRUE(wait_for([&] { return recorder.count() == 1; }));

    for (int i = 0; i < 120; ++i)
        streamer.update(2.0 + static_cast<double>(i % 16), 0.0, 2.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    EXPECT_EQ(recorder.count(), 1);

    fs::remove_all(dir);
}

TEST(Model3DStreamerTest, FirstUpdatePutsEverythingInAddedAndNothingInRemoved) {
    const fs::path dir = fs::temp_directory_path() / "mw_model3dstreamer_test_first_delta";
    fs::remove_all(dir);

    write_chunk_placements(dir.string(), ChunkCoord{0, 0}, {make_placement("only_one")});

    Model3DStreamer streamer(dir.string(), 2, 50.0, 1, 64);
    DeltaRecorder recorder;
    streamer.set_delta_callback(std::ref(recorder));
    streamer.update(0.0, 0.0, 0.0);

    ASSERT_TRUE(wait_for([&] { return recorder.any_added_contains("only_one"); }));
    EXPECT_FALSE(recorder.any_removed_contains("only_one"));

    fs::remove_all(dir);
}

TEST(Model3DStreamerTest, MovingOutOfRangePutsPlacementInRemoved) {
    const fs::path dir = fs::temp_directory_path() / "mw_model3dstreamer_test_moved_away";
    fs::remove_all(dir);

    write_chunk_placements(dir.string(), ChunkCoord{0, 0}, {make_placement("left_behind")});

    Model3DStreamer streamer(dir.string(), 2, 50.0, 1, 64);
    DeltaRecorder recorder;
    streamer.set_delta_callback(std::ref(recorder));

    streamer.update(0.0, 0.0, 0.0);
    ASSERT_TRUE(wait_for([&] { return recorder.any_added_contains("left_behind"); }));

    streamer.update(1'000'000.0, 0.0, 1'000'000.0);
    ASSERT_TRUE(wait_for([&] { return recorder.any_removed_contains("left_behind"); }));

    fs::remove_all(dir);
}

TEST(Model3DStreamerTest, MovingIntoRangePutsPlacementInAdded) {
    const fs::path dir = fs::temp_directory_path() / "mw_model3dstreamer_test_moved_in";
    fs::remove_all(dir);

    write_chunk_placements(dir.string(), ChunkCoord{500, 500}, {make_placement("newly_near")});

    Model3DStreamer streamer(dir.string(), 2, 50.0, 1, 64);
    DeltaRecorder recorder;
    streamer.set_delta_callback(std::ref(recorder));

    streamer.update(0.0, 0.0, 0.0);  // far away initially -- not loaded
    ASSERT_TRUE(wait_for([&] { return recorder.count() >= 1; }));

    streamer.update(500.0 * 64.0, 0.0, 500.0 * 64.0);
    ASSERT_TRUE(wait_for([&] { return recorder.any_added_contains("newly_near"); }));

    fs::remove_all(dir);
}

// M218 (MAP14) -- store_pool_capacity_ is actually enforced: visiting more
// distinct regions than the configured cap must never leave more shards
// open than that cap, at any point along the way.
TEST(Model3DStreamerTest, StorePoolNeverExceedsItsConfiguredCapacity) {
    const fs::path dir = fs::temp_directory_path() / "mw_model3dstreamer_test_pool_cap";
    fs::remove_all(dir);

    const std::vector<int> region_chunk_x = {32, 96, 160, 224, 288};  // 5 distinct regions
    for (int i = 0; i < 5; ++i) {
        write_chunk_placements(dir.string(), ChunkCoord{region_chunk_x[i], 0},
                                {make_placement("region" + std::to_string(i))});
    }

    constexpr std::size_t kCapacity = 3;
    Model3DStreamer streamer(dir.string(), /*horizontal_radius_chunks=*/2, 50.0, 1, 64, kCapacity);

    for (int i = 0; i < 5; ++i) {
        const double world_x = region_chunk_x[i] * 64.0 + 32.0;
        streamer.update(world_x, 0.0, 0.0);
        const std::string id = "region" + std::to_string(i);
        ASSERT_TRUE(wait_for([&] { return ids_of(streamer.loaded_placements()).count(id) > 0; }))
            << id << " never loaded";
        EXPECT_LE(streamer.open_shard_count(), kCapacity)
            << "after visiting region " << i << ", more shards were open than the configured cap";
    }

    fs::remove_all(dir);
}

// M219 (MAP14) — a genuinely long simulated traverse (far more distinct
// regions than store_pool_capacity_ holds, well beyond M218's own
// correctness-focused 5-region test) must keep the pool bounded throughout
// -- not just "eventually converges," but never exceeds the cap at any
// single point along the way.
TEST(Model3DStreamerTest, LongTraverseNeverExceedsStorePoolCapacity) {
    const fs::path dir = fs::temp_directory_path() / "mw_model3dstreamer_test_long_traverse";
    fs::remove_all(dir);

    constexpr std::size_t kCapacity   = 8;
    constexpr int         kNumRegions = 40;
    // Each region's placement sits at its region's middle (chunk offset 32)
    // so a small horizontal_radius_chunks never straddles a boundary.
    for (int i = 0; i < kNumRegions; ++i) {
        write_chunk_placements(dir.string(), ChunkCoord{i * 64 + 32, 0},
                                {make_placement("region" + std::to_string(i))});
    }

    Model3DStreamer streamer(dir.string(), /*horizontal_radius_chunks=*/2, 50.0, 1, 64, kCapacity);

    for (int i = 0; i < kNumRegions; ++i) {
        const double world_x = (i * 64 + 32) * 64.0 + 32.0;
        streamer.update(world_x, 0.0, 0.0);
        const std::string id = "region" + std::to_string(i);
        ASSERT_TRUE(wait_for([&] { return ids_of(streamer.loaded_placements()).count(id) > 0; }))
            << id << " never loaded";
        ASSERT_LE(streamer.open_shard_count(), kCapacity)
            << "pool exceeded its capacity after visiting region " << i;
    }

    fs::remove_all(dir);
}

// M218 (MAP14) -- store_pool_ is now bounded (kDefaultStorePoolCapacity=32
// in production; overridden here to 2 so this test doesn't need to open
// dozens of real region DBs). Visiting more distinct regions than the cap
// allows necessarily evicts (closes) an earlier shard's ModelPlacementStore
// -- the correctness property that matters isn't "was the handle literally
// closed" (an internal, hard-to-observe detail) but "does the region still
// come back correctly when revisited": ModelPlacementStore's open-or-create
// constructor (mirroring MapTileStore's) means a re-open should just work,
// but this is exactly the kind of interaction a subtle bug could hide in
// (e.g. a write not actually committed before the handle closes), so it's
// worth asserting end-to-end rather than trusting the design by inspection
// alone.
TEST(Model3DStreamerTest, EvictedRegionShardReopensCorrectlyWhenRevisited) {
    const fs::path dir = fs::temp_directory_path() / "mw_model3dstreamer_test_shard_evict";
    fs::remove_all(dir);

    // 4 distinct regions (kRegionBlockChunks == 64); each placement sits at
    // its region's middle (chunk offset 32) so a small horizontal_radius_chunks
    // never straddles a region boundary while querying it.
    const std::vector<int> region_chunk_x = {32, 96, 160, 224};
    for (int i = 0; i < 4; ++i) {
        write_chunk_placements(dir.string(), ChunkCoord{region_chunk_x[i], 0},
                                {make_placement("region" + std::to_string(i))});
    }
    ASSERT_EQ(region_for_chunk(ChunkCoord{region_chunk_x[0], 0}).rx, 0);
    ASSERT_EQ(region_for_chunk(ChunkCoord{region_chunk_x[1], 0}).rx, 1);
    ASSERT_EQ(region_for_chunk(ChunkCoord{region_chunk_x[2], 0}).rx, 2);
    ASSERT_EQ(region_for_chunk(ChunkCoord{region_chunk_x[3], 0}).rx, 3);

    Model3DStreamer streamer(dir.string(), /*horizontal_radius_chunks=*/2, 50.0, 1, 64,
                              /*store_pool_capacity=*/2);

    for (int i = 0; i < 4; ++i) {
        const double world_x = region_chunk_x[i] * 64.0 + 32.0;
        streamer.update(world_x, 0.0, 0.0);
        const std::string id = "region" + std::to_string(i);
        ASSERT_TRUE(wait_for([&] { return ids_of(streamer.loaded_placements()).count(id) > 0; }))
            << id << " never loaded";
    }

    // Revisit region 0 -- with capacity 2 and 4 regions visited since, its
    // shard was necessarily evicted. Must reopen and return correct data,
    // not crash or silently come back empty/stale.
    streamer.update(region_chunk_x[0] * 64.0 + 32.0, 0.0, 0.0);
    ASSERT_TRUE(wait_for([&] { return ids_of(streamer.loaded_placements()).count("region0") > 0; }))
        << "region0's shard did not correctly reopen after eviction";

    fs::remove_all(dir);
}

TEST(Model3DStreamerTest, ShutdownJoinsAllWorkerThreadsWithoutHanging) {
    const fs::path dir = fs::temp_directory_path() / "mw_model3dstreamer_test_shutdown";
    fs::remove_all(dir);

    {
        Model3DStreamer streamer(dir.string(), 2, 50.0, /*thread_count=*/3, 64);
        streamer.update(0.0, 0.0, 0.0);
        streamer.shutdown();
        // A second shutdown() (also called by the destructor) must not hang
        // or double-join.
        streamer.shutdown();
    }

    fs::remove_all(dir);
}
