// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP9 tests. M142: RoadNetwork data model.
// M143: Roads::build() -- minimum-ish spanning network + redundant links.
// M144: Roads::build() -- roads avoid steep slopes / cross rivers at bridges.
// M145: Roads::exportCrossings() -- per-tile road crossings into TileEdge.
// M150: road network is connected across a continent's settlements.

#include <gtest/gtest.h>

#include <cstddef>
#include <vector>

#include "Map/RoadNetwork.hpp"

using namespace MeshWorld::Map;

TEST(RoadNetworkTest, DefaultNetworkIsEmpty) {
    RoadNetwork net;
    EXPECT_TRUE(net.empty());
    EXPECT_TRUE(net.nodes.empty());
    EXPECT_TRUE(net.edges.empty());
}

TEST(RoadNetworkTest, DefaultNodeIsAtOrigin) {
    RoadNode n;
    EXPECT_DOUBLE_EQ(n.x, 0.0);
    EXPECT_DOUBLE_EQ(n.z, 0.0);
}

TEST(RoadNetworkTest, DefaultEdgeHasNoEndpointsAndAnEmptyPath) {
    RoadEdge e;
    EXPECT_EQ(e.from, -1);
    EXPECT_EQ(e.to, -1);
    EXPECT_TRUE(e.path.empty());
}

TEST(RoadNetworkTest, NodeConstructAndAccess) {
    RoadNode n;
    n.x = 150.0;
    n.z = -75.0;
    EXPECT_DOUBLE_EQ(n.x, 150.0);
    EXPECT_DOUBLE_EQ(n.z, -75.0);
}

TEST(RoadNetworkTest, EdgeConstructAndAccessWithPath) {
    RoadEdge e;
    e.from = 0;
    e.to   = 1;
    e.path = {{0.0, 0.0}, {50.0, 10.0}, {100.0, 0.0}};

    EXPECT_EQ(e.from, 0);
    EXPECT_EQ(e.to, 1);
    ASSERT_EQ(e.path.size(), 3u);
    EXPECT_DOUBLE_EQ(e.path[1][0], 50.0);
    EXPECT_DOUBLE_EQ(e.path[1][1], 10.0);
}

TEST(RoadNetworkTest, NetworkWithOnlyNodesIsNotEmptyEvenWithNoEdges) {
    RoadNetwork net;
    net.nodes.push_back(RoadNode{100.0, 200.0});
    EXPECT_FALSE(net.empty());
    EXPECT_TRUE(net.edges.empty());
}

TEST(RoadNetworkTest, NetworkWithNodesAndEdgesReferencesNodesByIndex) {
    RoadNetwork net;
    net.nodes.push_back(RoadNode{0.0, 0.0});
    net.nodes.push_back(RoadNode{100.0, 0.0});
    net.nodes.push_back(RoadNode{100.0, 100.0});

    RoadEdge e1;
    e1.from = 0;
    e1.to   = 1;
    RoadEdge e2;
    e2.from = 1;
    e2.to   = 2;
    net.edges.push_back(e1);
    net.edges.push_back(e2);

    EXPECT_FALSE(net.empty());
    ASSERT_EQ(net.edges.size(), 2u);
    EXPECT_EQ(net.nodes[static_cast<std::size_t>(net.edges[0].to)].x, 100.0);
    EXPECT_EQ(net.nodes[static_cast<std::size_t>(net.edges[1].to)].z, 100.0);
}

// --- M143: Roads::build() ---

namespace {

Settlement settlement_at(double x, double z, SettlementTier tier) {
    Settlement s;
    s.x    = x;
    s.z    = z;
    s.tier = tier;
    return s;
}

int uf_find(std::vector<int>& parent, int x) {
    while (parent[static_cast<std::size_t>(x)] != x) {
        parent[static_cast<std::size_t>(x)] = parent[static_cast<std::size_t>(parent[static_cast<std::size_t>(x)])];
        x = parent[static_cast<std::size_t>(x)];
    }
    return x;
}

} // namespace

TEST(RoadsTest, BuildWithNoSettlementsReturnsEmptyNetwork) {
    SettlementNetwork settlements;
    const RoadNetwork net = Roads::build(settlements);
    EXPECT_TRUE(net.empty());
    EXPECT_TRUE(net.edges.empty());
}

