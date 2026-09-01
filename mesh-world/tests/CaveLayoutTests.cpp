// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M332 (MAP21) — tests for CaveLayout::openings_for(): the deterministic,
// symmetric cross-chunk tunnel-opening decision CaveGenerator uses to draw
// a real connected cave system instead of isolated sealed rooms.

#include <gtest/gtest.h>

#include "CaveLayout.hpp"

using namespace MeshWorld;

TEST(CaveLayoutTest, DeterministicForTheSameInputs) {
    const ChunkCoord coord{7, 3};
    const auto a = CaveLayout::openings_for(42, coord);
    const auto b = CaveLayout::openings_for(42, coord);
    EXPECT_EQ(a.north, b.north);
    EXPECT_EQ(a.south, b.south);
    EXPECT_EQ(a.east,  b.east);
    EXPECT_EQ(a.west,  b.west);
}

// The core correctness property: chunk A's own opening toward its neighbor
// B must always agree with chunk B's own opening toward A -- both sides
// compute this independently (per-chunk generate() calls never see each
// other's output), so without real symmetry, a tunnel could open on one
// side of a shared wall and stay solid on the other.
TEST(CaveLayoutTest, SymmetricAcrossNeighboringChunksInAllFourDirections) {
    for (std::uint64_t seed = 1; seed <= 8; ++seed) {
        for (int32_t x = -5; x <= 5; ++x) {
            for (int32_t y = -5; y <= 5; ++y) {
                const ChunkCoord c{x, y};
                const auto here = CaveLayout::openings_for(seed, c);

                EXPECT_EQ(here.east, CaveLayout::openings_for(seed, c.east()).west)
                    << "seed=" << seed << " coord=(" << x << "," << y << ")";
                EXPECT_EQ(here.west, CaveLayout::openings_for(seed, c.west()).east)
                    << "seed=" << seed << " coord=(" << x << "," << y << ")";
                EXPECT_EQ(here.north, CaveLayout::openings_for(seed, c.north()).south)
                    << "seed=" << seed << " coord=(" << x << "," << y << ")";
                EXPECT_EQ(here.south, CaveLayout::openings_for(seed, c.south()).north)
                    << "seed=" << seed << " coord=(" << x << "," << y << ")";
            }
        }
    }
}

TEST(CaveLayoutTest, DifferentSeedsProduceDifferentLayouts) {
    const ChunkCoord coord{100, 200};
    bool any_different = false;
    const auto baseline = CaveLayout::openings_for(1, coord);
    for (std::uint64_t seed = 2; seed <= 20; ++seed) {
        const auto o = CaveLayout::openings_for(seed, coord);
        if (o.north != baseline.north || o.south != baseline.south ||
            o.east != baseline.east   || o.west != baseline.west) {
            any_different = true;
            break;
        }
    }
    EXPECT_TRUE(any_different);
}

// The layout must produce a genuine MIX -- both isolated chambers (0
// openings) and connected chambers (>=1 opening) -- not degenerate into
// "always sealed" or "always wide open" for any real range of coordinates.
TEST(CaveLayoutTest, ProducesAMixOfIsolatedAndConnectedChunks) {
    bool found_isolated = false, found_connected = false;
    for (int32_t x = 0; x < 30 && !(found_isolated && found_connected); ++x) {
        for (int32_t y = 0; y < 30 && !(found_isolated && found_connected); ++y) {
            const auto o = CaveLayout::openings_for(99, ChunkCoord{x, y});
            if (o.count() == 0) found_isolated = true;
            if (o.count() >= 1) found_connected = true;
        }
    }
    EXPECT_TRUE(found_isolated)  << "expected at least one fully-isolated chunk (0 openings)";
    EXPECT_TRUE(found_connected) << "expected at least one connected chunk (>=1 opening)";
}

TEST(CaveOpeningsTest, CountMatchesTheNumberOfTrueFields) {
    CaveOpenings o;
    EXPECT_EQ(o.count(), 0);
    o.north = true;
    EXPECT_EQ(o.count(), 1);
    o.east = true;
    EXPECT_EQ(o.count(), 2);
    o.south = o.west = true;
    EXPECT_EQ(o.count(), 4);
}
