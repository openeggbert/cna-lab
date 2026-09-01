// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP8 tests. M121: Hydrology river-network data model. M122: Hydrology::trace().
// M123: basin/lake filling.

#include <gtest/gtest.h>

#include <cmath>

#include "Map/Hydrology.hpp"

using namespace MeshWorld::Map;

namespace {

// A monotonic ramp: elevation depends only on gx, constant along gy, so
// every row is an independent, parallel, never-converging downhill path —
// no confluence, easy to reason about by hand. x=0 is highest (110), x=W-1
// is the only column at/below sea level (ocean mouth).
FieldGrid make_ramp_to_ocean(int W, int H) {
    FieldGrid g;
    g.w = W;
    g.h = H;
    g.data.resize(static_cast<std::size_t>(W * H));
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            g.data[static_cast<std::size_t>(y * W + x)] =
                static_cast<float>((W - 1 - x) * 30.0 - 10.0);
    return g;
}

// A 3x3 enclosed basin: an 8-cell ring at 100 m (all tied local maxima,
// each adjacent to the center) draining into a 50 m pit with no lower
// neighbor anywhere — every source dead-ends at the pit, none reach ocean.
FieldGrid make_enclosed_basin() {
    FieldGrid g;
    g.w = g.h = 3;
    g.data    = {100, 100, 100,
                 100,  50, 100,
                 100, 100, 100};
    return g;
}

// M133 — walks a segment's downstream_segment chain (following confluences,
// M124) to its terminus and returns that terminal segment's index.
// downstream_segment always points to a strictly earlier-processed segment
// (an already-completed segment's cells were the ones a later trace could
// have merged into), so the chain is a DAG on strictly decreasing indices
// and always terminates -- the `visited` guard is defensive, not load-
// bearing, and turns a hypothetical cycle into a test failure rather than
// an infinite loop.
int terminus_of(const HydrologyNetwork& net, int seg_idx) {
    std::vector<char> visited(net.rivers.size(), 0);
    while (visited[static_cast<std::size_t>(seg_idx)] == 0) {
        visited[static_cast<std::size_t>(seg_idx)] = 1;
        const RiverSegment& seg = net.rivers[static_cast<std::size_t>(seg_idx)];
        if (seg.downstream_segment < 0) return seg_idx;
        seg_idx = seg.downstream_segment;
    }
    ADD_FAILURE() << "downstream_segment chain cycles back on itself";
    return -1;
}

// M133 — asserts every segment's downstream_segment chain ends at a real
// mouth: reaches_ocean, or a lake. A chain ending with neither (a dead end
// with downstream_segment == -1 but reaches_ocean == false and
// lake_index == -1) would mean a river flows uphill or vanishes mid-land,
// which trace()/M123's basin-filling should never produce.
void expect_every_river_reaches_a_mouth(const HydrologyNetwork& net) {
    for (std::size_t i = 0; i < net.rivers.size(); ++i) {
        const int            term_idx = terminus_of(net, static_cast<int>(i));
        ASSERT_GE(term_idx, 0);
        const RiverSegment& term = net.rivers[static_cast<std::size_t>(term_idx)];
        EXPECT_TRUE(term.reaches_ocean || term.lake_index >= 0)
            << "segment " << i << "'s chain ends at segment " << term_idx
            << ", which is neither an ocean mouth nor a lake";
    }
}

} // namespace

TEST(HydrologyTest, DefaultNetworkIsEmpty) {
    HydrologyNetwork net;
    EXPECT_TRUE(net.empty());
    EXPECT_TRUE(net.rivers.empty());
    EXPECT_TRUE(net.lakes.empty());
}

// M121 — construct, populate, and read back a river segment: points,
// downstream linkage, and the ocean/lake mouth flag.
TEST(HydrologyTest, RiverSegmentConstructAndAccess) {
    RiverSegment seg;
    seg.points = {
        RiverPoint{0.0, 0.0, 1.0f},
        RiverPoint{10.0, 5.0, 2.5f},
        RiverPoint{20.0, 12.0, 4.0f},
    };
    seg.downstream_segment = -1;
    seg.reaches_ocean       = true;

    ASSERT_EQ(seg.points.size(), 3u);
    EXPECT_DOUBLE_EQ(seg.points[0].x, 0.0);
    EXPECT_DOUBLE_EQ(seg.points[2].z, 12.0);
    // Flow grows monotonically downstream, by construction of this fixture.
    EXPECT_LT(seg.points[0].flow, seg.points[1].flow);
    EXPECT_LT(seg.points[1].flow, seg.points[2].flow);
    EXPECT_EQ(seg.downstream_segment, -1);
    EXPECT_TRUE(seg.reaches_ocean);
}