TEST(RoadsTest, BuildWithOneQualifyingSettlementHasNoEdges) {
    SettlementNetwork settlements;
    settlements.settlements.push_back(settlement_at(0.0, 0.0, SettlementTier::Capital));

    const RoadNetwork net = Roads::build(settlements);
    ASSERT_EQ(net.nodes.size(), 1u);
    EXPECT_TRUE(net.edges.empty());
}

TEST(RoadsTest, BuildOnlyIncludesSettlementsAtOrAboveMaxTier) {
    SettlementNetwork settlements;
    settlements.settlements.push_back(settlement_at(0.0, 0.0, SettlementTier::Capital));
    settlements.settlements.push_back(settlement_at(100.0, 0.0, SettlementTier::City));
    settlements.settlements.push_back(settlement_at(200.0, 0.0, SettlementTier::Town));
    settlements.settlements.push_back(settlement_at(300.0, 0.0, SettlementTier::Village));

    const RoadNetwork net = Roads::build(settlements, SettlementTier::City);
    EXPECT_EQ(net.nodes.size(), 2u);  // capital + city only, town/village excluded
}

TEST(RoadsTest, BuildWithMaxTierVillageIncludesEverySettlement) {
    SettlementNetwork settlements;
    settlements.settlements.push_back(settlement_at(0.0, 0.0, SettlementTier::Capital));
    settlements.settlements.push_back(settlement_at(100.0, 0.0, SettlementTier::Town));
    settlements.settlements.push_back(settlement_at(200.0, 0.0, SettlementTier::Village));

    const RoadNetwork net = Roads::build(settlements, SettlementTier::Village);
    EXPECT_EQ(net.nodes.size(), 3u);
}

TEST(RoadsTest, BuildWithTwoSettlementsConnectsThemWithOneEdge) {
    SettlementNetwork settlements;
    settlements.settlements.push_back(settlement_at(0.0, 0.0, SettlementTier::Capital));
    settlements.settlements.push_back(settlement_at(100.0, 0.0, SettlementTier::Capital));

    const RoadNetwork net = Roads::build(settlements, SettlementTier::Capital, /*extra_redundant_links=*/0);
    ASSERT_EQ(net.edges.size(), 1u);
    EXPECT_EQ(net.edges[0].from, 0);
    EXPECT_EQ(net.edges[0].to, 1);
}

TEST(RoadsTest, BuildEdgePathIsAStraightTwoPointLineBetweenEndpoints) {
    SettlementNetwork settlements;
    settlements.settlements.push_back(settlement_at(10.0, 20.0, SettlementTier::Capital));
    settlements.settlements.push_back(settlement_at(110.0, 220.0, SettlementTier::Capital));

    const RoadNetwork net = Roads::build(settlements, SettlementTier::Capital, 0);
    ASSERT_EQ(net.edges.size(), 1u);
    ASSERT_EQ(net.edges[0].path.size(), 2u);
    EXPECT_DOUBLE_EQ(net.edges[0].path[0][0], 10.0);
    EXPECT_DOUBLE_EQ(net.edges[0].path[0][1], 20.0);
    EXPECT_DOUBLE_EQ(net.edges[0].path[1][0], 110.0);
    EXPECT_DOUBLE_EQ(net.edges[0].path[1][1], 220.0);
}

TEST(RoadsTest, BuildWithZeroExtraLinksProducesExactlyASpanningTree) {
    SettlementNetwork settlements;
    settlements.settlements.push_back(settlement_at(0.0, 0.0, SettlementTier::Capital));
    settlements.settlements.push_back(settlement_at(100.0, 0.0, SettlementTier::Capital));
    settlements.settlements.push_back(settlement_at(200.0, 0.0, SettlementTier::Capital));
    settlements.settlements.push_back(settlement_at(300.0, 0.0, SettlementTier::Capital));

    const RoadNetwork net = Roads::build(settlements, SettlementTier::Capital, /*extra_redundant_links=*/0);
    EXPECT_EQ(net.edges.size(), net.nodes.size() - 1);
}

