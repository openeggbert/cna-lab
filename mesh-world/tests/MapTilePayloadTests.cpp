// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP2 tests. M017: MapTilePayload data container.

#include <gtest/gtest.h>
#include "Map/MapTilePayload.hpp"

using namespace MeshWorld::Map;

// M017 — construct, populate, and read back every payload section.
TEST(MapTilePayloadTest, ConstructAndAccess) {
    MapTilePayload p;
    p.tile      = TileCoord{3, 1, 2};
    p.entropy   = 0xABCDEF;
    p.culture   = "nordic";
    p.generator = "lua.map.continent.default";

    // scalar field, row-major
    p.elevation = FieldGrid{2, 2, {1.f, 2.f, 3.f, 4.f}};
    EXPECT_EQ(p.elevation.at(1, 0), 2.0f);  // gy=0, gx=1
    EXPECT_EQ(p.elevation.at(0, 1), 3.0f);  // gy=1, gx=0
    EXPECT_FALSE(p.elevation.empty());
    EXPECT_TRUE(p.moisture.empty());

    // biome grid (ZoneType ordinals)
    p.biome = BiomeGrid{1, 1, {5}};
    EXPECT_EQ(p.biome.at(0, 0), 5);

    // feature
    MapFeature f;
    f.type = FeatureType::City;
    f.name = "Vorhavn";
    f.points = {{10.0, 20.0}};
    f.attributes["population_hint"] = 42000;
    p.features.push_back(f);
    ASSERT_EQ(p.features.size(), 1u);
    EXPECT_EQ(p.features[0].type, FeatureType::City);
    EXPECT_EQ(p.features[0].name, "Vorhavn");
    EXPECT_DOUBLE_EQ(p.features[0].attributes.at("population_hint"), 42000.0);

    // edges (N,E,S,W)
    p.edges[0].elevation = {1.f, 2.f, 3.f};
    EXPECT_EQ(p.edges[0].elevation.size(), 3u);
    EXPECT_TRUE(p.edges[2].elevation.empty());

    // M107 — edge biome/crossings default empty, and can be populated.
    EXPECT_TRUE(p.edges[0].biome.empty());
    EXPECT_TRUE(p.edges[0].crossings.empty());
    p.edges[0].biome     = {1, 2, 3};
    p.edges[0].crossings = {EdgeCrossing{EdgeCrossingType::River, 0.25f},
                            EdgeCrossing{EdgeCrossingType::Road, 0.75f}};
    EXPECT_EQ(p.edges[0].biome.size(), 3u);
    ASSERT_EQ(p.edges[0].crossings.size(), 2u);
    EXPECT_EQ(p.edges[0].crossings[0].type, EdgeCrossingType::River);
    EXPECT_FLOAT_EQ(p.edges[0].crossings[1].position, 0.75f);

    // label
    p.labels.push_back({"Aeland", {{0.0, 0.0}}, "country"});
    ASSERT_EQ(p.labels.size(), 1u);
    EXPECT_EQ(p.labels[0].kind, "country");

    EXPECT_EQ(p.tile, (TileCoord{3, 1, 2}));
}
