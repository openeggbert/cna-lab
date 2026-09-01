// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP8 tests. M126: MountainRanges ridge data model + generate(). M127:
// elevation profile (sampleElevation()) and applying it into a FieldGrid.

#include <gtest/gtest.h>

#include <cmath>

#include "Map/BiomeClassifier.hpp"
#include "Map/MountainRanges.hpp"
#include "ZoneType.hpp"

using namespace MeshWorld::Map;

TEST(MountainRangesTest, DefaultNetworkIsEmpty) {
    MountainRangeNetwork net;
    EXPECT_TRUE(net.empty());
}

TEST(MountainRangesTest, NetworkWithARangeIsNotEmpty) {
    MountainRangeNetwork net;
    net.ranges.push_back(MountainRange{});
    EXPECT_FALSE(net.empty());
}

TEST(MountainRangesTest, GenerateZeroOrNegativeCountReturnsEmptyNetwork) {
    EXPECT_TRUE(MountainRanges::generate(42, 0, 0.0, 0.0, 1000.0, 1000.0, 1500.0, 4000.0).empty());
    EXPECT_TRUE(MountainRanges::generate(42, -3, 0.0, 0.0, 1000.0, 1000.0, 1500.0, 4000.0).empty());
}

TEST(MountainRangesTest, GenerateDegenerateBoundsReturnsEmptyNetwork) {
    // world_x1 <= world_x0: zero/negative area, nothing to seed into.
    EXPECT_TRUE(MountainRanges::generate(42, 3, 100.0, 0.0, 100.0, 1000.0, 1500.0, 4000.0).empty());
}

TEST(MountainRangesTest, GenerateProducesRequestedRangeCountWithinBoundsAndElevationRange) {
    constexpr double x0 = 0.0, z0 = 0.0, x1 = 100000.0, z1 = 100000.0;
    constexpr double min_peak = 1500.0, max_peak = 4000.0;

    const MountainRangeNetwork net = MountainRanges::generate(42, 3, x0, z0, x1, z1, min_peak, max_peak);
    ASSERT_EQ(net.ranges.size(), 3u);

    for (const MountainRange& range : net.ranges) {
        ASSERT_FALSE(range.ridge.empty());
        for (const RidgePoint& p : range.ridge) {
            // Ridge points may wander outside the seed bounds (a random walk
            // isn't clamped), but elevation must stay within the requested
            // profile range.
            EXPECT_GE(p.elevation_m, min_peak);
            EXPECT_LE(p.elevation_m, max_peak);
        }
    }
}

// M134 — a named range's peaks classify as at least alpine tundra (or full
// mountain), never a lower-elevation biome. `min_peak_elevation_m` here
// (1500.0) matches both the constant PlanetGenerator/ChildGenerator
// actually pass and BiomeClassifier's own alpine threshold -- there's no
// public constant shared between the two systems, so this test is what
// would catch them drifting apart, not just a coincidental magic number.
// temperature/moisture are chosen so a low-elevation cell with the same
// values would NOT already be tundra by temperature alone (20 degC, 0.5
// moisture -> forest) -- this isolates the elevation-only alpine rule.
TEST(MountainRangesTest, PeaksClassifyAsAtLeastAlpineTundra) {
    constexpr double min_peak = 1500.0, max_peak = 4000.0;
    const MountainRangeNetwork net =
        MountainRanges::generate(7, 5, 0.0, 0.0, 100000.0, 100000.0, min_peak, max_peak);
    ASSERT_FALSE(net.ranges.empty());

    bool checked_any = false;
    for (const MountainRange& range : net.ranges) {
        for (const RidgePoint& p : range.ridge) {
            checked_any    = true;
            const auto zone = BiomeClassifier::classify(p.elevation_m, /*temperature_c=*/20.0,
                                                          /*moisture=*/0.5, /*sea_level_m=*/0.0);
            EXPECT_TRUE(zone == MeshWorld::ZoneType::mountain || zone == MeshWorld::ZoneType::tundra)
                << "elevation_m=" << p.elevation_m << " classified as "
                << MeshWorld::to_string(zone) << ", expected mountain or alpine tundra";
        }
    }
    EXPECT_TRUE(checked_any);
}