TEST(RoadsTest, BuildAddsRequestedRedundantLinksBeyondTheSpanningTree) {
    // 4 settlements in a square: 6 possible edges total, the MST uses 3 --
    // 3 candidate edges remain to draw redundant links from.
    SettlementNetwork settlements;
    settlements.settlements.push_back(settlement_at(0.0, 0.0, SettlementTier::Capital));
    settlements.settlements.push_back(settlement_at(100.0, 0.0, SettlementTier::Capital));
    settlements.settlements.push_back(settlement_at(0.0, 100.0, SettlementTier::Capital));
    settlements.settlements.push_back(settlement_at(100.0, 100.0, SettlementTier::Capital));

    const RoadNetwork net = Roads::build(settlements, SettlementTier::Capital, /*extra_redundant_links=*/2);
    EXPECT_EQ(net.edges.size(), (net.nodes.size() - 1) + 2);
}

TEST(RoadsTest, BuildClampsRedundantLinksToWhatIsActuallyAvailable) {
    // Only 2 settlements -- one MST edge and zero possible redundant edges,
    // regardless of how many are requested.
    SettlementNetwork settlements;
    settlements.settlements.push_back(settlement_at(0.0, 0.0, SettlementTier::Capital));
    settlements.settlements.push_back(settlement_at(100.0, 0.0, SettlementTier::Capital));

    const RoadNetwork net = Roads::build(settlements, SettlementTier::Capital, /*extra_redundant_links=*/5);
    EXPECT_EQ(net.edges.size(), 1u);
}

TEST(RoadsTest, BuildConnectsEveryNodeIntoOneComponent) {
    SettlementNetwork settlements;
    settlements.settlements.push_back(settlement_at(0.0, 0.0, SettlementTier::Capital));
    settlements.settlements.push_back(settlement_at(100.0, 0.0, SettlementTier::Capital));
    settlements.settlements.push_back(settlement_at(0.0, 100.0, SettlementTier::Capital));
    settlements.settlements.push_back(settlement_at(100.0, 100.0, SettlementTier::Capital));

    const RoadNetwork net = Roads::build(settlements, SettlementTier::Capital, 0);

    std::vector<int> parent(net.nodes.size());
    for (std::size_t i = 0; i < parent.size(); ++i) parent[i] = static_cast<int>(i);
    for (const auto& e : net.edges) parent[static_cast<std::size_t>(uf_find(parent, e.from))] = uf_find(parent, e.to);

    const int root = uf_find(parent, 0);
    for (std::size_t i = 1; i < net.nodes.size(); ++i)
        EXPECT_EQ(uf_find(parent, static_cast<int>(i)), root) << "node " << i << " not connected to node 0";
}

// --- M150: road network is connected across a continent's settlements ---

// Realistic multi-settlement scenario via a real Settlements::place() call
// (not a hand-built SettlementNetwork) fed into Roads::build() -- confirms
// the single-connected-component property BuildConnectsEveryNodeIntoOneComponent
// already proves on a toy 4-node square also holds for a larger, more
// "continent-like" settlement layout.
TEST(RoadsTest, RoadNetworkIsConnectedAcrossARealisticMultiSettlementScenario) {
    constexpr int W = 20, H = 20;
    FieldGrid elevation;
    elevation.w = W;
    elevation.h = H;
    elevation.data.assign(static_cast<std::size_t>(W * H), 50.0f);  // uniform flat land, no ocean

    constexpr double world_extent = 2000.0;  // 100 m cells
    const SettlementNetwork settlements = Settlements::place(
        7, elevation, /*sea_level_m=*/-100.0, 0.0, 0.0, world_extent, world_extent,
        /*capital_count=*/2, /*city_count=*/4, /*town_count=*/0,
        /*min_spacing_m=*/150.0);
    ASSERT_GE(settlements.settlements.size(), 3u)
        << "fixture should place multiple settlements -- test would be vacuous otherwise";

    const RoadNetwork net = Roads::build(settlements, SettlementTier::City, /*extra_redundant_links=*/2);
    ASSERT_FALSE(net.nodes.empty());

    std::vector<int> parent(net.nodes.size());
    for (std::size_t i = 0; i < parent.size(); ++i) parent[i] = static_cast<int>(i);
    for (const auto& e : net.edges) parent[static_cast<std::size_t>(uf_find(parent, e.from))] = uf_find(parent, e.to);

    const int root = uf_find(parent, 0);
    for (std::size_t i = 1; i < net.nodes.size(); ++i)
        EXPECT_EQ(uf_find(parent, static_cast<int>(i)), root) << "node " << i << " not connected to node 0";
}