// M121 — confluence: one segment's downstream_segment index names the
// segment it joins (a tributary flowing into a larger river).
TEST(HydrologyTest, ConfluenceLinksTwoSegments) {
    HydrologyNetwork net;

    RiverSegment main_stem;
    main_stem.points = {RiverPoint{0.0, 0.0, 5.0f}, RiverPoint{5.0, 5.0, 8.0f}};
    main_stem.reaches_ocean = true;
    net.rivers.push_back(main_stem);  // index 0

    RiverSegment tributary;
    tributary.points = {RiverPoint{-5.0, -5.0, 1.0f}, RiverPoint{0.0, 0.0, 2.0f}};
    tributary.downstream_segment = 0;  // flows into main_stem
    net.rivers.push_back(tributary);   // index 1

    ASSERT_EQ(net.rivers.size(), 2u);
    EXPECT_EQ(net.rivers[1].downstream_segment, 0);
    EXPECT_EQ(net.rivers[net.rivers[1].downstream_segment].points.front().x, 0.0);
    EXPECT_FALSE(net.empty());
}

// M121 — lake: closed basin with a shoreline polygon and a spill elevation.
TEST(HydrologyTest, LakeConstructAndAccess) {
    Lake lake;
    lake.x                   = 100.0;
    lake.z                   = 200.0;
    lake.surface_elevation_m = 50.0;
    lake.shoreline           = {{95.0, 195.0}, {105.0, 195.0}, {105.0, 205.0}, {95.0, 205.0}};

    EXPECT_DOUBLE_EQ(lake.x, 100.0);
    EXPECT_DOUBLE_EQ(lake.surface_elevation_m, 50.0);
    ASSERT_EQ(lake.shoreline.size(), 4u);
    EXPECT_DOUBLE_EQ(lake.shoreline[2][0], 105.0);
}

TEST(HydrologyTest, NetworkWithRiversAndLakesIsNotEmpty) {
    HydrologyNetwork net;
    net.rivers.push_back(RiverSegment{});
    EXPECT_FALSE(net.empty());

    HydrologyNetwork lake_only;
    lake_only.lakes.push_back(Lake{});
    EXPECT_FALSE(lake_only.empty());
}

// --- M122: Hydrology::trace() ---

// M122 — an all-ocean field has no land, so no sources and an empty network.
TEST(HydrologyTest, TraceOverAllOceanProducesEmptyNetwork) {
    FieldGrid g;
    g.w = g.h = 3;
    g.data.assign(9, -100.0f);

    const HydrologyNetwork net = Hydrology::trace(g, /*sea_level_m=*/0.0, 0.0, 0.0, 300.0, 300.0);
    EXPECT_TRUE(net.empty());
}

// M122 — every row of the ramp is its own source-to-ocean segment: flow
// increases monotonically, the segment ends at the ocean, and RiverPoint
// world coordinates match the (gx+0.5)*cell_size conversion.
TEST(HydrologyTest, TraceOverRampReachesOceanWithIncreasingFlow) {
    constexpr int W = 5, H = 3;
    const FieldGrid g = make_ramp_to_ocean(W, H);

    const HydrologyNetwork net = Hydrology::trace(g, /*sea_level_m=*/0.0,
                                                    0.0, 0.0, 500.0, 300.0);  // 100 m cells
    ASSERT_EQ(net.rivers.size(), static_cast<std::size_t>(H))
        << "one source per row (tied plateau at gx=0)";

    for (const auto& seg : net.rivers) {
        ASSERT_EQ(seg.points.size(), static_cast<std::size_t>(W));
        EXPECT_TRUE(seg.reaches_ocean);
        EXPECT_EQ(seg.downstream_segment, -1);
        for (std::size_t i = 1; i < seg.points.size(); ++i)
            EXPECT_LT(seg.points[i - 1].flow, seg.points[i].flow)
                << "flow must increase monotonically downstream";
        // Source point (gx=0) world x = 0 + (0+0.5)*100 = 50.
        EXPECT_DOUBLE_EQ(seg.points.front().x, 50.0);
        // Mouth point (gx=W-1=4) world x = 0 + (4+0.5)*100 = 450.
        EXPECT_DOUBLE_EQ(seg.points.back().x, 450.0);
    }
}

