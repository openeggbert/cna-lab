// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M342 (MAP22): Coastline::trace() -- ocean/land boundary tracing +
// Chaikin smoothing, closing the "FeatureType::Coastline exists in name
// only" gap this task's audit found.

#include <gtest/gtest.h>

#include "Map/Coastline.hpp"

using namespace MeshWorld::Map;

namespace {

FieldGrid uniform_grid(int w, int h, float value) {
    FieldGrid g;
    g.w = w;
    g.h = h;
    g.data.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), value);
    return g;
}

} // namespace

TEST(CoastlineTest, EmptyElevationReturnsNoLoops) {
    FieldGrid elevation;
    const auto loops = Coastline::trace(elevation, 0.0, 0.0, 0.0, 100.0, 100.0);
    EXPECT_TRUE(loops.empty());
}

TEST(CoastlineTest, UniformLandReturnsNoLoops) {
    FieldGrid elevation = uniform_grid(6, 6, 50.0f);
    const auto loops = Coastline::trace(elevation, /*sea_level_m=*/-100.0, 0.0, 0.0, 600.0, 600.0);
    EXPECT_TRUE(loops.empty());
}

TEST(CoastlineTest, UniformOceanReturnsNoLoops) {
    FieldGrid elevation = uniform_grid(6, 6, -50.0f);
    const auto loops = Coastline::trace(elevation, /*sea_level_m=*/0.0, 0.0, 0.0, 600.0, 600.0);
    EXPECT_TRUE(loops.empty());
}

// A single land cell surrounded on all sides by ocean, well inside the
// grid (not touching any tile edge) -- must trace as exactly one CLOSED
// loop (first point == last point).
TEST(CoastlineTest, IslandFullyInsideTheGridTracesOneClosedLoop) {
    constexpr int W = 5, H = 5;
    FieldGrid elevation = uniform_grid(W, H, -50.0f);  // all ocean
    elevation.data[static_cast<std::size_t>(2 * W + 2)] = 50.0f;  // land island at (2,2)

    const auto loops = Coastline::trace(elevation, /*sea_level_m=*/0.0, 0.0, 0.0, 500.0, 500.0,
                                         /*smoothing_iterations=*/0);
    ASSERT_EQ(loops.size(), 1u);
    ASSERT_GE(loops[0].size(), 4u);
    EXPECT_EQ(loops[0].front(), loops[0].back()) << "an island fully inside the tile must close";
}

// Land fills the west half, ocean the east half -- the coastline is a
// single straight line running off BOTH the north and south tile edges,
// so it must trace as an OPEN path (front != back), not a closed loop.
TEST(CoastlineTest, CoastlineCrossingTheTileEdgeTracesAnOpenPath) {
    constexpr int W = 6, H = 6;
    FieldGrid elevation;
    elevation.w = W;
    elevation.h = H;
    elevation.data.resize(static_cast<std::size_t>(W * H));
    for (int gy = 0; gy < H; ++gy)
        for (int gx = 0; gx < W; ++gx)
            elevation.data[static_cast<std::size_t>(gy * W + gx)] = (gx < W / 2) ? 50.0f : -50.0f;

    const auto loops = Coastline::trace(elevation, /*sea_level_m=*/0.0, 0.0, 0.0, 600.0, 600.0,
                                         /*smoothing_iterations=*/0);
    ASSERT_EQ(loops.size(), 1u);
    EXPECT_NE(loops[0].front(), loops[0].back())
        << "a coastline that runs off both tile edges must NOT close into a loop";
}

TEST(CoastlineTest, EveryPointStaysWithinTheHalfOpenTileBounds) {
    constexpr int W = 8, H = 8;
    FieldGrid elevation;
    elevation.w = W;
    elevation.h = H;
    elevation.data.resize(static_cast<std::size_t>(W * H));
    for (int gy = 0; gy < H; ++gy)
        for (int gx = 0; gx < W; ++gx)
            elevation.data[static_cast<std::size_t>(gy * W + gx)] =
                ((gx + gy) % 3 == 0) ? -50.0f : 50.0f;  // an irregular scattered coastline

    const double x0 = 10.0, z0 = 20.0, x1 = 810.0, z1 = 820.0;
    const auto loops = Coastline::trace(elevation, 0.0, x0, z0, x1, z1, /*smoothing_iterations=*/2);
    ASSERT_FALSE(loops.empty());
    for (const auto& loop : loops) {
        for (const auto& p : loop) {
            EXPECT_GE(p[0], x0);
            EXPECT_LT(p[0], x1);
            EXPECT_GE(p[1], z0);
            EXPECT_LT(p[1], z1);
        }
    }
}