TEST(RoadsTest, BuildEdgeIndicesAreWithinNodeBounds) {
    SettlementNetwork settlements;
    settlements.settlements.push_back(settlement_at(0.0, 0.0, SettlementTier::Capital));
    settlements.settlements.push_back(settlement_at(100.0, 0.0, SettlementTier::Capital));
    settlements.settlements.push_back(settlement_at(50.0, 80.0, SettlementTier::Capital));

    const RoadNetwork net = Roads::build(settlements, SettlementTier::Capital, 1);
    for (const auto& e : net.edges) {
        EXPECT_GE(e.from, 0);
        EXPECT_LT(static_cast<std::size_t>(e.from), net.nodes.size());
        EXPECT_GE(e.to, 0);
        EXPECT_LT(static_cast<std::size_t>(e.to), net.nodes.size());
    }
}

TEST(RoadsTest, BuildIsDeterministic) {
    SettlementNetwork settlements;
    settlements.settlements.push_back(settlement_at(0.0, 0.0, SettlementTier::Capital));
    settlements.settlements.push_back(settlement_at(100.0, 0.0, SettlementTier::City));
    settlements.settlements.push_back(settlement_at(50.0, 80.0, SettlementTier::City));
    settlements.settlements.push_back(settlement_at(30.0, 40.0, SettlementTier::Capital));

    const RoadNetwork a = Roads::build(settlements, SettlementTier::City, 1);
    const RoadNetwork b = Roads::build(settlements, SettlementTier::City, 1);

    ASSERT_EQ(a.edges.size(), b.edges.size());
    for (std::size_t i = 0; i < a.edges.size(); ++i) {
        EXPECT_EQ(a.edges[i].from, b.edges[i].from);
        EXPECT_EQ(a.edges[i].to, b.edges[i].to);
    }
}

// --- M144: roads avoid steep slopes / cross rivers at bridges ---

namespace {

// A 5x3 grid, 100m cells (500x300 world). Flat everywhere (elevation 50)
// except column x=2, which is a "wall": rows 0 and 2 are 1000m higher
// (elevation diff 1000, far past the 30m default steep-slope threshold),
// row 1 is left at the flat baseline -- a single "pass" through the wall.
FieldGrid make_wall_with_pass_grid() {
    FieldGrid g;
    g.w = 5;
    g.h = 3;
    g.data.assign(15, 50.0f);
    g.data[static_cast<std::size_t>(0 * 5 + 2)] = 1050.0f;  // (x=2, y=0): wall
    g.data[static_cast<std::size_t>(2 * 5 + 2)] = 1050.0f;  // (x=2, y=2): wall
    // (x=2, y=1) stays 50.0f: the pass.
    return g;
}

SettlementNetwork make_west_east_capitals_row1() {
    SettlementNetwork s;
    Settlement        west;
    west.x    = 50.0;  // column 0 center
    west.z    = 150.0;  // row 1 center
    west.tier = SettlementTier::Capital;
    Settlement east;
    east.x    = 450.0;  // column 4 center
    east.z    = 150.0;  // row 1 center
    east.tier = SettlementTier::Capital;
    s.settlements.push_back(west);
    s.settlements.push_back(east);
    return s;
}

// True if every path point that falls within column `wall_gx`'s x-range
// also falls within row `pass_gy`'s z-range -- i.e. the road only crosses
// that column at the expected row.
bool path_crosses_column_only_at_row(const std::vector<std::array<double, 2>>& path, double cell_w,
                                      double cell_h, int wall_gx, int pass_gy) {
    for (const auto& p : path) {
        const int gx = static_cast<int>(p[0] / cell_w);
        if (gx != wall_gx) continue;
        const int gy = static_cast<int>(p[1] / cell_h);
        if (gy != pass_gy) return false;
    }
    return true;
}

} // namespace

TEST(RoadsTest, BuildWithoutElevationKeepsTheM143StraightLinePath) {
    const SettlementNetwork settlements = make_west_east_capitals_row1();

    // elevation == nullptr is the default -- routing must not engage.
    const RoadNetwork net = Roads::build(settlements, SettlementTier::Capital, 0);
    ASSERT_EQ(net.edges.size(), 1u);
    ASSERT_EQ(net.edges[0].path.size(), 2u);
}