// --- M123: lakes in basins / endorheic sinks ---

// M123 — a landlocked pit with no path to the ocean anywhere in the grid
// (a fully enclosed endorheic basin) fills into a single Lake shared by
// every segment that dead-ends there; its spill elevation is the highest
// point the flood-fill had to absorb (here: the entire 3x3 grid, since
// there's no ocean cell anywhere in it at all).
TEST(HydrologyTest, TraceOverEnclosedBasinFillsASharedEndorheicLake) {
    const FieldGrid g = make_enclosed_basin();

    const HydrologyNetwork net = Hydrology::trace(g, /*sea_level_m=*/0.0, 0.0, 0.0, 300.0, 300.0);
    ASSERT_EQ(net.rivers.size(), 8u) << "the 8-cell ring are all tied local maxima";
    ASSERT_EQ(net.lakes.size(), 1u) << "all 8 segments dead-end at the same pit -> one shared lake";

    for (const auto& seg : net.rivers) {
        EXPECT_FALSE(seg.reaches_ocean);
        EXPECT_EQ(seg.lake_index, 0);
        ASSERT_EQ(seg.points.size(), 2u) << "ring cell -> center pit, then stuck";
        EXPECT_LT(seg.points[0].flow, seg.points[1].flow);
    }

    const Lake& lake = net.lakes[0];
    EXPECT_DOUBLE_EQ(lake.surface_elevation_m, 100.0)
        << "no ocean outlet anywhere in this grid, so the whole 3x3 basin is absorbed";
    EXPECT_EQ(lake.shoreline.size(), 9u) << "every cell of the fully-enclosed grid";
    // 100 m cells over a 0..300 world-bounds grid centers this basin at (150,150).
    EXPECT_NEAR(lake.x, 150.0, 1e-9);
    EXPECT_NEAR(lake.z, 150.0, 1e-9);
}

// M-fix — without a cap, a broad landlocked area with no ocean access
// anywhere floods almost the entire tile into one giant "lake" (confirmed on
// a real 100%-land 64x64 map tile, 6 separate lake names). This grid
// reproduces that shape at map scale: a smooth bowl (elevation grows with
// distance from center), entirely above sea level, so the flood-fill from
// the center pit has nowhere to stop except by exhausting the whole
// 64x64=4096-cell grid -- exactly the pre-fix failure mode. The fix bounds
// the flooded set to a fraction of the grid instead.
TEST(HydrologyTest, TraceOverAVastLandlockedBowlCapsTheLakeInsteadOfFloodingTheWholeGrid) {
    constexpr int N = 64;
    FieldGrid g;
    g.w = g.h = N;
    g.data.resize(static_cast<std::size_t>(N * N));
    const double cx = (N - 1) / 2.0;
    const double cy = (N - 1) / 2.0;
    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            const double dist = std::hypot(static_cast<double>(x) - cx, static_cast<double>(y) - cy);
            g.data[static_cast<std::size_t>(y * N + x)] = static_cast<float>(10.0 + dist);
        }
    }

    const HydrologyNetwork net = Hydrology::trace(g, /*sea_level_m=*/0.0, 0.0, 0.0, 6400.0, 6400.0);
    ASSERT_FALSE(net.lakes.empty());

    std::size_t total_flooded = 0;
    for (const auto& lake : net.lakes) total_flooded += lake.shoreline.size();
    EXPECT_LT(total_flooded, static_cast<std::size_t>(N * N) / 2)
        << "a landlocked bowl covering the whole tile must not flood into one giant lake ("
        << total_flooded << "/" << (N * N) << " cells flooded)";
}

// M-fix regression guard — the cap must not shrink a genuinely SMALL
// enclosed basin (well under the cap already) at all; only an
// unrealistically vast one gets bounded. Reuses the exact 3x3 fixture
// TraceOverEnclosedBasinFillsASharedEndorheicLake already asserts floods
// 100% (9/9 cells) -- this just makes the "small basins are untouched"
// property explicit as its own test, separate from that one's own main
// assertion.
TEST(HydrologyTest, TraceOverASmallEnclosedBasinIsNotShrunkByTheAreaCap) {
    const FieldGrid g = make_enclosed_basin();
    const HydrologyNetwork net = Hydrology::trace(g, /*sea_level_m=*/0.0, 0.0, 0.0, 300.0, 300.0);

    ASSERT_EQ(net.lakes.size(), 1u);
    EXPECT_EQ(net.lakes[0].shoreline.size(), 9u)
        << "a 3x3 fully-enclosed basin is nowhere near the area cap's floor and must flood entirely";
}

