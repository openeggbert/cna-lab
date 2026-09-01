// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP3 tests. M041-M045: PlanetWorld bootstrap. M047/M048: entropy uniqueness
// across worlds + revisit consistency.

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <thread>

#include "PlanetWorld.hpp"
#include "Map/MapTilePayload.hpp"
#include "Map/MapPayloadCodec.hpp"

using namespace MeshWorld;
namespace fs = std::filesystem;

namespace {

fs::path fresh_dir(const char* name) {
    const fs::path dir = fs::temp_directory_path() / name;
    fs::remove_all(dir);
    return dir;
}

} // namespace

// create_new writes world.json + a non-zero entropy and creates the directory.
TEST(PlanetWorldTest, CreateNewWritesWorldJson) {
    const fs::path dir = fresh_dir("mw_planetworld_create");

    PlanetWorld world = PlanetWorld::create_new(dir.string());
    EXPECT_NE(world.world_entropy(), 0u);
    EXPECT_TRUE(fs::exists(dir / "world.json"));
    EXPECT_EQ(world.dir(), dir.string());

    fs::remove_all(dir);
}

// M047 — two worlds created at different instants get different entropy.
TEST(PlanetWorldTest, DistinctWorldsGetDifferentEntropy) {
    const fs::path dir_a = fresh_dir("mw_planetworld_entropy_a");
    const fs::path dir_b = fresh_dir("mw_planetworld_entropy_b");

    PlanetWorld a = PlanetWorld::create_new(dir_a.string());
    std::this_thread::sleep_for(std::chrono::milliseconds(2));  // advance steady_clock
    PlanetWorld b = PlanetWorld::create_new(dir_b.string());

    EXPECT_NE(a.world_entropy(), b.world_entropy());

    fs::remove_all(dir_a);
    fs::remove_all(dir_b);
}

// M048 — open_existing reloads the identical entropy + config (revisit
// consistency without a fixed seed).
TEST(PlanetWorldTest, OpenExistingReloadsEntropyAndConfig) {
    const fs::path dir = fresh_dir("mw_planetworld_reopen");

    WorldConfig cfg;
    cfg.name           = "reopen_test";
    cfg.continents_min = 7;
    cfg.continents_max = 14;
    cfg.planet_size_m  = 5000000.0;

    const std::uint64_t created_entropy =
        PlanetWorld::create_new(dir.string(), cfg).world_entropy();

    PlanetWorld reopened = PlanetWorld::open_existing(dir.string());
    EXPECT_EQ(reopened.world_entropy(), created_entropy);
    EXPECT_EQ(reopened.config().name, "reopen_test");
    EXPECT_EQ(reopened.config().continents_min, 7);
    EXPECT_EQ(reopened.config().continents_max, 14);
    EXPECT_DOUBLE_EQ(reopened.config().planet_size_m, 5000000.0);

    fs::remove_all(dir);
}

// open_existing on a missing world throws.
TEST(PlanetWorldTest, OpenMissingThrows) {
    const fs::path dir = fresh_dir("mw_planetworld_missing");
    EXPECT_THROW(PlanetWorld::open_existing(dir.string()), std::runtime_error);
    fs::remove_all(dir);
}

// M045 — tile_store lazily opens per level and returns the same instance.
TEST(PlanetWorldTest, TileStoreLazyAndCached) {
    const fs::path dir = fresh_dir("mw_planetworld_tilestore");

    PlanetWorld world = PlanetWorld::create_new(dir.string());
    Map::MapTileStore& s3a = world.tile_store(3);
    Map::MapTileStore& s3b = world.tile_store(3);
    EXPECT_EQ(&s3a, &s3b);                 // cached: same object
    EXPECT_EQ(world.tile_store(3).level(), 3);
    EXPECT_NE(&world.tile_store(5), &s3a);  // different level, different store

    fs::remove_all(dir);
}

// A payload stored through PlanetWorld survives a close + reopen of the world.
TEST(PlanetWorldTest, PayloadPersistsAcrossReopen) {
    const fs::path dir = fresh_dir("mw_planetworld_persist");

    const Map::TileCoord     tile{2, 1, 3};
    Map::MapTilePayload p;
    p.tile    = tile;
    p.culture = "planet-persist";

    {
        PlanetWorld world = PlanetWorld::create_new(dir.string());
        world.tile_store(tile.level).store(tile, p);
    }
    {
        PlanetWorld world = PlanetWorld::open_existing(dir.string());
        const auto loaded = world.tile_store(tile.level).load(tile);
        ASSERT_TRUE(loaded.has_value());
        EXPECT_EQ(Map::MapPayloadCodec::encode(*loaded), Map::MapPayloadCodec::encode(p));
    }

    fs::remove_all(dir);
}