TEST(RoadsTest, BuildOverFlatElevationConnectsExactEndpoints) {
    FieldGrid elevation;
    elevation.w = 5;
    elevation.h = 3;
    elevation.data.assign(15, 50.0f);
    const SettlementNetwork settlements = make_west_east_capitals_row1();

    const RoadNetwork net = Roads::build(settlements, SettlementTier::Capital, 0, &elevation,
                                          0.0, 0.0, 500.0, 300.0);
    ASSERT_EQ(net.edges.size(), 1u);
    ASSERT_FALSE(net.edges[0].path.empty());
    EXPECT_DOUBLE_EQ(net.edges[0].path.front()[0], 50.0);
    EXPECT_DOUBLE_EQ(net.edges[0].path.front()[1], 150.0);
    EXPECT_DOUBLE_EQ(net.edges[0].path.back()[0], 450.0);
    EXPECT_DOUBLE_EQ(net.edges[0].path.back()[1], 150.0);
}

TEST(RoadsTest, BuildRoutesThroughTheGridInsteadOfAStraightLineOnceElevationIsGiven) {
    FieldGrid elevation;
    elevation.w = 5;
    elevation.h = 3;
    elevation.data.assign(15, 50.0f);
    const SettlementNetwork settlements = make_west_east_capitals_row1();

    const RoadNetwork net = Roads::build(settlements, SettlementTier::Capital, 0, &elevation,
                                          0.0, 0.0, 500.0, 300.0);
    ASSERT_EQ(net.edges.size(), 1u);
    // A 5-cell-wide grid crossing has intermediate cell-center via-points,
    // not just the 2 endpoints M143 used.
    EXPECT_GT(net.edges[0].path.size(), 2u);
}

TEST(RoadsTest, SteepSlopeWallIsAvoidedInFavorOfThePass) {
    const FieldGrid          elevation   = make_wall_with_pass_grid();
    const SettlementNetwork settlements = make_west_east_capitals_row1();

    const RoadNetwork net = Roads::build(settlements, SettlementTier::Capital, 0, &elevation,
                                          0.0, 0.0, 500.0, 300.0);
    ASSERT_EQ(net.edges.size(), 1u);
    EXPECT_TRUE(path_crosses_column_only_at_row(net.edges[0].path, /*cell_w=*/100.0,
                                                 /*cell_h=*/100.0, /*wall_gx=*/2, /*pass_gy=*/1))
        << "road should funnel through the flat pass, not climb the wall";
}

TEST(RoadsTest, RiverWallIsAvoidedInFavorOfTheNonRiverGapEvenWithFlatElevation) {
    FieldGrid elevation;
    elevation.w = 5;
    elevation.h = 3;
    elevation.data.assign(15, 50.0f);  // uniform flat -- no slope cost anywhere
    const SettlementNetwork settlements = make_west_east_capitals_row1();

    HydrologyNetwork hydro;
    RiverSegment     river;
    river.points.push_back(RiverPoint{250.0, 50.0, 1.0f});   // (x=2, y=0)
    river.points.push_back(RiverPoint{250.0, 250.0, 1.0f});  // (x=2, y=2)
    // No river point at (x=2, y=1): that's the non-river gap.
    hydro.rivers.push_back(river);

    const RoadNetwork net = Roads::build(settlements, SettlementTier::Capital, 0, &elevation,
                                          0.0, 0.0, 500.0, 300.0, &hydro);
    ASSERT_EQ(net.edges.size(), 1u);
    EXPECT_TRUE(path_crosses_column_only_at_row(net.edges[0].path, /*cell_w=*/100.0,
                                                 /*cell_h=*/100.0, /*wall_gx=*/2, /*pass_gy=*/1))
        << "road should cross the river at the one bridgeable (non-river) gap";
}

TEST(RoadsTest, BuildWithInvalidWorldBoundsKeepsTheStraightLinePath) {
    FieldGrid elevation;
    elevation.w = 5;
    elevation.h = 3;
    elevation.data.assign(15, 50.0f);
    const SettlementNetwork settlements = make_west_east_capitals_row1();

    // world_x1 == world_x0 -- degenerate bounds, cell_w would be 0.
    const RoadNetwork net = Roads::build(settlements, SettlementTier::Capital, 0, &elevation,
                                          0.0, 0.0, 0.0, 300.0);
    ASSERT_EQ(net.edges.size(), 1u);
    EXPECT_EQ(net.edges[0].path.size(), 2u);
}