// M123 — a basin with a real (if roundabout) path to the ocean spills at
// the lowest ridge crest on that path, not at the pit's own elevation and
// not at the much-higher surrounding walls.
TEST(HydrologyTest, TraceOverBasinWithRidgeOutletFindsCorrectSpillElevation) {
    // Row1 walks 300(wall) -> 50(pit, dead end since 150 > 50) -> 150(ridge
    // crest, the only way out) -> 20(downhill again) -> -50(ocean).
    FieldGrid g;
    g.w = 5;
    g.h = 3;
    g.data = {
        300, 300, 300, 300, 300,
        300,  50, 150,  20, -50,
        300, 300, 300, 300, 300,
    };

    const HydrologyNetwork net = Hydrology::trace(g, /*sea_level_m=*/0.0, 0.0, 0.0, 500.0, 300.0);

    bool found_ridge_spill = false;
    for (const auto& lake : net.lakes)
        if (std::abs(lake.surface_elevation_m - 150.0) < 1e-6) found_ridge_spill = true;
    EXPECT_TRUE(found_ridge_spill)
        << "expected a lake whose spill elevation is the 150 m ridge crest, not the 300 m walls";

    bool found_ocean_segment = false;
    for (const auto& seg : net.rivers)
        if (seg.reaches_ocean) found_ocean_segment = true;
    EXPECT_TRUE(found_ocean_segment)
        << "the (2,0)-style sources should still reach ocean directly via steepest descent";
}

TEST(HydrologyTest, TraceIsDeterministic) {
    const FieldGrid g = make_ramp_to_ocean(5, 3);
    const HydrologyNetwork a = Hydrology::trace(g, 0.0, 0.0, 0.0, 500.0, 300.0);
    const HydrologyNetwork b = Hydrology::trace(g, 0.0, 0.0, 0.0, 500.0, 300.0);

    ASSERT_EQ(a.rivers.size(), b.rivers.size());
    for (std::size_t i = 0; i < a.rivers.size(); ++i) {
        ASSERT_EQ(a.rivers[i].points.size(), b.rivers[i].points.size());
        for (std::size_t j = 0; j < a.rivers[i].points.size(); ++j) {
            EXPECT_DOUBLE_EQ(a.rivers[i].points[j].x, b.rivers[i].points[j].x);
            EXPECT_DOUBLE_EQ(a.rivers[i].points[j].z, b.rivers[i].points[j].z);
            EXPECT_FLOAT_EQ(a.rivers[i].points[j].flow, b.rivers[i].points[j].flow);
        }
    }
}

TEST(HydrologyTest, TraceOverEmptyGridReturnsEmptyNetwork) {
    FieldGrid g;  // default-constructed: empty
    const HydrologyNetwork net = Hydrology::trace(g, 0.0, 0.0, 0.0, 100.0, 100.0);
    EXPECT_TRUE(net.empty());
}

// --- M124: confluence detection + accumulated flow ---

// A "Y" confluence: two ridge peaks, (0,0) and (0,2), each descend
// diagonally straight into a shared channel cell (1,1) -- it's the true
// elevation minimum among all 8 neighbors of both peaks, not merely a tied
// cardinal option, so steepest descent reaches it in one step. From there
// the channel alone continues to the ocean at (2,1). Hand-traced (see
// M124 commit for the full derivation):
//   source (0,0) [seg 0, processed first]:  (0,0) -> (1,1) -> (2,1)=ocean
//   source (0,2) [seg 1, processed second]: (0,2) -> (1,1)=confluence
// Seg 1 reaches (1,1) after seg 0 already claimed it (seg 0's 2nd point),
// so seg 1 stops there and defers to seg 0 instead of re-tracing the
// identical (1,1)->(2,1) tail.
FieldGrid make_y_confluence() {
    FieldGrid g;
    g.w = g.h = 3;
    g.data    = {
        100, 50, 30,    // y=0: source ridge A, descending toward the channel
         80, 20, -10,   // y=1: channel row; (1,1) is the confluence, (2,1) is ocean
        100, 50, 30,    // y=2: source ridge B, mirrors y=0
    };
    return g;
}

