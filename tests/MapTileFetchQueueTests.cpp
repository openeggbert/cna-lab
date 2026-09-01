// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M192-193 (MAP12) — MapTileFetchQueue: background tile generation for the
// map view, polled via is_ready()/is_pending(), headless (no ImGui/UI
// needed to test this).

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <thread>
#include <vector>

#include "MapTileFetchQueue.hpp"
#include "MapView.hpp"
#include "Map/MapTileStore.hpp"
#include "PlanetWorld.hpp"
#include "generators/map/PlanetGenerator.hpp"

using namespace MeshWorld;
using namespace MeshWorld::Map;

namespace {

std::string tmp_dir(const std::string& suffix) {
    return (std::filesystem::temp_directory_path() / ("meshworld_fetchqueue_test_" + suffix)).string();
}

template <typename Pred>
bool wait_for(Pred pred, int timeout_ms = 5000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (!pred()) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return true;
}

} // namespace

TEST(MapTileFetchQueueTest, RequestedTileBecomesReadyAndIsPersisted) {
    const std::string dir = tmp_dir("basic");
    std::filesystem::remove_all(dir);
    PlanetWorld::create_new(dir);

    MapTileFetchQueue queue(dir, PlanetParams{});
    const TileCoord tile{3, 1, 1};

    EXPECT_FALSE(queue.is_ready(tile));
    queue.request(tile);

    const bool became_ready = wait_for([&] { return queue.is_ready(tile); });
    EXPECT_TRUE(became_ready);
    EXPECT_FALSE(queue.is_pending(tile));

    // Actually persisted -- a fresh, independent PlanetWorld/MapTileStore
    // opened on this (test) thread can see it.
    PlanetWorld verify_world = PlanetWorld::open_existing(dir);
    EXPECT_TRUE(verify_world.tile_store(tile.level).has(tile));
}

TEST(MapTileFetchQueueTest, UnrequestedTileIsNeitherReadyNorPending) {
    const std::string dir = tmp_dir("unrequested");
    std::filesystem::remove_all(dir);
    PlanetWorld::create_new(dir);

    MapTileFetchQueue queue(dir, PlanetParams{});
    const TileCoord tile{5, 2, 2};

    EXPECT_FALSE(queue.is_ready(tile));
    EXPECT_FALSE(queue.is_pending(tile));
}

TEST(MapTileFetchQueueTest, RequestingTheSameTileTwiceIsHarmless) {
    const std::string dir = tmp_dir("dup_request");
    std::filesystem::remove_all(dir);
    PlanetWorld::create_new(dir);

    MapTileFetchQueue queue(dir, PlanetParams{});
    const TileCoord tile{2, 1, 0};

    queue.request(tile);
    queue.request(tile);  // must not crash or duplicate work in any observable way

    EXPECT_TRUE(wait_for([&] { return queue.is_ready(tile); }));
}

TEST(MapTileFetchQueueTest, MultipleDistinctTilesAllBecomeReady) {
    const std::string dir = tmp_dir("multi");
    std::filesystem::remove_all(dir);
    PlanetWorld::create_new(dir);

    MapTileFetchQueue queue(dir, PlanetParams{});
    const std::vector<TileCoord> tiles = {{4, 0, 0}, {4, 1, 0}, {4, 0, 1}, {4, 1, 1}};
    for (const auto& t : tiles) queue.request(t);

    for (const auto& t : tiles)
        EXPECT_TRUE(wait_for([&] { return queue.is_ready(t); })) << "tile ("
            << t.x << "," << t.y << ") at level " << t.level << " never became ready";
}