TEST(RoadsTest, BuildWithEmptyElevationGridKeepsTheStraightLinePath) {
    FieldGrid elevation;  // default-constructed: empty
    const SettlementNetwork settlements = make_west_east_capitals_row1();

    const RoadNetwork net = Roads::build(settlements, SettlementTier::Capital, 0, &elevation,
                                          0.0, 0.0, 500.0, 300.0);
    ASSERT_EQ(net.edges.size(), 1u);
    EXPECT_EQ(net.edges[0].path.size(), 2u);
}

TEST(RoadsTest, RoutedPathIsDeterministic) {
    const FieldGrid          elevation   = make_wall_with_pass_grid();
    const SettlementNetwork settlements = make_west_east_capitals_row1();

    const RoadNetwork a = Roads::build(settlements, SettlementTier::Capital, 0, &elevation,
                                        0.0, 0.0, 500.0, 300.0);
    const RoadNetwork b = Roads::build(settlements, SettlementTier::Capital, 0, &elevation,
                                        0.0, 0.0, 500.0, 300.0);

    ASSERT_EQ(a.edges[0].path.size(), b.edges[0].path.size());
    for (std::size_t i = 0; i < a.edges[0].path.size(); ++i) {
        EXPECT_DOUBLE_EQ(a.edges[0].path[i][0], b.edges[0].path[i][0]);
        EXPECT_DOUBLE_EQ(a.edges[0].path[i][1], b.edges[0].path[i][1]);
    }
}

// --- M145: Roads::exportCrossings() ---

namespace {

RoadNetwork network_with_one_path(std::vector<std::array<double, 2>> path) {
    RoadNetwork net;
    net.nodes.push_back({path.front()[0], path.front()[1]});
    net.nodes.push_back({path.back()[0], path.back()[1]});
    RoadEdge e;
    e.from = 0;
    e.to   = 1;
    e.path = std::move(path);
    net.edges.push_back(std::move(e));
    return net;
}

// Tile bounds used by every geometry test below: [0,500] x [0,300].
constexpr double kX0 = 0.0, kZ0 = 0.0, kX1 = 500.0, kZ1 = 300.0;

} // namespace

TEST(RoadsTest, ExportCrossingsWithNoEdgesLeavesEverythingEmpty) {
    RoadNetwork              net;
    std::array<TileEdge, 4> edges;
    Roads::exportCrossings(net, kX0, kZ0, kX1, kZ1, edges);
    for (const auto& e : edges) EXPECT_TRUE(e.crossings.empty());
}

TEST(RoadsTest, ExportCrossingsWithDegenerateBoundsIsANoOp) {
    const RoadNetwork       net = network_with_one_path({{100.0, -50.0}, {100.0, 50.0}});
    std::array<TileEdge, 4> edges;
    Roads::exportCrossings(net, /*world_x1 == world_x0*/ 0.0, 0.0, 0.0, kZ1, edges);
    for (const auto& e : edges) EXPECT_TRUE(e.crossings.empty());
}

TEST(RoadsTest, ExportCrossingsDetectsNorthCrossingAtCorrectPosition) {
    const RoadNetwork       net = network_with_one_path({{100.0, -50.0}, {100.0, 50.0}});
    std::array<TileEdge, 4> edges;
    Roads::exportCrossings(net, kX0, kZ0, kX1, kZ1, edges);

    ASSERT_EQ(edges[0].crossings.size(), 1u);  // N
    EXPECT_EQ(edges[0].crossings[0].type, EdgeCrossingType::Road);
    EXPECT_FLOAT_EQ(edges[0].crossings[0].position, 0.2f);  // x=100 of [0,500]
    EXPECT_TRUE(edges[1].crossings.empty());
    EXPECT_TRUE(edges[2].crossings.empty());
    EXPECT_TRUE(edges[3].crossings.empty());
}

TEST(RoadsTest, ExportCrossingsDetectsSouthCrossingAtCorrectPosition) {
    const RoadNetwork       net = network_with_one_path({{200.0, 250.0}, {200.0, 350.0}});
    std::array<TileEdge, 4> edges;
    Roads::exportCrossings(net, kX0, kZ0, kX1, kZ1, edges);

    ASSERT_EQ(edges[2].crossings.size(), 1u);  // S
    EXPECT_EQ(edges[2].crossings[0].type, EdgeCrossingType::Road);
    EXPECT_FLOAT_EQ(edges[2].crossings[0].position, 0.4f);  // x=200 of [0,500]
}