TEST(HydrologyTest, TraceMergesTwoTributariesAtAConfluence) {
    const FieldGrid g = make_y_confluence();

    const HydrologyNetwork net = Hydrology::trace(g, /*sea_level_m=*/0.0,
                                                    0.0, 0.0, 300.0, 300.0);  // 100 m cells
    ASSERT_EQ(net.rivers.size(), 2u) << "(0,0) and (0,2) are the only local maxima";

    const RiverSegment& main_stem = net.rivers[0];  // (0,0): scanned first
    const RiverSegment& trib      = net.rivers[1];  // (0,2): scanned second, merges into seg 0

    EXPECT_EQ(main_stem.downstream_segment, -1);
    EXPECT_TRUE(main_stem.reaches_ocean);
    ASSERT_EQ(main_stem.points.size(), 3u);

    EXPECT_EQ(trib.downstream_segment, 0) << "merges into the main stem, not a duplicate ocean path";
    EXPECT_FALSE(trib.reaches_ocean) << "a tributary's own terminus is the confluence, not the ocean";
    EXPECT_EQ(trib.lake_index, -1);
    ASSERT_EQ(trib.points.size(), 2u) << "own approach only: (0,2)->(1,1); no duplicated ocean tail";

    // The confluence cell (1,1): both segments record their own copy of it,
    // at the same world position.
    EXPECT_DOUBLE_EQ(trib.points.back().x, main_stem.points[1].x);
    EXPECT_DOUBLE_EQ(trib.points.back().z, main_stem.points[1].z);

    // Before the merge, the main stem's flow is its own running count.
    EXPECT_FLOAT_EQ(main_stem.points[0].flow, 1.0f);
    // The tributary's own flow (1, 2) is unaffected by propagation -- only
    // the segment it merges INTO is updated.
    EXPECT_FLOAT_EQ(trib.points[0].flow, 1.0f);
    EXPECT_FLOAT_EQ(trib.points[1].flow, 2.0f);
    // From the confluence onward, the main stem absorbs the tributary's
    // total flow (2.0) on top of its own running count (2, then 3).
    EXPECT_FLOAT_EQ(main_stem.points[1].flow, 4.0f);
    EXPECT_FLOAT_EQ(main_stem.points[2].flow, 5.0f);
}

// --- M133: every river reaches an ocean or a lake ---

TEST(HydrologyTest, EveryRiverInTheRampReachesTheOcean) {
    const FieldGrid         g   = make_ramp_to_ocean(5, 3);
    const HydrologyNetwork  net = Hydrology::trace(g, 0.0, 0.0, 0.0, 500.0, 300.0);
    ASSERT_FALSE(net.rivers.empty());
    expect_every_river_reaches_a_mouth(net);
}

TEST(HydrologyTest, EveryRiverInTheEnclosedBasinReachesItsLake) {
    const FieldGrid        g   = make_enclosed_basin();
    const HydrologyNetwork net = Hydrology::trace(g, 0.0, 0.0, 0.0, 300.0, 300.0);
    ASSERT_FALSE(net.rivers.empty());
    expect_every_river_reaches_a_mouth(net);
}

TEST(HydrologyTest, EveryRiverInTheRidgeOutletBasinReachesAMouth) {
    FieldGrid g;
    g.w = 5;
    g.h = 3;
    g.data = {
        300, 300, 300, 300, 300,
        300,  50, 150,  20, -50,
        300, 300, 300, 300, 300,
    };
    const HydrologyNetwork net = Hydrology::trace(g, 0.0, 0.0, 0.0, 500.0, 300.0);
    ASSERT_FALSE(net.rivers.empty());
    expect_every_river_reaches_a_mouth(net);
}

TEST(HydrologyTest, EveryRiverInAConfluenceReachesAMouth) {
    const FieldGrid        g   = make_y_confluence();
    const HydrologyNetwork net = Hydrology::trace(g, 0.0, 0.0, 0.0, 300.0, 300.0);
    ASSERT_FALSE(net.rivers.empty());
    expect_every_river_reaches_a_mouth(net);
}

// --- M125: Hydrology::carve() ---

TEST(HydrologyTest, CarveOverEmptyNetworkIsANoOp) {
    FieldGrid g = make_ramp_to_ocean(5, 3);
    const FieldGrid original = g;

    HydrologyNetwork empty_net;
    Hydrology::carve(g, empty_net, 0.0, 0.0, 0.0, 500.0, 300.0);

    EXPECT_EQ(g.data, original.data);
}

