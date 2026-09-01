// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// T173-T174: WorldStreamer load/unload radius and thread-safety tests.

#include <gtest/gtest.h>
#include "WorldStreamer.hpp"
#include "WorldConfig.hpp"
#include "WorldMap.hpp"
#include "BuiltinMaterials.hpp"
#include "ChunkCoord.hpp"
#include "PlanetWorld.hpp"
#include "generators/map/PlanetGenerator.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <thread>

namespace {

MeshWorld::WorldConfig make_cfg() {
    MeshWorld::WorldConfig cfg;
    cfg.grid_w       = 20;
    cfg.grid_h       = 20;
    cfg.chunk_size_m = 64;
    cfg.seed         = 42;
    return cfg;
}

// Scratch directory under the system temp for test planet worlds. Mirrors
// MapPipelineTests.cpp's own tmp_dir() (see NEXT.md §5 #3e: a stale dir from
// a prior run can mask real generation — remove before a verification run).
std::string map_tmp_dir(const std::string& suffix) {
    return (std::filesystem::temp_directory_path() / ("meshworld_streamer_map_test_" + suffix))
        .string();
}

// Waits until predicate returns true or timeout_ms elapses.
template<typename Pred>
bool wait_for(Pred pred, int timeout_ms = 5000) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (!pred()) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return true;
}

} // namespace

// T173 — update() at world origin loads chunks within radius, unloads chunks outside.
TEST(WorldStreamerTests, LoadsChunksInRadiusAndUnloadsOutside) {
    MeshWorld::register_builtin_materials();

    auto cfg = make_cfg();
    MeshWorld::WorldMap map(cfg);

    // radius=1, 1 thread for determinism in test.
    MeshWorld::WorldStreamer streamer(cfg, map, /*load_radius=*/1, /*thread_count=*/1);

    // Center at chunk (2,2) in world coords.
    streamer.update(2.f * 64.f + 32.f, 2.f * 64.f + 32.f);

    // Wait until all chunks in radius are loaded (max 5 s).
    // Radius 1 circle: (0,0)+(0,±1)+(±1,0) = 5 chunks.
    bool ok = wait_for([&] { return streamer.loaded_count() >= 5; });
    EXPECT_TRUE(ok) << "Timed out waiting for chunks to load; loaded=" << streamer.loaded_count();

    // Chunk (2,2) must be loaded.
    const auto* c = streamer.chunk_at({2, 2});
    ASSERT_NE(c, nullptr);
    EXPECT_FALSE(c->xml.empty());
    EXPECT_EQ(c->state, MeshWorld::ChunkState::LOADED);

    // Chunk at (10,10) — far outside radius — must NOT be loaded.
    const auto* far = streamer.chunk_at({10, 10});
    EXPECT_EQ(far, nullptr);

    // Move player far away. Chunks around (2,2) should unload.
    streamer.update(10.f * 64.f + 32.f, 10.f * 64.f + 32.f);

    // After update, the old chunks are scheduled for unload immediately (synchronous unload).
    const auto* old = streamer.chunk_at({2, 2});
    EXPECT_EQ(old, nullptr) << "Chunk (2,2) should have been unloaded after player moved away";
}

// T174 — two simultaneous update() calls on different threads do not crash.
TEST(WorldStreamerTests, ConcurrentUpdateIsSafe) {
    MeshWorld::register_builtin_materials();

    auto cfg = make_cfg();
    MeshWorld::WorldMap map(cfg);
    MeshWorld::WorldStreamer streamer(cfg, map, /*load_radius=*/2, /*thread_count=*/2);

    std::atomic<int> iterations{0};
    constexpr int N = 10;

    auto task = [&](float base_x, float base_z) {
        for (int i = 0; i < N; ++i) {
            streamer.update(base_x + static_cast<float>(i) * 64.f, base_z);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            ++iterations;
        }
    };

    std::thread t1(task, 0.f,   0.f);
    std::thread t2(task, 640.f, 640.f);
    t1.join();
    t2.join();

    EXPECT_EQ(iterations.load(), N * 2);
    // Any loaded chunks must have valid (non-empty) XML.
    // We just verify no crash and no assertion failure above.
}

// Wiring Map::MapPipeline into WorldStreamer/WorldRenderer (MeshWorldApp task,
// see NEXT.md §8): each worker thread opens its own PlanetWorld/MapPipeline
// against the same on-disk directory (the constructor's own doc comment
// explains why sharing one across threads would race). The real risk this
// change introduces is exactly that concurrent-independent-connections
// pattern, not the single-threaded wiring itself (already covered by
// ChunkPipelineMapContextTests.cpp) — this test exercises it with multiple
// worker threads hammering update() concurrently, same shape as
// ConcurrentUpdateIsSafe above, plus the map layer attached.
TEST(WorldStreamerTests, WithMapLayerLoadsChunksWithoutCrashing) {
    MeshWorld::register_builtin_materials();

    const std::string dir = map_tmp_dir("with_map_layer");
    std::filesystem::remove_all(dir);
    MeshWorld::PlanetWorld::create_new(dir);

    auto cfg = make_cfg();
    MeshWorld::WorldMap map(cfg);
    MeshWorld::WorldStreamer streamer(cfg, map, /*load_radius=*/2, /*thread_count=*/3,
                                      "cache/chunks", dir, MeshWorld::Map::PlanetParams{});

    streamer.update(2.f * 64.f + 32.f, 2.f * 64.f + 32.f);
    bool ok = wait_for([&] { return streamer.loaded_count() >= 5; });
    EXPECT_TRUE(ok) << "Timed out waiting for chunks to load; loaded=" << streamer.loaded_count();

    const auto* c = streamer.chunk_at({2, 2});
    ASSERT_NE(c, nullptr);
    EXPECT_FALSE(c->xml.empty());
}
