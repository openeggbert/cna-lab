// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP16 tests, M265-268. Volcanism hotspot data model + generate()
// (structurally mirrors MountainRangesTests.cpp).

#include <gtest/gtest.h>

#include <algorithm>

#include "Map/Volcanism.hpp"

using namespace MeshWorld::Map;

TEST(VolcanismTest, DefaultFieldIsEmpty) {
    VolcanicField field;
    EXPECT_TRUE(field.empty());
}

TEST(VolcanismTest, FieldWithAHotspotIsNotEmpty) {
    VolcanicField field;
    field.hotspots.push_back(VolcanicHotspot{});
    EXPECT_FALSE(field.empty());
}

TEST(VolcanismTest, GenerateZeroOrNegativeCountReturnsEmptyField) {
    EXPECT_TRUE(Volcanism::generate(42, 0, 0.0, 0.0, 1000.0, 1000.0, 1500.0, 4000.0).empty());
    EXPECT_TRUE(Volcanism::generate(42, -3, 0.0, 0.0, 1000.0, 1000.0, 1500.0, 4000.0).empty());
}

TEST(VolcanismTest, GenerateDegenerateBoundsReturnsEmptyField) {
    // world_x1 <= world_x0: zero/negative area, nothing to seed into.
    EXPECT_TRUE(Volcanism::generate(42, 3, 100.0, 0.0, 100.0, 1000.0, 1500.0, 4000.0).empty());
}

TEST(VolcanismTest, GenerateProducesRequestedHotspotCountWithinBoundsAndElevationRange) {
    constexpr double x0 = 0.0, z0 = 0.0, x1 = 100000.0, z1 = 100000.0;
    constexpr double min_peak = 1500.0, max_peak = 4000.0;

    const VolcanicField field = Volcanism::generate(42, 5, x0, z0, x1, z1, min_peak, max_peak);
    ASSERT_EQ(field.hotspots.size(), 5u);

    const double min_dim    = std::min(x1 - x0, z1 - z0);
    const double min_radius = min_dim / 16.0;
    const double max_radius = min_dim / 6.0;

    for (const VolcanicHotspot& h : field.hotspots) {
        EXPECT_GE(h.peak_elevation_m, min_peak);
        EXPECT_LE(h.peak_elevation_m, max_peak);
        EXPECT_GE(h.radius_m, min_radius);
        EXPECT_LE(h.radius_m, max_radius);
    }
}

TEST(VolcanismTest, GenerateProducesAMixOfActiveAndDormantAcrossManyHotspots) {
    // ~30% active by design (kActiveProbability) -- over enough hotspots,
    // both states must appear, or the active/dormant draw itself is broken.
    const VolcanicField field = Volcanism::generate(99, 200, 0.0, 0.0, 1000000.0, 1000000.0,
                                                      1500.0, 4000.0);
    ASSERT_EQ(field.hotspots.size(), 200u);

    int active_count = 0;
    for (const VolcanicHotspot& h : field.hotspots)
        if (h.active) ++active_count;

    EXPECT_GT(active_count, 0);
    EXPECT_LT(active_count, static_cast<int>(field.hotspots.size()));
}

TEST(VolcanismTest, GenerateIsDeterministic) {
    const VolcanicField a = Volcanism::generate(7, 4, 0.0, 0.0, 50000.0, 50000.0, 1000.0, 3000.0);
    const VolcanicField b = Volcanism::generate(7, 4, 0.0, 0.0, 50000.0, 50000.0, 1000.0, 3000.0);

    ASSERT_EQ(a.hotspots.size(), b.hotspots.size());
    for (std::size_t i = 0; i < a.hotspots.size(); ++i) {
        EXPECT_DOUBLE_EQ(a.hotspots[i].x, b.hotspots[i].x);
        EXPECT_DOUBLE_EQ(a.hotspots[i].z, b.hotspots[i].z);
        EXPECT_DOUBLE_EQ(a.hotspots[i].peak_elevation_m, b.hotspots[i].peak_elevation_m);
        EXPECT_DOUBLE_EQ(a.hotspots[i].radius_m, b.hotspots[i].radius_m);
        EXPECT_EQ(a.hotspots[i].active, b.hotspots[i].active);
    }
}

TEST(VolcanismTest, GenerateWithDifferentEntropyProducesDifferentHotspots) {
    const VolcanicField a = Volcanism::generate(1, 2, 0.0, 0.0, 50000.0, 50000.0, 1000.0, 3000.0);
    const VolcanicField b = Volcanism::generate(2, 2, 0.0, 0.0, 50000.0, 50000.0, 1000.0, 3000.0);

    ASSERT_FALSE(a.hotspots.empty());
    ASSERT_FALSE(b.hotspots.empty());
    EXPECT_NE(a.hotspots[0].x, b.hotspots[0].x);
}

// --- sampleElevation() ---

TEST(VolcanismTest, SampleElevationOverEmptyFieldIsZero) {
    VolcanicField field;
    EXPECT_DOUBLE_EQ(Volcanism::sampleElevation(field, 0.0, 0.0), 0.0);
}

TEST(VolcanismTest, SampleElevationAtHotspotCenterEqualsItsOwnPeak) {
    VolcanicField field;
    field.hotspots.push_back(VolcanicHotspot{100.0, 200.0, /*peak_elevation_m=*/3000.0,
                                               /*radius_m=*/500.0, /*active=*/true});

    EXPECT_NEAR(Volcanism::sampleElevation(field, 100.0, 200.0), 3000.0, 1e-9);
}