TEST(HydrologyTest, CarveLowersInteriorCellsButNeverTouchesTheGridEdge) {
    constexpr int W = 7, H = 5;
    FieldGrid g = make_ramp_to_ocean(W, H);
    const FieldGrid original = g;

    const HydrologyNetwork net = Hydrology::trace(g, /*sea_level_m=*/0.0,
                                                    0.0, 0.0, 700.0, 500.0);  // 100 m cells
    Hydrology::carve(g, net, 0.0, 0.0, 0.0, 700.0, 500.0);

    // Edges (gx=0, gx=W-1, gy=0, gy=H-1) are byte-for-byte identical to the
    // pre-carve grid, even though (0,y) sources and (W-1,y) ocean mouths are
    // literally on those edges.
    for (int x = 0; x < W; ++x) {
        EXPECT_FLOAT_EQ(g.at(x, 0), original.at(x, 0)) << "x=" << x << " y=0 (N edge)";
        EXPECT_FLOAT_EQ(g.at(x, H - 1), original.at(x, H - 1)) << "x=" << x << " y=H-1 (S edge)";
    }
    for (int y = 0; y < H; ++y) {
        EXPECT_FLOAT_EQ(g.at(0, y), original.at(0, y)) << "x=0 (W edge) y=" << y;
        EXPECT_FLOAT_EQ(g.at(W - 1, y), original.at(W - 1, y)) << "x=W-1 (E edge) y=" << y;
    }

    // A middle row's interior cells, which the traced river actually
    // crosses, are strictly lower than before carving.
    const int y = H / 2;
    for (int x = 1; x < W - 1; ++x)
        EXPECT_LT(g.at(x, y), original.at(x, y)) << "x=" << x << " y=" << y;

    // Flow (and therefore carve depth) grows toward the mouth, so the cell
    // near the mouth is carved deeper than the cell near the source.
    const float reduction_near_source = original.at(1, y) - g.at(1, y);
    const float reduction_near_mouth  = original.at(W - 2, y) - g.at(W - 2, y);
    EXPECT_LT(reduction_near_source, reduction_near_mouth);

    // No cell that started above sea level was carved down to/below it.
    for (int yy = 0; yy < H; ++yy)
        for (int xx = 0; xx < W; ++xx)
            if (original.at(xx, yy) > 0.0f) EXPECT_GT(g.at(xx, yy), 0.0f) << "x=" << xx << " y=" << yy;
}

TEST(HydrologyTest, CarveOnTooSmallGridIsANoOp) {
    // 2x2 has no interior cell at all (every cell is on some edge) -- carve
    // must not attempt to write anywhere, let alone out of bounds.
    FieldGrid g;
    g.w = g.h = 2;
    g.data    = {100, 100, 100, -10};
    const FieldGrid original = g;

    const HydrologyNetwork net = Hydrology::trace(g, 0.0, 0.0, 0.0, 200.0, 200.0);
    Hydrology::carve(g, net, 0.0, 0.0, 0.0, 200.0, 200.0);

    EXPECT_EQ(g.data, original.data);
}

// --- M345 (MAP22): RiverSegment::exits_tile / Hydrology::exportCrossings() ---

namespace {

// Unique elevations everywhere (no ties -> exactly one source), engineered
// so the single traced river's steepest descent lands on (1,0) -- the
// tile's own North edge, at a non-corner column so it registers on exactly
// one edge. (2,2)=100 is the sole local max: 100 -> (1,1)=20 -> (1,0)=10,
// where (1,0)'s own neighbors {15,16,50,20,55} are all higher than 10 --
// no lower in-bounds neighbor, but (1,0) sits on gy=0, this tile's own
// outer row, not a genuine interior pit.
FieldGrid make_single_north_exit_grid() {
    FieldGrid g;
    g.w = g.h = 3;
    g.data    = {
        15, 10, 16,
        50, 20, 55,
        90, 95, 100,
    };
    return g;
}

} // namespace

