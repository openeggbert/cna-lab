// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP11 tests. M179: dev-only export of a chunk's ModelPlacements to MC3
// XML for visual/manual debugging (not used at runtime).

#include <gtest/gtest.h>

#include <string>

#include "PlacementDevExport.hpp"

using namespace MeshWorld;

namespace {

ChunkContext make_ctx(ChunkCoord coord = {0, 0}) {
    ChunkContext ctx;
    ctx.coord        = coord;
    ctx.chunk_size_m = 64.0f;
    return ctx;
}

ModelPlacement make_placement(const std::string& id, double pos_x, double pos_z) {
    ModelPlacement p;
    p.definition_id = id;
    p.pos_x = pos_x;
    p.pos_z = pos_z;
    p.rot_y = 45.0f;
    p.scale = 1.5f;
    return p;
}

} // namespace

TEST(PlacementDevExportTest, EmptyPlacementsProducesValidEmptyChunkXml) {
    const std::string xml = export_chunk_placements_to_mc3(make_ctx(), {});
    EXPECT_NE(xml.find("<mc3"), std::string::npos) << xml;
    EXPECT_EQ(xml.find("<instance"), std::string::npos);
}

TEST(PlacementDevExportTest, OnePlacementPerInstanceElement) {
    const std::vector<ModelPlacement> placements{
        make_placement("tree_oak", 10.0, 20.0), make_placement("tree_pine", 30.0, 40.0)};
    const std::string xml = export_chunk_placements_to_mc3(make_ctx(), placements);

    int count = 0;
    std::size_t pos = 0;
    while ((pos = xml.find("<instance", pos)) != std::string::npos) {
        ++count;
        pos += 1;
    }
    EXPECT_EQ(count, 2);
    EXPECT_NE(xml.find("tree_oak"), std::string::npos);
    EXPECT_NE(xml.find("tree_pine"), std::string::npos);
}

TEST(PlacementDevExportTest, WorldPositionIsConvertedToChunkLocalCoordinates) {
    // Chunk (2, 3) with chunk_size_m=64 has world origin (128, 192). A
    // placement at world (138, 202) should end up local (10, 10).
    const ChunkContext ctx = make_ctx(ChunkCoord{2, 3});
    const std::vector<ModelPlacement> placements{make_placement("tree_oak", 138.0, 202.0)};
    const std::string xml = export_chunk_placements_to_mc3(ctx, placements);

    // Chunk-local coordinates must land within [0, chunk_size_m], not the
    // raw (much larger) world-space values.
    EXPECT_EQ(xml.find("x=\"138"), std::string::npos);
    EXPECT_EQ(xml.find("z=\"202"), std::string::npos);
}