// M201 (MAP12) — "zooming into an ungenerated region enqueues exactly the
// needed tiles": a real MapView::visible_tiles() set, filtered down to the
// ones actually missing from disk (mirroring draw_map_view()'s own has()
// check), all become ready — and once they do, every originally-visible
// tile (not just the ones this test explicitly requested) is persisted.
TEST(MapTileFetchQueueTest, ZoomingIntoAnUngeneratedRegionEnqueuesExactlyTheNeededTiles) {
    const std::string dir = tmp_dir("mapview_integration");
    std::filesystem::remove_all(dir);
    PlanetWorld::create_new(dir);

    MapTileFetchQueue queue(dir, PlanetParams{});
    MapView           view;
    view.set_view(TileCoord{7, 5, 5});

    const auto tiles = view.visible_tiles(5, 5);
    ASSERT_FALSE(tiles.empty());

    // A fresh world has nothing persisted yet -- every visible tile needs
    // background generation, same "check has() first" logic
    // draw_map_view() itself uses.
    PlanetWorld reader_world = PlanetWorld::open_existing(dir);
    std::vector<TileCoord> needed;
    for (const auto& t : tiles)
        if (!reader_world.tile_store(t.level).has(t)) needed.push_back(t);
    ASSERT_EQ(needed.size(), tiles.size())
        << "expected every visible tile in a brand-new world to need generation";

    for (const auto& t : needed) queue.request(t);
    for (const auto& t : needed)
        EXPECT_TRUE(wait_for([&] { return queue.is_ready(t); }))
            << "tile (" << t.x << "," << t.y << ") never became ready";

    // Exactly the needed tiles were persisted -- every originally-visible
    // tile is now found by yet another fresh, independent PlanetWorld.
    PlanetWorld verify_world = PlanetWorld::open_existing(dir);
    for (const auto& t : tiles)
        EXPECT_TRUE(verify_world.tile_store(t.level).has(t))
            << "tile (" << t.x << "," << t.y << ") still missing after generation";
}

// M217 (MAP14) -- cancel_unneeded() drops requests that fast pan/zoom has
// scrolled away from before the worker even started them. Requests a large
// FIFO batch and immediately cancels down to just the first tile, with no
// delay in between: the single worker thread processes strictly in request
// order and each generation takes at least ~1ms (SQLite persistence alone),
// while issuing 50 request() calls is a handful of microseconds, so the
// batch's last tile is (deterministically, in practice) still sitting in
// the queue -- never popped -- when cancel_unneeded() runs.
TEST(MapTileFetchQueueTest, CancelUnneededDropsStillQueuedTilesNotInTheKeepSet) {
    const std::string dir = tmp_dir("cancel_unneeded");
    std::filesystem::remove_all(dir);
    PlanetWorld::create_new(dir);

    MapTileFetchQueue queue(dir, PlanetParams{});

    std::vector<TileCoord> batch;
    for (int i = 0; i < 50; ++i) batch.push_back(TileCoord{9, i, 0});
    for (const auto& t : batch) queue.request(t);

    const TileCoord kept    = batch.front();
    const TileCoord dropped = batch.back();
    queue.cancel_unneeded({kept});

    EXPECT_FALSE(queue.is_pending(dropped)) << "dropped tile should no longer be tracked at all";
    EXPECT_TRUE(wait_for([&] { return queue.is_ready(kept); })) << "kept tile should still complete normally";

    // Give the worker a further beat, then confirm the dropped tile really
    // never got generated -- not just "not pending yet".
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_FALSE(queue.is_ready(dropped));
}

// cancel_unneeded({}) -- an empty viewport (e.g. the map view was closed
// mid-generation) -- drops every still-queued request.
TEST(MapTileFetchQueueTest, CancelUnneededWithEmptyKeepSetDropsEverythingStillQueued) {
    const std::string dir = tmp_dir("cancel_unneeded_empty");
    std::filesystem::remove_all(dir);
    PlanetWorld::create_new(dir);

    MapTileFetchQueue queue(dir, PlanetParams{});

    std::vector<TileCoord> batch;
    for (int i = 0; i < 50; ++i) batch.push_back(TileCoord{9, i, 1});
    for (const auto& t : batch) queue.request(t);

    queue.cancel_unneeded({});

    EXPECT_FALSE(queue.is_pending(batch.back()));
}

// A tile still in the keep set survives cancel_unneeded() untouched --
// this is the "nothing actually changed" case that should run every frame
// as a no-op.
TEST(MapTileFetchQueueTest, CancelUnneededKeepsRequestedTilesStillInTheKeepSet) {
    const std::string dir = tmp_dir("cancel_unneeded_keep_all");
    std::filesystem::remove_all(dir);
    PlanetWorld::create_new(dir);

    MapTileFetchQueue queue(dir, PlanetParams{});
    const std::vector<TileCoord> tiles = {{6, 3, 3}, {6, 4, 3}};
    for (const auto& t : tiles) queue.request(t);

    queue.cancel_unneeded(tiles);

    for (const auto& t : tiles) EXPECT_TRUE(wait_for([&] { return queue.is_ready(t); }));
}