TEST(HydrologyTest, TraceMarksABoundaryDeadEndAsExitsTileNotALake) {
    const FieldGrid g = make_single_north_exit_grid();
    const HydrologyNetwork net = Hydrology::trace(g, /*sea_level_m=*/-1000.0, 0.0, 0.0, 300.0, 300.0);

    ASSERT_EQ(net.rivers.size(), 1u);
    const RiverSegment& seg = net.rivers[0];
    EXPECT_TRUE(seg.exits_tile);
    EXPECT_FALSE(seg.reaches_ocean);
    EXPECT_EQ(seg.lake_index, -1);
    EXPECT_TRUE(net.lakes.empty()) << "a tile-edge dead end must not be basin-filled into a spurious lake";
    ASSERT_EQ(seg.points.size(), 3u) << "(2,2) -> (1,1) -> (1,0)";
}

TEST(HydrologyTest, TraceOverEnclosedBasinStillFillsALakeWhenThePitIsGenuinelyInterior) {
    // Regression guard for M345: an INTERIOR pit (make_enclosed_basin()'s
    // own center cell, not on any grid boundary) must still basin-fill
    // into a real Lake exactly as before -- exits_tile is for tile-edge
    // dead ends only, not a blanket replacement for M123's own mechanism.
    const FieldGrid g = make_enclosed_basin();
    const HydrologyNetwork net = Hydrology::trace(g, /*sea_level_m=*/0.0, 0.0, 0.0, 300.0, 300.0);

    ASSERT_EQ(net.lakes.size(), 1u);
    for (const auto& seg : net.rivers) {
        EXPECT_FALSE(seg.exits_tile);
        EXPECT_EQ(seg.lake_index, 0);
    }
}

TEST(HydrologyTest, ExportCrossingsWithEmptyNetworkLeavesEverythingEmpty) {
    std::array<TileEdge, 4> edges;
    const HydrologyNetwork  empty_net;
    Hydrology::exportCrossings(empty_net, 3, 3, 0.0, 0.0, 300.0, 300.0, edges);
    for (const auto& e : edges) EXPECT_TRUE(e.crossings.empty());
}

TEST(HydrologyTest, ExportCrossingsWithDegenerateBoundsIsANoOp) {
    const FieldGrid        g   = make_single_north_exit_grid();
    const HydrologyNetwork net = Hydrology::trace(g, -1000.0, 0.0, 0.0, 300.0, 300.0);

    std::array<TileEdge, 4> edges;
    Hydrology::exportCrossings(net, 3, 3, /*world_x1 == world_x0*/ 0.0, 0.0, 0.0, 300.0, edges);
    for (const auto& e : edges) EXPECT_TRUE(e.crossings.empty());
}

TEST(HydrologyTest, ExportCrossingsRegistersTheExitOnlyOnTheNorthEdgeAtTheCorrectPosition) {
    const FieldGrid        g   = make_single_north_exit_grid();
    const HydrologyNetwork net = Hydrology::trace(g, -1000.0, 0.0, 0.0, 300.0, 300.0);
    ASSERT_TRUE(net.rivers[0].exits_tile);

    std::array<TileEdge, 4> edges;
    Hydrology::exportCrossings(net, 3, 3, 0.0, 0.0, 300.0, 300.0, edges);

    ASSERT_EQ(edges[0].crossings.size(), 1u) << "North";
    EXPECT_EQ(edges[0].crossings[0].type, EdgeCrossingType::River);
    EXPECT_NEAR(edges[0].crossings[0].position, 0.5f, 1e-6f)  // gx=1 of 3 -> (1+0.5)/3
        << "the exit cell is the middle column";
    EXPECT_TRUE(edges[1].crossings.empty()) << "East";
    EXPECT_TRUE(edges[2].crossings.empty()) << "South";
    EXPECT_TRUE(edges[3].crossings.empty()) << "West";
}

TEST(HydrologyTest, ExportCrossingsIsAdditiveAndPreservesExistingCrossings) {
    const FieldGrid        g   = make_single_north_exit_grid();
    const HydrologyNetwork net = Hydrology::trace(g, -1000.0, 0.0, 0.0, 300.0, 300.0);

    std::array<TileEdge, 4> edges;
    edges[1].crossings.push_back({EdgeCrossingType::Road, 0.25f});  // pre-existing, from Roads::exportCrossings()

    Hydrology::exportCrossings(net, 3, 3, 0.0, 0.0, 300.0, 300.0, edges);

    ASSERT_EQ(edges[1].crossings.size(), 1u) << "untouched -- no river ever reached the East edge";
    EXPECT_EQ(edges[1].crossings[0].type, EdgeCrossingType::Road);
    ASSERT_EQ(edges[0].crossings.size(), 1u) << "the river's own North exit, added alongside the pre-existing Road entry";
}