TEST(RoadsTest, ExportCrossingsDetectsWestCrossingAtCorrectPosition) {
    const RoadNetwork       net = network_with_one_path({{-50.0, 150.0}, {50.0, 150.0}});
    std::array<TileEdge, 4> edges;
    Roads::exportCrossings(net, kX0, kZ0, kX1, kZ1, edges);

    ASSERT_EQ(edges[3].crossings.size(), 1u);  // W
    EXPECT_EQ(edges[3].crossings[0].type, EdgeCrossingType::Road);
    EXPECT_FLOAT_EQ(edges[3].crossings[0].position, 0.5f);  // z=150 of [0,300]
}

TEST(RoadsTest, ExportCrossingsDetectsEastCrossingAtCorrectPosition) {
    const RoadNetwork       net = network_with_one_path({{450.0, 100.0}, {550.0, 100.0}});
    std::array<TileEdge, 4> edges;
    Roads::exportCrossings(net, kX0, kZ0, kX1, kZ1, edges);

    ASSERT_EQ(edges[1].crossings.size(), 1u);  // E
    EXPECT_EQ(edges[1].crossings[0].type, EdgeCrossingType::Road);
    EXPECT_NEAR(edges[1].crossings[0].position, 1.0f / 3.0f, 1e-6);  // z=100 of [0,300]
}

TEST(RoadsTest, ExportCrossingsIgnoresASegmentEntirelyInsideTheTile) {
    const RoadNetwork       net = network_with_one_path({{100.0, 100.0}, {200.0, 150.0}});
    std::array<TileEdge, 4> edges;
    Roads::exportCrossings(net, kX0, kZ0, kX1, kZ1, edges);
    for (const auto& e : edges) EXPECT_TRUE(e.crossings.empty());
}

TEST(RoadsTest, ExportCrossingsIgnoresASegmentEntirelyOutsideTheTile) {
    const RoadNetwork       net = network_with_one_path({{-100.0, -100.0}, {-50.0, -50.0}});
    std::array<TileEdge, 4> edges;
    Roads::exportCrossings(net, kX0, kZ0, kX1, kZ1, edges);
    for (const auto& e : edges) EXPECT_TRUE(e.crossings.empty());
}

TEST(RoadsTest, ExportCrossingsIsAdditiveAndPreservesExistingCrossings) {
    const RoadNetwork       net = network_with_one_path({{100.0, -50.0}, {100.0, 50.0}});
    std::array<TileEdge, 4> edges;
    edges[0].crossings.push_back({EdgeCrossingType::River, 0.75f});

    Roads::exportCrossings(net, kX0, kZ0, kX1, kZ1, edges);

    ASSERT_EQ(edges[0].crossings.size(), 2u);
    EXPECT_EQ(edges[0].crossings[0].type, EdgeCrossingType::River);
    EXPECT_FLOAT_EQ(edges[0].crossings[0].position, 0.75f);
    EXPECT_EQ(edges[0].crossings[1].type, EdgeCrossingType::Road);
    EXPECT_FLOAT_EQ(edges[0].crossings[1].position, 0.2f);
}

TEST(RoadsTest, ExportCrossingsHandlesMultipleEdgesAndMultipleCrossingPoints) {
    RoadNetwork net;
    net.nodes = {{100.0, -50.0}, {100.0, 350.0}, {-50.0, 150.0}, {550.0, 150.0}};
    RoadEdge crossesBoth;
    crossesBoth.from = 0;
    crossesBoth.to   = 1;
    crossesBoth.path = {{100.0, -50.0}, {100.0, 350.0}};  // crosses N then S
    RoadEdge crossesWestEast;
    crossesWestEast.from = 2;
    crossesWestEast.to   = 3;
    crossesWestEast.path = {{-50.0, 150.0}, {550.0, 150.0}};  // crosses W then E
    net.edges = {crossesBoth, crossesWestEast};

    std::array<TileEdge, 4> edges;
    Roads::exportCrossings(net, kX0, kZ0, kX1, kZ1, edges);

    EXPECT_EQ(edges[0].crossings.size(), 1u);  // N
    EXPECT_EQ(edges[1].crossings.size(), 1u);  // E
    EXPECT_EQ(edges[2].crossings.size(), 1u);  // S
    EXPECT_EQ(edges[3].crossings.size(), 1u);  // W
}
