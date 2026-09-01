// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP1 tests. M001: TileCoord struct + comparison operators.

#include <gtest/gtest.h>
#include <cmath>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include "Map/TileCoord.hpp"

using MeshWorld::Map::TileCoord;
using MeshWorld::Map::PLANET_SIZE_M;
using MeshWorld::Map::MAX_LEVEL;

// M001 — equality / inequality.
TEST(TileCoordTests, EqualityAndInequality) {
    TileCoord a{3, 10, 20};
    TileCoord b{3, 10, 20};
    TileCoord c{3, 10, 21};

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
    EXPECT_NE(a, (TileCoord{4, 10, 20}));  // different level
    EXPECT_NE(a, (TileCoord{3, 11, 20}));  // different x

    EXPECT_EQ((TileCoord{}), (TileCoord{0, 0, 0}));  // default == origin
}

// M001 — lexicographic ordering by (level, x, y).
TEST(TileCoordTests, Ordering) {
    TileCoord a{3, 10, 20};

    EXPECT_LT(a, (TileCoord{3, 10, 21}));  // same level/x, larger y
    EXPECT_LT((TileCoord{3, 9, 99}), a);   // smaller x dominates y
    EXPECT_LT((TileCoord{2, 99, 99}), a);  // smaller level dominates x and y

    EXPECT_FALSE(a < a);                   // strict: not less than itself
}

// M003 — parent(): tile one level up, (level-1, x/2, y/2).
TEST(TileCoordTests, Parent) {
    EXPECT_EQ((TileCoord{3, 10, 20}).parent(), (TileCoord{2, 5, 10}));
    EXPECT_EQ((TileCoord{3, 11, 21}).parent(), (TileCoord{2, 5, 10}));  // floor: all 4 children share a parent
    EXPECT_EQ((TileCoord{1, 1, 0}).parent(),   (TileCoord{0, 0, 0}));

    // planet root (level 0) is its own parent
    EXPECT_EQ((TileCoord{0, 0, 0}).parent(), (TileCoord{0, 0, 0}));
}

// M004 — child(qx,qy) and children(): the four level+1 quadtree children.
TEST(TileCoordTests, Children) {
    TileCoord t{2, 5, 10};

    EXPECT_EQ(t.child(0, 0), (TileCoord{3, 10, 20}));
    EXPECT_EQ(t.child(1, 0), (TileCoord{3, 11, 20}));
    EXPECT_EQ(t.child(0, 1), (TileCoord{3, 10, 21}));
    EXPECT_EQ(t.child(1, 1), (TileCoord{3, 11, 21}));

    auto kids = t.children();
    ASSERT_EQ(kids.size(), 4u);
    for (const auto& c : kids) {
        EXPECT_EQ(c.level, t.level + 1);
        EXPECT_EQ(c.parent(), t);  // child → parent round-trip
    }
}

// M006 — tile_size_m(level) = PLANET_SIZE_M / 2^level; halves each level.
TEST(TileCoordTests, TileSize) {
    EXPECT_DOUBLE_EQ(TileCoord::tile_size_m(0),  PLANET_SIZE_M);
    EXPECT_DOUBLE_EQ(TileCoord::tile_size_m(1),  PLANET_SIZE_M / 2.0);
    EXPECT_DOUBLE_EQ(TileCoord::tile_size_m(10), PLANET_SIZE_M / 1024.0);

    for (int z = 0; z < MAX_LEVEL; ++z)
        EXPECT_DOUBLE_EQ(TileCoord::tile_size_m(z + 1), TileCoord::tile_size_m(z) / 2.0);

    // member size_m() uses own level
    EXPECT_DOUBLE_EQ((TileCoord{5, 0, 0}).size_m(), TileCoord::tile_size_m(5));
}

// M007 — world_bounds(): world-space extent of a tile in meters.
TEST(TileCoordTests, WorldBounds) {
    // level 0 covers the whole planet
    auto b0 = TileCoord{0, 0, 0}.world_bounds();
    EXPECT_DOUBLE_EQ(b0.min_x, 0.0);
    EXPECT_DOUBLE_EQ(b0.min_z, 0.0);
    EXPECT_DOUBLE_EQ(b0.max_x, PLANET_SIZE_M);
    EXPECT_DOUBLE_EQ(b0.max_z, PLANET_SIZE_M);

    // level 1, far quadrant (1,1)
    const double half = PLANET_SIZE_M / 2.0;
    auto b = TileCoord{1, 1, 1}.world_bounds();
    EXPECT_DOUBLE_EQ(b.min_x, half);
    EXPECT_DOUBLE_EQ(b.min_z, half);
    EXPECT_DOUBLE_EQ(b.max_x, PLANET_SIZE_M);
    EXPECT_DOUBLE_EQ(b.max_z, PLANET_SIZE_M);

    // bounds span exactly one tile edge
    auto t  = TileCoord{5, 3, 7};
    auto bb = t.world_bounds();
    EXPECT_DOUBLE_EQ(bb.max_x - bb.min_x, t.size_m());
    EXPECT_DOUBLE_EQ(bb.max_z - bb.min_z, t.size_m());
}

