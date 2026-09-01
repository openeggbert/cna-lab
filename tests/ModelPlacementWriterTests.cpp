// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP11 tests. M169: src/ModelPlacementWriter.cpp -- write a chunk's
// placements into the region ModelPlacementStore.

#include <gtest/gtest.h>

#include <filesystem>

#include "ModelPlacementStore.hpp"
#include "ModelPlacementWriter.hpp"
#include "RegionShard.hpp"

using namespace MeshWorld;
namespace fs = std::filesystem;

namespace {

ModelPlacement make_placement(const std::string& id) {
    ModelPlacement p;
    p.definition_id = id;
    p.pos_x = 1.0;
    p.pos_y = 2.0;
    p.pos_z = 3.0;
    return p;
}

} // namespace

TEST(ModelPlacementWriterTest, WrittenPlacementsAreQueryableAfterward) {
    const fs::path dir = fs::temp_directory_path() / "mw_modelplacementwriter_test_basic";
    fs::remove_all(dir);

    const ChunkCoord chunk{4, 5};
    write_chunk_placements(dir.string(), chunk,
                            {make_placement("tree_oak"), make_placement("tree_pine")});

    ModelPlacementStore store(dir.string(), region_for_chunk(chunk));
    const auto results = store.query_box(chunk, chunk, alt_band_for(0.0) - 1, alt_band_for(0.0) + 1);
    EXPECT_EQ(results.size(), 2u);

    fs::remove_all(dir);
}

TEST(ModelPlacementWriterTest, EmptyPlacementsDoesNotCreateARegionFile) {
    const fs::path dir = fs::temp_directory_path() / "mw_modelplacementwriter_test_empty";
    fs::remove_all(dir);

    write_chunk_placements(dir.string(), ChunkCoord{0, 0}, {});
    EXPECT_FALSE(fs::exists(dir));

    fs::remove_all(dir);
}

TEST(ModelPlacementWriterTest, ChunksInDifferentRegionsCreateSeparateShardFiles) {
    const fs::path dir = fs::temp_directory_path() / "mw_modelplacementwriter_test_regions";
    fs::remove_all(dir);

    const ChunkCoord chunk_a{0, 0};
    const ChunkCoord chunk_b{1000, 1000};  // far enough to land in a different region
    ASSERT_NE(region_for_chunk(chunk_a), region_for_chunk(chunk_b));

    write_chunk_placements(dir.string(), chunk_a, {make_placement("tree_oak")});
    write_chunk_placements(dir.string(), chunk_b, {make_placement("tree_pine")});

    EXPECT_TRUE(fs::exists(region_for_chunk(chunk_a).shard_path(dir.string())));
    EXPECT_TRUE(fs::exists(region_for_chunk(chunk_b).shard_path(dir.string())));
    EXPECT_NE(region_for_chunk(chunk_a).shard_path(dir.string()),
              region_for_chunk(chunk_b).shard_path(dir.string()));

    fs::remove_all(dir);
}