TEST(MountainRangesTest, GenerateIsDeterministic) {
    const MountainRangeNetwork a = MountainRanges::generate(7, 4, 0.0, 0.0, 50000.0, 50000.0, 1000.0, 3000.0);
    const MountainRangeNetwork b = MountainRanges::generate(7, 4, 0.0, 0.0, 50000.0, 50000.0, 1000.0, 3000.0);

    ASSERT_EQ(a.ranges.size(), b.ranges.size());
    for (std::size_t i = 0; i < a.ranges.size(); ++i) {
        ASSERT_EQ(a.ranges[i].ridge.size(), b.ranges[i].ridge.size());
        for (std::size_t j = 0; j < a.ranges[i].ridge.size(); ++j) {
            EXPECT_DOUBLE_EQ(a.ranges[i].ridge[j].x, b.ranges[i].ridge[j].x);
            EXPECT_DOUBLE_EQ(a.ranges[i].ridge[j].z, b.ranges[i].ridge[j].z);
            EXPECT_DOUBLE_EQ(a.ranges[i].ridge[j].elevation_m, b.ranges[i].ridge[j].elevation_m);
        }
    }
}

TEST(MountainRangesTest, GenerateWithDifferentEntropyProducesDifferentRidges) {
    const MountainRangeNetwork a = MountainRanges::generate(1, 2, 0.0, 0.0, 50000.0, 50000.0, 1000.0, 3000.0);
    const MountainRangeNetwork b = MountainRanges::generate(2, 2, 0.0, 0.0, 50000.0, 50000.0, 1000.0, 3000.0);

    ASSERT_FALSE(a.ranges.empty());
    ASSERT_FALSE(b.ranges.empty());
    EXPECT_NE(a.ranges[0].ridge[0].x, b.ranges[0].ridge[0].x);
}

// --- M127: sampleElevation() ---

TEST(MountainRangesTest, SampleElevationOverEmptyNetworkIsZero) {
    MountainRangeNetwork net;
    EXPECT_DOUBLE_EQ(MountainRanges::sampleElevation(net, 0.0, 0.0, 1000.0), 0.0);
}

TEST(MountainRangesTest, SampleElevationAtRidgePointEqualsItsOwnElevation) {
    MountainRangeNetwork net;
    MountainRange        range;
    range.ridge.push_back(RidgePoint{100.0, 200.0, 3000.0});
    net.ranges.push_back(range);

    // Distance 0 from the ridge point -> full falloff (1.0) -> its own elevation.
    EXPECT_NEAR(MountainRanges::sampleElevation(net, 100.0, 200.0, 500.0), 3000.0, 1e-9);
}

TEST(MountainRangesTest, SampleElevationFallsOffLinearlyWithDistanceAndReachesZeroBeyondWidth) {
    MountainRangeNetwork net;
    MountainRange        range;
    range.ridge.push_back(RidgePoint{0.0, 0.0, 1000.0});
    net.ranges.push_back(range);

    constexpr double falloff_width = 100.0;
    // Halfway to the falloff width: half the elevation.
    EXPECT_NEAR(MountainRanges::sampleElevation(net, 50.0, 0.0, falloff_width), 500.0, 1e-6);
    // At or beyond the falloff width: exactly zero.
    EXPECT_DOUBLE_EQ(MountainRanges::sampleElevation(net, 100.0, 0.0, falloff_width), 0.0);
    EXPECT_DOUBLE_EQ(MountainRanges::sampleElevation(net, 500.0, 0.0, falloff_width), 0.0);
}

TEST(MountainRangesTest, SampleElevationUsesNearestRidgePointAcrossRanges) {
    MountainRangeNetwork net;
    MountainRange        near_range;
    near_range.ridge.push_back(RidgePoint{10.0, 0.0, 500.0});
    net.ranges.push_back(near_range);

    MountainRange far_range;
    far_range.ridge.push_back(RidgePoint{1000.0, 0.0, 4000.0});
    net.ranges.push_back(far_range);

    // Query point is much closer to the first range's point, so its
    // (lower) elevation wins even though the other range is "taller".
    const double result = MountainRanges::sampleElevation(net, 0.0, 0.0, 2000.0);
    const double expected_falloff = 1.0 - 10.0 / 2000.0;
    EXPECT_NEAR(result, 500.0 * expected_falloff, 1e-6);
}