TEST(VolcanismTest, SampleElevationFallsOffLinearlyAndReachesZeroAtRadius) {
    VolcanicField field;
    field.hotspots.push_back(VolcanicHotspot{0.0, 0.0, /*peak_elevation_m=*/1000.0,
                                               /*radius_m=*/100.0, /*active=*/true});

    // Halfway to the radius: half the elevation.
    EXPECT_NEAR(Volcanism::sampleElevation(field, 50.0, 0.0), 500.0, 1e-6);
    // At or beyond the radius: exactly zero.
    EXPECT_DOUBLE_EQ(Volcanism::sampleElevation(field, 100.0, 0.0), 0.0);
    EXPECT_DOUBLE_EQ(Volcanism::sampleElevation(field, 500.0, 0.0), 0.0);
}

TEST(VolcanismTest, SampleElevationTakesTheLargestOverlappingContributionNotTheNearest) {
    // A small, nearby hotspot with a modest peak vs. a farther, larger
    // hotspot with a much taller peak whose falloff still reaches the query
    // point with a bigger absolute contribution -- "largest wins", not
    // "nearest wins" (distinct from MountainRanges::sampleElevation()'s own
    // nearest-point convention -- see this function's own doc comment for
    // why: Hydrology::carve()'s "largest reduction wins" precedent, applied
    // symmetrically to uplift).
    VolcanicField field;
    field.hotspots.push_back(VolcanicHotspot{10.0, 0.0, /*peak_elevation_m=*/100.0,
                                               /*radius_m=*/50.0, /*active=*/true});
    field.hotspots.push_back(VolcanicHotspot{0.0, 0.0, /*peak_elevation_m=*/1000.0,
                                               /*radius_m=*/1000.0, /*active=*/true});

    // Query at the origin: hotspot A is 10 m away (falloff 0.8 -> 80),
    // hotspot B is 0 m away (falloff 1.0 -> 1000). B's contribution wins.
    EXPECT_NEAR(Volcanism::sampleElevation(field, 0.0, 0.0), 1000.0, 1e-6);
}

// --- apply() ---

TEST(VolcanismTest, ApplyOverEmptyFieldIsANoOp) {
    FieldGrid g;
    g.w = g.h = 5;
    g.data.assign(25, 100.0f);
    const FieldGrid original = g;

    VolcanicField empty_field;
    Volcanism::apply(g, empty_field, /*sea_level_m=*/0.0, 0.0, 0.0, 500.0, 500.0);

    EXPECT_EQ(g.data, original.data);
}

TEST(VolcanismTest, ApplyRaisesInteriorCellsButNeverTouchesTheGridEdge) {
    constexpr int W = 7, H = 7;
    FieldGrid g;
    g.w = W;
    g.h = H;
    g.data.assign(static_cast<std::size_t>(W * H), 100.0f);
    const FieldGrid original = g;

    // World bounds: 100 m cells, so the grid center (3.5, 3.5 in cell
    // units) is at world (350, 350).
    constexpr double world_extent = 700.0;
    VolcanicField field;
    field.hotspots.push_back(VolcanicHotspot{350.0, 350.0, /*peak_elevation_m=*/2000.0,
                                               /*radius_m=*/400.0, /*active=*/true});

    Volcanism::apply(g, field, /*sea_level_m=*/0.0, 0.0, 0.0, world_extent, world_extent);

    // Edges are byte-for-byte unchanged.
    for (int x = 0; x < W; ++x) {
        EXPECT_FLOAT_EQ(g.at(x, 0), original.at(x, 0));
        EXPECT_FLOAT_EQ(g.at(x, H - 1), original.at(x, H - 1));
    }
    for (int y = 0; y < H; ++y) {
        EXPECT_FLOAT_EQ(g.at(0, y), original.at(0, y));
        EXPECT_FLOAT_EQ(g.at(W - 1, y), original.at(W - 1, y));
    }

    // The center cell (nearest the hotspot) is raised above its original
    // elevation.
    EXPECT_GT(g.at(3, 3), original.at(3, 3));
}

TEST(VolcanismTest, ApplyNeverRaisesAnOceanCellAboveSeaLevel) {
    // A grid that's half land (100 m) and half ocean (-50 m), split down
    // the middle column-wise, with a hotspot sitting exactly on the ocean
    // side so its full falloff reaches deep into the ocean cells too.
    constexpr int W = 7, H = 7;
    FieldGrid g;
    g.w = W;
    g.h = H;
    g.data.assign(static_cast<std::size_t>(W * H), 100.0f);
    for (int y = 0; y < H; ++y)
        for (int x = 4; x < W; ++x) g.data[static_cast<std::size_t>(y * W + x)] = -50.0f;

    constexpr double world_extent = 700.0;  // 100 m cells
    VolcanicField field;
    field.hotspots.push_back(VolcanicHotspot{550.0, 350.0, /*peak_elevation_m=*/5000.0,
                                               /*radius_m=*/600.0,
                                               /*active=*/true});  // centered in the ocean half

    Volcanism::apply(g, field, /*sea_level_m=*/0.0, 0.0, 0.0, world_extent, world_extent);

    for (int y = 0; y < H; ++y)
        for (int x = 4; x < W; ++x)
            EXPECT_LT(g.at(x, y), 0.0f) << "ocean cell (" << x << "," << y << ") was raised above sea level";
}

TEST(VolcanismTest, ApplyOnTooSmallGridIsANoOp) {
    FieldGrid g;
    g.w = g.h = 2;
    g.data    = {100, 100, 100, 100};
    const FieldGrid original = g;

    VolcanicField field;
    field.hotspots.push_back(VolcanicHotspot{0.5, 0.5, /*peak_elevation_m=*/3000.0,
                                               /*radius_m=*/100.0, /*active=*/true});

    Volcanism::apply(g, field, /*sea_level_m=*/0.0, 0.0, 0.0, 2.0, 2.0);

    EXPECT_EQ(g.data, original.data);
}
