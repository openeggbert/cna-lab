// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M161 — PersistentWorldMap is the legacy, pre-Map::-subsystem persistence
// mechanism still driving MeshWorldApp's own live world generation today
// (confirmed by tracing apps/mesh-world-app/main.cpp: it's the only real
// construction site, and WorldStreamer never attaches a Map::MapPipeline to
// its own internal ChunkPipeline, so Map::-derived data never reaches it).
// No test coverage existed for this class before M161 — these tests exist
// specifically to give plan.md's own "don't break existing saves" wording
// something concrete to stand on, since that can't be verified without a
// test proving the DB round-trip actually holds.

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "PersistentWorldMap.hpp"
#include "WorldConfig.hpp"
#include "WorldMap.hpp"
#include "ZoneType.hpp"

using namespace MeshWorld;

namespace {

std::string tmp_dir(const std::string& suffix) {
    return (std::filesystem::temp_directory_path() / ("meshworld_pwm_test_" + suffix)).string();
}

} // namespace

TEST(PersistentWorldMapTest, FirstVisitGeneratesAndPopulatesWorldMap) {
    const std::string dir = tmp_dir("first_visit");
    std::filesystem::remove_all(dir);

    WorldConfig cfg;
    WorldMap    map(cfg);
    PersistentWorldMap pwm(dir, map, /*level=*/0);

    const ChunkInfo info = pwm.ensure_chunk(5, 5);
    EXPECT_NE(info.zone, ZoneType::empty);
    // ensure_chunk() must also have written the result into the WorldMap
    // it was constructed with (not just returned it).
    EXPECT_EQ(map.info(5, 5).zone, info.zone);
    EXPECT_EQ(map.info(5, 5).region, info.region);
}

TEST(PersistentWorldMapTest, OutOfBoundsChunkReturnsEmptyAndDoesNotThrow) {
    const std::string dir = tmp_dir("out_of_bounds");
    std::filesystem::remove_all(dir);

    WorldConfig cfg;  // default 20x20 grid
    WorldMap    map(cfg);
    PersistentWorldMap pwm(dir, map, /*level=*/0);

    const ChunkInfo info = pwm.ensure_chunk(999, 999);
    EXPECT_EQ(info.zone, ZoneType::empty);
    EXPECT_EQ(info.region, RegionType::empty);
}

// The core "don't break existing saves" proof: a second PersistentWorldMap
// instance (a fresh process/session, its own time-seeded session_entropy_)
// pointed at the SAME directory must load the SAME ChunkInfo the first
// instance persisted, not regenerate a different one. session_entropy_
// differs every construction (see PersistentWorldMap.cpp), so if db_load()
// were broken this would be extremely unlikely to pass by chance.
TEST(PersistentWorldMapTest, SecondSessionLoadsPersistedDataUnchanged) {
    const std::string dir = tmp_dir("second_session");
    std::filesystem::remove_all(dir);

    ChunkInfo first_info;
    {
        WorldConfig cfg;
        WorldMap    map(cfg);
        PersistentWorldMap pwm(dir, map, /*level=*/0);
        first_info = pwm.ensure_chunk(7, 3);
    }
    {
        WorldConfig cfg;
        WorldMap    map(cfg);
        PersistentWorldMap pwm(dir, map, /*level=*/0);  // fresh session_entropy_
        const ChunkInfo second_info = pwm.ensure_chunk(7, 3);
        EXPECT_EQ(second_info.zone, first_info.zone);
        EXPECT_EQ(second_info.region, first_info.region);
    }
}

TEST(PersistentWorldMapTest, GeneratedZonesStayCoherentWithinPlanningBlock) {
    const std::string dir = tmp_dir("planning_blocks");
    std::filesystem::remove_all(dir);

    WorldConfig cfg;
    WorldMap    map(cfg);
    PersistentWorldMap pwm(dir, map, /*level=*/0);

    // (4,4) through (7,7) share one first-visit planning block. The exact
    // zone remains intentionally session-random, but it must be contiguous
    // so a city can contain a coherent road-and-parcel layout.
    const ZoneType expected = pwm.ensure_chunk(4, 4).zone;
    for (int y = 4; y < 8; ++y)
        for (int x = 4; x < 8; ++x)
            EXPECT_EQ(pwm.ensure_chunk(x, y).zone, expected) << "x=" << x << " y=" << y;
}

TEST(PersistentWorldMapTest, LateRoadPopulationUpdatesExistingNeighbourExits) {
    WorldConfig cfg;
    cfg.grid_w = 3;
    cfg.grid_h = 3;
    WorldMap map(cfg);

    map.set_info(1, 1, ChunkInfo{ZoneType::city, RegionType::small_house_block, {}});
    EXPECT_FALSE(map.exits(1, 1).north_road);

    map.set_info(1, 0, ChunkInfo{ZoneType::city, RegionType::road, {}});
    EXPECT_TRUE(map.exits(1, 1).north_road);
}

TEST(PersistentWorldMapTest, EnsureRegionPopulatesEveryCellWithinRadius) {
    const std::string dir = tmp_dir("ensure_region");
    std::filesystem::remove_all(dir);

    WorldConfig cfg;
    WorldMap    map(cfg);
    PersistentWorldMap pwm(dir, map, /*level=*/0);

    pwm.ensure_region(10, 10, 2);

    // Every cell within the circular radius must be populated (not the
    // default-empty ZoneType that an un-visited WorldMap cell starts as).
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            if (dx * dx + dy * dy > 2 * 2) continue;
            EXPECT_NE(map.info(10 + dx, 10 + dy).zone, ZoneType::empty)
                << "dx=" << dx << " dy=" << dy;
        }
    }
}

// Different world directories (levels) must not interfere with each other
// -- level is baked into the DB filename ("map_level{N}.db").
TEST(PersistentWorldMapTest, DifferentLevelsUseSeparateDatabases) {
    const std::string dir = tmp_dir("levels");
    std::filesystem::remove_all(dir);

    WorldConfig cfg;
    WorldMap    map0(cfg);
    WorldMap    map1(cfg);
    PersistentWorldMap pwm0(dir, map0, /*level=*/0);
    PersistentWorldMap pwm1(dir, map1, /*level=*/1);

    pwm0.ensure_chunk(2, 2);
    pwm1.ensure_chunk(2, 2);

    EXPECT_TRUE(std::filesystem::exists(dir + "/map_level0.db"));
    EXPECT_TRUE(std::filesystem::exists(dir + "/map_level1.db"));
}