// --- M127: apply() ---

TEST(MountainRangesTest, ApplyOverEmptyNetworkIsANoOp) {
    FieldGrid g;
    g.w = g.h = 5;
    g.data.assign(25, 100.0f);
    const FieldGrid original = g;

    MountainRangeNetwork empty_net;
    MountainRanges::apply(g, empty_net, /*sea_level_m=*/0.0, /*falloff_width_m=*/100.0,
                           0.0, 0.0, 500.0, 500.0);

    EXPECT_EQ(g.data, original.data);
}

TEST(MountainRangesTest, ApplyRaisesInteriorCellsButNeverTouchesTheGridEdge) {
    constexpr int W = 7, H = 7;
    FieldGrid g;
    g.w = W;
    g.h = H;
    g.data.assign(static_cast<std::size_t>(W * H), 100.0f);
    const FieldGrid original = g;

    // World bounds: 100 m cells, so the grid center (3.5, 3.5 in cell
    // units) is at world (350, 350).
    constexpr double world_extent = 700.0;
    MountainRangeNetwork net;
    MountainRange        range;
    range.ridge.push_back(RidgePoint{350.0, 350.0, 2000.0});
    net.ranges.push_back(range);

    MountainRanges::apply(g, net, /*sea_level_m=*/0.0, /*falloff_width_m=*/400.0,
                           0.0, 0.0, world_extent, world_extent);

    // Edges are byte-for-byte unchanged.
    for (int x = 0; x < W; ++x) {
        EXPECT_FLOAT_EQ(g.at(x, 0), original.at(x, 0));
        EXPECT_FLOAT_EQ(g.at(x, H - 1), original.at(x, H - 1));
    }
    for (int y = 0; y < H; ++y) {
        EXPECT_FLOAT_EQ(g.at(0, y), original.at(0, y));
        EXPECT_FLOAT_EQ(g.at(W - 1, y), original.at(W - 1, y));
    }

    // The center cell (nearest the ridge point) is raised above its
    // original elevation.
    EXPECT_GT(g.at(3, 3), original.at(3, 3));
}

TEST(MountainRangesTest, ApplyNeverRaisesAnOceanCellAboveSeaLevel) {
    // A grid that's half land (100 m) and half ocean (-50 m), split down
    // the middle column-wise, with a ridge point sitting exactly on the
    // ocean side so its full falloff reaches deep into the ocean cells too.
    constexpr int W = 7, H = 7;
    FieldGrid g;
    g.w = W;
    g.h = H;
    g.data.assign(static_cast<std::size_t>(W * H), 100.0f);
    for (int y = 0; y < H; ++y)
        for (int x = 4; x < W; ++x) g.data[static_cast<std::size_t>(y * W + x)] = -50.0f;

    constexpr double world_extent = 700.0;  // 100 m cells
    MountainRangeNetwork net;
    MountainRange        range;
    range.ridge.push_back(RidgePoint{550.0, 350.0, 5000.0});  // centered in the ocean half
    net.ranges.push_back(range);

    MountainRanges::apply(g, net, /*sea_level_m=*/0.0, /*falloff_width_m=*/600.0,
                           0.0, 0.0, world_extent, world_extent);

    for (int y = 0; y < H; ++y)
        for (int x = 4; x < W; ++x)
            EXPECT_LT(g.at(x, y), 0.0f) << "ocean cell (" << x << "," << y << ") was raised above sea level";
}

TEST(MountainRangesTest, ApplyOnTooSmallGridIsANoOp) {
    FieldGrid g;
    g.w = g.h = 2;
    g.data    = {100, 100, 100, 100};
    const FieldGrid original = g;

    MountainRangeNetwork net;
    MountainRange        range;
    range.ridge.push_back(RidgePoint{0.5, 0.5, 3000.0});
    net.ranges.push_back(range);

    MountainRanges::apply(g, net, /*sea_level_m=*/0.0, /*falloff_width_m=*/100.0, 0.0, 0.0, 2.0, 2.0);

    EXPECT_EQ(g.data, original.data);
}