TEST(CoastlineTest, SmoothingProducesMorePointsThanTheRawTrace) {
    constexpr int W = 6, H = 6;
    FieldGrid elevation = uniform_grid(W, H, -50.0f);
    elevation.data[static_cast<std::size_t>(2 * W + 2)] = 50.0f;

    const auto raw     = Coastline::trace(elevation, 0.0, 0.0, 0.0, 600.0, 600.0, /*smoothing_iterations=*/0);
    const auto smoothed = Coastline::trace(elevation, 0.0, 0.0, 0.0, 600.0, 600.0, /*smoothing_iterations=*/2);
    ASSERT_EQ(raw.size(), 1u);
    ASSERT_EQ(smoothed.size(), 1u);
    EXPECT_GT(smoothed[0].size(), raw[0].size());
    EXPECT_EQ(smoothed[0].front(), smoothed[0].back()) << "still closed after smoothing";
}

// Chaikin's own point: a smoothed loop should no longer sit exactly ON the
// original blocky grid-corner positions -- it should have visibly cut the
// corners, i.e. moved strictly inside the raw trace's own bounding
// rectangle at at least one of its new points.
TEST(CoastlineTest, SmoothingActuallyMovesPointsAwayFromTheRawCorners) {
    constexpr int W = 6, H = 6;
    FieldGrid elevation = uniform_grid(W, H, -50.0f);
    elevation.data[static_cast<std::size_t>(2 * W + 2)] = 50.0f;

    const auto raw = Coastline::trace(elevation, 0.0, 0.0, 0.0, 600.0, 600.0, /*smoothing_iterations=*/0);
    const auto smoothed = Coastline::trace(elevation, 0.0, 0.0, 0.0, 600.0, 600.0, /*smoothing_iterations=*/2);
    ASSERT_EQ(raw.size(), 1u);
    ASSERT_EQ(smoothed.size(), 1u);

    bool any_new_point = true;
    for (const auto& sp : smoothed[0]) {
        bool matches_a_raw_corner = false;
        for (const auto& rp : raw[0]) {
            if (sp == rp) { matches_a_raw_corner = true; break; }
        }
        if (!matches_a_raw_corner) { any_new_point = true; break; }
        any_new_point = false;
    }
    EXPECT_TRUE(any_new_point);
}

TEST(CoastlineTest, DeterministicForTheSameInputs) {
    constexpr int W = 7, H = 7;
    FieldGrid elevation;
    elevation.w = W;
    elevation.h = H;
    elevation.data.resize(static_cast<std::size_t>(W * H));
    for (int gy = 0; gy < H; ++gy)
        for (int gx = 0; gx < W; ++gx)
            elevation.data[static_cast<std::size_t>(gy * W + gx)] =
                ((gx * 3 + gy * 7) % 5 == 0) ? -50.0f : 50.0f;

    const auto a = Coastline::trace(elevation, 0.0, 0.0, 0.0, 700.0, 700.0, 2);
    const auto b = Coastline::trace(elevation, 0.0, 0.0, 0.0, 700.0, 700.0, 2);
    EXPECT_EQ(a, b);
}

TEST(CoastlineTest, ZeroSmoothingIterationsReturnsTheRawBlockyTrace) {
    constexpr int W = 5, H = 5;
    FieldGrid elevation = uniform_grid(W, H, -50.0f);
    elevation.data[static_cast<std::size_t>(2 * W + 2)] = 50.0f;

    // A single land cell at (2,2) traces a 1x1 square: 4 corners + the
    // closing repeat = 5 points, exactly -- no smoothing has happened yet.
    const auto loops = Coastline::trace(elevation, 0.0, 0.0, 0.0, 500.0, 500.0, 0);
    ASSERT_EQ(loops.size(), 1u);
    EXPECT_EQ(loops[0].size(), 5u);
}

TEST(CoastlineTest, MultipleDisjointIslandsEachTraceTheirOwnLoop) {
    constexpr int W = 9, H = 9;
    FieldGrid elevation = uniform_grid(W, H, -50.0f);
    elevation.data[static_cast<std::size_t>(2 * W + 2)] = 50.0f;  // island A
    elevation.data[static_cast<std::size_t>(6 * W + 6)] = 50.0f;  // island B, far away

    const auto loops = Coastline::trace(elevation, 0.0, 0.0, 0.0, 900.0, 900.0, 0);
    EXPECT_EQ(loops.size(), 2u);
    for (const auto& loop : loops) EXPECT_EQ(loop.front(), loop.back());
}
