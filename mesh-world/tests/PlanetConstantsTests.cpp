// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP1 tests. M005: planet & LOD-pyramid constants.

#include <gtest/gtest.h>
#include "Map/PlanetConstants.hpp"

using namespace MeshWorld::Map;

// M005 — constant values.
TEST(PlanetConstantsTest, Values) {
    EXPECT_DOUBLE_EQ(PLANET_SIZE_M, 22585000.0);
    EXPECT_EQ(MAX_LEVEL, 18);
    EXPECT_DOUBLE_EQ(ALT_BAND_M, 64.0);
}

// M005 — planet area is approximately Earth's surface (~510M km²), within 1%.
TEST(PlanetConstantsTest, AreaApproximatesEarth) {
    const double side_km = PLANET_SIZE_M / 1000.0;
    const double area_km2 = side_km * side_km;
    EXPECT_NEAR(area_km2, 510072000.0, 510072000.0 * 0.01);
}

// M005 — the deepest tile is at chunk scale (≈ 64 m; sane band 50–130 m).
TEST(PlanetConstantsTest, DeepestTileIsChunkScale) {
    const double deepest = PLANET_SIZE_M / static_cast<double>(1LL << MAX_LEVEL);
    EXPECT_GT(deepest, 50.0);
    EXPECT_LT(deepest, 130.0);
}