// M008 — from_world(): tile containing a world point; inverse of world_bounds().
TEST(TileCoordTests, FromWorld) {
    // any in-bounds point at level 0 → the single planet tile
    EXPECT_EQ(TileCoord::from_world(123456.0, 7890.0, 0), (TileCoord{0, 0, 0}));

    // round-trip: the center of a tile maps back to that tile
    for (TileCoord t : {TileCoord{1, 1, 1}, TileCoord{5, 3, 7}, TileCoord{10, 500, 200}}) {
        auto b = t.world_bounds();
        const double cx = (b.min_x + b.max_x) / 2.0;
        const double cz = (b.min_z + b.max_z) / 2.0;
        EXPECT_EQ(TileCoord::from_world(cx, cz, t.level), t);
    }

    // min corner is inclusive (belongs to the tile)
    auto t = TileCoord{4, 2, 3};
    auto b = t.world_bounds();
    EXPECT_EQ(TileCoord::from_world(b.min_x, b.min_z, 4), t);
}

// M011 — chunk_range(): inclusive chunk indices a tile overlaps.
TEST(TileCoordTests, ChunkRange) {
    const int cs = 64;

    for (TileCoord t : {TileCoord{0, 0, 0}, TileCoord{10, 5, 9}, TileCoord{MAX_LEVEL, 3, 7}}) {
        auto b = t.world_bounds();
        auto r = t.chunk_range(cs);
        EXPECT_LE(r.x_min, r.x_max);
        EXPECT_LE(r.y_min, r.y_max);

        // min-corner chunk contains the tile's min corner
        EXPECT_LE(static_cast<double>(r.x_min) * cs,       b.min_x);
        EXPECT_GT(static_cast<double>(r.x_min + 1) * cs,   b.min_x);
        // max chunk starts before max edge and its far side reaches it
        EXPECT_LT(static_cast<double>(r.x_max) * cs,       b.max_x);
        EXPECT_GE(static_cast<double>(r.x_max + 1) * cs,   b.max_x);
    }

    // planet tile starts at chunk (0,0)
    auto r0 = TileCoord{0, 0, 0}.chunk_range(cs);
    EXPECT_EQ(r0.x_min, 0);
    EXPECT_EQ(r0.y_min, 0);

    // deepest tile (~86 m) spans a 2-chunk block per axis
    auto rd = TileCoord{MAX_LEVEL, 0, 0}.chunk_range(cs);
    EXPECT_EQ(rd.count_x(), 2);
    EXPECT_EQ(rd.count_y(), 2);
}

// M013 — parent/child round-trip for random tiles across levels 0..MAX_LEVEL.
TEST(TileCoordTests, ParentChildRoundTripRandom) {
    std::mt19937_64 rng(0xC0FFEEULL);
    for (int iter = 0; iter < 2000; ++iter) {
        const int level   = std::uniform_int_distribution<int>(0, MAX_LEVEL)(rng);
        const int64_t span = int64_t{1} << level;  // valid index range [0, 2^level)
        const int64_t x = std::uniform_int_distribution<int64_t>(0, span - 1)(rng);
        const int64_t y = std::uniform_int_distribution<int64_t>(0, span - 1)(rng);
        const TileCoord t{level, x, y};

        // down then up: every child reports t as its parent
        if (level < MAX_LEVEL) {
            for (auto c : t.children())
                EXPECT_EQ(c.parent(), t);
        }
        // up then down: the parent's child at (x%2, y%2) is t again
        if (level >= 1) {
            const TileCoord back =
                t.parent().child(static_cast<int>(x & 1), static_cast<int>(y & 1));
            EXPECT_EQ(back, t);
        }
    }
}

// M002 — usable as an unordered_set / unordered_map key.
TEST(TileCoordTests, HashableAsKey) {
    std::unordered_set<TileCoord> s;
    s.insert({3, 10, 20});
    s.insert({3, 10, 20});  // duplicate — must collapse
    s.insert({3, 10, 21});
    EXPECT_EQ(s.size(), 2u);

    std::unordered_map<TileCoord, int> m;
    m[{0, 0, 0}] = 1;
    m[{0, 0, 0}] = 2;       // overwrite same key
    m[{1, 0, 0}] = 9;       // different level → different key
    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ((m[{0, 0, 0}]), 2);

    // equal coords hash equal
    std::hash<TileCoord> h;
    EXPECT_EQ(h(TileCoord{5, 1, 2}), h(TileCoord{5, 1, 2}));
}
