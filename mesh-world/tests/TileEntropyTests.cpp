// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP1 tests. M010: tile_entropy() — per-tile entropy from world entropy + address.

#include <gtest/gtest.h>
#include <cstdint>
#include <unordered_set>
#include "Map/TileEntropy.hpp"

using namespace MeshWorld::Map;

// M010 — deterministic for the same inputs.
TEST(TileEntropyTest, Deterministic) {
    EXPECT_EQ(tile_entropy(12345, 5, 10, 20), tile_entropy(12345, 5, 10, 20));
    EXPECT_EQ(tile_entropy(0, 0, 0, 0),       tile_entropy(0, 0, 0, 0));
}

// M010 — the TileCoord overload matches the (level,x,y) form.
TEST(TileEntropyTest, TileCoordOverloadMatches) {
    EXPECT_EQ(tile_entropy(999, TileCoord{7, 3, 4}), tile_entropy(999, 7, 3, 4));
}

// M015 — neighboring tiles produce distinct entropy (no collisions in a block).
TEST(TileEntropyTest, DistinctForNeighbors) {
    const uint64_t we = 0xABCDEF12345ULL;
    std::unordered_set<uint64_t> seen;
    int count = 0;
    for (int dl = 0; dl <= 2; ++dl)
        for (int64_t dx = 0; dx < 8; ++dx)
            for (int64_t dy = 0; dy < 8; ++dy) {
                seen.insert(tile_entropy(we, 5 + dl, 100 + dx, 200 + dy));
                ++count;
            }
    EXPECT_EQ(seen.size(), static_cast<size_t>(count));  // every neighbor distinct
}

// M015 — a single-coordinate change changes the output (level / x / y).
TEST(TileEntropyTest, SingleCoordChangeChangesOutput) {
    const uint64_t base = tile_entropy(7, 5, 10, 20);
    EXPECT_NE(base, tile_entropy(7, 6, 10, 20));  // level + 1
    EXPECT_NE(base, tile_entropy(7, 5, 11, 20));  // x + 1
    EXPECT_NE(base, tile_entropy(7, 5, 10, 21));  // y + 1
}

// M015 — different world entropy yields different tile entropy (worlds diverge).
TEST(TileEntropyTest, SensitiveToWorldEntropy) {
    EXPECT_NE(tile_entropy(1, 5, 10, 20), tile_entropy(2, 5, 10, 20));
    EXPECT_NE(tile_entropy(0, 5, 10, 20),
              tile_entropy(0xFFFFFFFFFFFFFFFFULL, 5, 10, 20));
}
