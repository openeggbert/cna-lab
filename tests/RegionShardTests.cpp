// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP2 tests (M035-M037 territory), brought forward as a MAP11 M169
// prerequisite. M026: include/RegionShard.hpp -- region id from chunk coord.

#include <gtest/gtest.h>

#include <unordered_map>

#include "RegionShard.hpp"

using namespace MeshWorld;

TEST(RegionShardTest, SameChunkAlwaysMapsToTheSameRegion) {
    const ChunkCoord c{123, -456};
    const RegionId first = region_for_chunk(c);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(region_for_chunk(c), first);
    }
}

TEST(RegionShardTest, ChunksWithinOneBlockShareTheSameRegion) {
    // kRegionBlockChunks == 64, so chunks 0 and 63 share a region; 64 starts
    // the next one.
    const RegionId r0 = region_for_chunk(ChunkCoord{0, 0});
    const RegionId r63 = region_for_chunk(ChunkCoord{63, 63});
    const RegionId r64 = region_for_chunk(ChunkCoord{64, 64});

    EXPECT_EQ(r0, r63);
    EXPECT_NE(r0, r64);
}

TEST(RegionShardTest, NegativeChunkCoordinatesFloorDivideCorrectly) {
    // Chunk x=-1 must land in region rx=-1 (the block covering [-64, -1]),
    // not rx=0 -- a truncating (C++ default) division would wrongly give 0.
    const RegionId neg1 = region_for_chunk(ChunkCoord{-1, -1});
    EXPECT_EQ(neg1.rx, -1);
    EXPECT_EQ(neg1.rz, -1);

    // Chunk x=-64 starts the NEXT region further negative (rx=-1 covers
    // [-64,-1]; x=-65 must be rx=-2).
    const RegionId neg64 = region_for_chunk(ChunkCoord{-64, -64});
    EXPECT_EQ(neg64.rx, -1);
    const RegionId neg65 = region_for_chunk(ChunkCoord{-65, -65});
    EXPECT_EQ(neg65.rx, -2);
}

TEST(RegionShardTest, ZeroAdjacentChunksOnOppositeSidesLandInDifferentRegions) {
    // Chunk -1 (region -1) and chunk 0 (region 0) must NOT collide even
    // though they're adjacent across the zero boundary.
    const RegionId neg = region_for_chunk(ChunkCoord{-1, 0});
    const RegionId zero = region_for_chunk(ChunkCoord{0, 0});
    EXPECT_NE(neg, zero);
}

TEST(RegionShardTest, FloorDivMatchesStdFloorForPositiveAndNegativeInputs) {
    EXPECT_EQ(floor_div(0, 64), 0);
    EXPECT_EQ(floor_div(63, 64), 0);
    EXPECT_EQ(floor_div(64, 64), 1);
    EXPECT_EQ(floor_div(-1, 64), -1);
    EXPECT_EQ(floor_div(-64, 64), -1);
    EXPECT_EQ(floor_div(-65, 64), -2);
}

TEST(RegionShardTest, ShardPathMatchesMapMdConvention) {
    const RegionId r{3, -2};
    EXPECT_EQ(r.shard_path("saves/myworld"), "saves/myworld/models/3_-2.db");
}

TEST(RegionShardTest, RegionIdIsUsableAsUnorderedMapKey) {
    std::unordered_map<RegionId, int> counts;
    counts[region_for_chunk(ChunkCoord{0, 0})] = 1;
    counts[region_for_chunk(ChunkCoord{63, 63})] += 1;  // same region as above
    counts[region_for_chunk(ChunkCoord{64, 64})] = 5;   // different region

    EXPECT_EQ(counts.size(), 2u);
    EXPECT_EQ(counts[region_for_chunk(ChunkCoord{0, 0})], 2);
}
