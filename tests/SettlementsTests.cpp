// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP9 tests. M137: Settlements data model. M138: Settlements::place().
// M148: Settlements::name() -- naming; Settlements::appendLabels() -- persistence.
// M152: city placement avoids ocean / extreme elevation.

#include <gtest/gtest.h>

#include <cmath>
#include <set>

#include "Map/Settlements.hpp"

using namespace MeshWorld::Map;

namespace {

double distance(const Settlement& a, const Settlement& b) {
    const double dx = a.x - b.x;
    const double dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

int count_tier(const SettlementNetwork& net, SettlementTier tier) {
    int n = 0;
    for (const auto& s : net.settlements)
        if (s.tier == tier) ++n;
    return n;
}

} // namespace

TEST(SettlementsTest, DefaultNetworkIsEmpty) {
    SettlementNetwork net;
    EXPECT_TRUE(net.empty());
    EXPECT_TRUE(net.settlements.empty());
}

// M137 — construct, populate, and read back a settlement record.
TEST(SettlementsTest, SettlementConstructAndAccess) {
    Settlement s;
    s.x    = 100.0;
    s.z    = 200.0;
    s.tier = SettlementTier::Capital;
    s.name = "Vorhavn";

    EXPECT_DOUBLE_EQ(s.x, 100.0);
    EXPECT_DOUBLE_EQ(s.z, 200.0);
    EXPECT_EQ(s.tier, SettlementTier::Capital);
    EXPECT_EQ(s.name, "Vorhavn");
}

TEST(SettlementsTest, DefaultSettlementIsAnUnnamedVillageAtOrigin) {
    Settlement s;
    EXPECT_DOUBLE_EQ(s.x, 0.0);
    EXPECT_DOUBLE_EQ(s.z, 0.0);
    EXPECT_EQ(s.tier, SettlementTier::Village);
    EXPECT_TRUE(s.name.empty());
}

TEST(SettlementsTest, NetworkWithASettlementIsNotEmpty) {
    SettlementNetwork net;
    net.settlements.push_back(Settlement{});
    EXPECT_FALSE(net.empty());
}

TEST(SettlementsTest, AllFourTiersAreDistinct) {
    EXPECT_NE(SettlementTier::Capital, SettlementTier::City);
    EXPECT_NE(SettlementTier::City, SettlementTier::Town);
    EXPECT_NE(SettlementTier::Town, SettlementTier::Village);
    EXPECT_NE(SettlementTier::Capital, SettlementTier::Village);
}

// --- M138: Settlements::place() ---

TEST(SettlementsTest, PlaceOverEmptyGridReturnsEmptyNetwork) {
    FieldGrid elevation;  // default-constructed: empty
    const SettlementNetwork net =
        Settlements::place(42, elevation, 0.0, 0.0, 0.0, 100.0, 100.0, 1, 1, 1, 10.0);
    EXPECT_TRUE(net.empty());
}

TEST(SettlementsTest, PlaceWithZeroCountsReturnsEmptyNetwork) {
    FieldGrid elevation;
    elevation.w = elevation.h = 4;
    elevation.data.assign(16, 50.0f);
    const SettlementNetwork net =
        Settlements::place(42, elevation, 0.0, 0.0, 0.0, 400.0, 400.0, 0, 0, 0, 10.0);
    EXPECT_TRUE(net.empty());
}

TEST(SettlementsTest, PlaceOverAllOceanReturnsEmptyNetwork) {
    FieldGrid elevation;
    elevation.w = elevation.h = 4;
    elevation.data.assign(16, -100.0f);
    const SettlementNetwork net =
        Settlements::place(42, elevation, 0.0, 0.0, 0.0, 400.0, 400.0, 2, 2, 2, 10.0);
    EXPECT_TRUE(net.empty());
}

TEST(SettlementsTest, PlaceExcludesTheOnlyLandCellWhenTooHighAboveSeaLevel) {
    // Only the center cell is land, but it's a 5000 m peak -- disqualified
    // outright regardless of any coastal/flatness bonus, so no candidates
    // survive at all.
    FieldGrid elevation;
    elevation.w = elevation.h = 3;
    elevation.data = {-10, -10, -10,
                       -10, 5000, -10,
                       -10, -10, -10};
    const SettlementNetwork net = Settlements::place(
        42, elevation, /*sea_level_m=*/0.0, 0.0, 0.0, 300.0, 300.0,
        /*capital_count=*/1, /*city_count=*/0, /*town_count=*/0,
        /*min_spacing_m=*/10.0, /*max_settlement_elevation_m=*/1200.0);
    EXPECT_TRUE(net.empty());
}

TEST(SettlementsTest, PlaceNeverPicksAnOceanCell) {
    constexpr int W = 6, H = 6;
    FieldGrid elevation;
    elevation.w = W;
    elevation.h = H;
    elevation.data.assign(static_cast<std::size_t>(W * H), 50.0f);
    for (int y = 0; y < H; ++y) elevation.data[static_cast<std::size_t>(y * W)] = -10.0f;  // column 0 is ocean

    const SettlementNetwork net =
        Settlements::place(7, elevation, 0.0, 0.0, 0.0, 600.0, 600.0,
                            /*capital_count=*/2, /*city_count=*/3, /*town_count=*/3,
                            /*min_spacing_m=*/10.0);

    constexpr double cell_w = 600.0 / W;
    for (const auto& s : net.settlements) {
        const int gx = static_cast<int>(s.x / cell_w);
        EXPECT_GT(gx, 0) << "settlement landed in the ocean column";
    }
}

// --- M152: city placement avoids ocean / extreme elevation ---

// PlaceExcludesTheOnlyLandCellWhenTooHighAboveSeaLevel (above) only proves
// the pathological "every land cell is too high" case ends up empty; it
// doesn't prove placement specifically avoids extreme-elevation cells when
// they're mixed among otherwise-valid land -- the more realistic scenario.
// This closes that gap, mirroring PlaceNeverPicksAnOceanCell's own style
// exactly, just for elevation instead of ocean.
TEST(SettlementsTest, PlaceNeverPicksACellAboveMaxSettlementElevation) {
    constexpr int W = 6, H = 6;
    FieldGrid elevation;
    elevation.w = W;
    elevation.h = H;
    elevation.data.assign(static_cast<std::size_t>(W * H), 50.0f);
    // Column 0 is a 5000 m "mountain" strip -- well above the 1200 m
    // default max_settlement_elevation_m -- amid otherwise ordinary land.
    for (int y = 0; y < H; ++y) elevation.data[static_cast<std::size_t>(y * W)] = 5000.0f;

    const SettlementNetwork net =
        Settlements::place(7, elevation, 0.0, 0.0, 0.0, 600.0, 600.0,
                            /*capital_count=*/2, /*city_count=*/5, /*town_count=*/5,
                            /*min_spacing_m=*/10.0);

    constexpr double cell_w = 600.0 / W;
    for (const auto& s : net.settlements) {
        const int gx = static_cast<int>(s.x / cell_w);
        EXPECT_GT(gx, 0) << "settlement landed in the extreme-elevation column";
    }
}

TEST(SettlementsTest, CoastalFlatSiteIsPreferredOverPlainInlandLand) {
    // 5x5, column 0 is ocean. Every cell in column 1 is coastal AND flat
    // (score 3, tied among themselves); columns 3-4 are plain inland flat
    // land, out of coastal range (score 2). Requesting a single capital
    // must land somewhere in column 1, never columns 3-4, regardless of
    // which row the tie-break among column 1's cells picks.
    constexpr int W = 5, H = 5;
    FieldGrid elevation;
    elevation.w = W;
    elevation.h = H;
    elevation.data.assign(static_cast<std::size_t>(W * H), 50.0f);
    for (int y = 0; y < H; ++y) elevation.data[static_cast<std::size_t>(y * W)] = -10.0f;

    const SettlementNetwork net =
        Settlements::place(42, elevation, 0.0, 0.0, 0.0, 500.0, 500.0,
                            /*capital_count=*/1, /*city_count=*/0, /*town_count=*/0,
                            /*min_spacing_m=*/10.0);

    ASSERT_EQ(net.settlements.size(), 1u);
    constexpr double cell = 500.0 / W;
    EXPECT_NEAR(net.settlements[0].x, (1 + 0.5) * cell, 1e-6) << "should pick a column-1 (coastal) cell";
    EXPECT_EQ(net.settlements[0].tier, SettlementTier::Capital);
}

TEST(SettlementsTest, PlaceRespectsMinimumSpacingBetweenSettlements) {
    constexpr int W = 8, H = 8;
    FieldGrid elevation;
    elevation.w = W;
    elevation.h = H;
    elevation.data.assign(static_cast<std::size_t>(W * H), 50.0f);  // uniform flat land, no ocean

    constexpr double world_extent = 800.0;  // 100 m cells
    const SettlementNetwork net =
        Settlements::place(42, elevation, /*sea_level_m=*/-100.0, 0.0, 0.0, world_extent,
                            world_extent, /*capital_count=*/4, /*city_count=*/0,
                            /*town_count=*/0, /*min_spacing_m=*/250.0);

    ASSERT_GE(net.settlements.size(), 2u)
        << "fixture should allow at least 2 non-conflicting sites -- test would be vacuous otherwise";
    for (std::size_t i = 0; i < net.settlements.size(); ++i)
        for (std::size_t j = i + 1; j < net.settlements.size(); ++j)
            EXPECT_GE(distance(net.settlements[i], net.settlements[j]), 250.0)
                << "settlements " << i << " and " << j << " are too close";
}

TEST(SettlementsTest, PlaceStopsWhenNoNonConflictingSitesRemain) {
    // Only 2 candidate cells exist in the whole grid, spaced closer together
    // than min_spacing_m -- requesting 2 capitals must still only place 1.
    FieldGrid elevation;
    elevation.w = elevation.h = 3;
    elevation.data = {-10, -10, -10,
                        50,  -10,  50,
                       -10, -10, -10};
    const SettlementNetwork net =
        Settlements::place(42, elevation, 0.0, 0.0, 0.0, 300.0, 300.0,
                            /*capital_count=*/2, /*city_count=*/0, /*town_count=*/0,
                            /*min_spacing_m=*/1000.0);
    EXPECT_EQ(net.settlements.size(), 1u);
}

TEST(SettlementsTest, PlaceProducesRequestedTierCountsWhenRoomAllows) {
    constexpr int W = 10, H = 10;
    FieldGrid elevation;
    elevation.w = W;
    elevation.h = H;
    elevation.data.assign(static_cast<std::size_t>(W * H), 50.0f);

    const SettlementNetwork net =
        Settlements::place(42, elevation, /*sea_level_m=*/-100.0, 0.0, 0.0, 1000.0, 1000.0,
                            /*capital_count=*/1, /*city_count=*/2, /*town_count=*/3,
                            /*min_spacing_m=*/10.0);

    EXPECT_EQ(net.settlements.size(), 6u);
    EXPECT_EQ(count_tier(net, SettlementTier::Capital), 1);
    EXPECT_EQ(count_tier(net, SettlementTier::City), 2);
    EXPECT_EQ(count_tier(net, SettlementTier::Town), 3);
}

TEST(SettlementsTest, PlacedSettlementsHaveNoNameYet) {
    FieldGrid elevation;
    elevation.w = elevation.h = 4;
    elevation.data.assign(16, 50.0f);
    const SettlementNetwork net = Settlements::place(
        42, elevation, -100.0, 0.0, 0.0, 400.0, 400.0, 1, 0, 0, 10.0);
    ASSERT_EQ(net.settlements.size(), 1u);
    EXPECT_TRUE(net.settlements[0].name.empty());
}

TEST(SettlementsTest, PlaceIsDeterministic) {
    FieldGrid elevation;
    elevation.w = elevation.h = 8;
    elevation.data.assign(64, 50.0f);
    for (int y = 0; y < 8; ++y) elevation.data[static_cast<std::size_t>(y * 8)] = -10.0f;

    const SettlementNetwork a =
        Settlements::place(123, elevation, 0.0, 0.0, 0.0, 800.0, 800.0, 2, 2, 2, 50.0);
    const SettlementNetwork b =
        Settlements::place(123, elevation, 0.0, 0.0, 0.0, 800.0, 800.0, 2, 2, 2, 50.0);

    ASSERT_EQ(a.settlements.size(), b.settlements.size());
    for (std::size_t i = 0; i < a.settlements.size(); ++i) {
        EXPECT_DOUBLE_EQ(a.settlements[i].x, b.settlements[i].x);
        EXPECT_DOUBLE_EQ(a.settlements[i].z, b.settlements[i].z);
        EXPECT_EQ(a.settlements[i].tier, b.settlements[i].tier);
    }
}

// --- M148: Settlements::name() ---

namespace {

SettlementNetwork make_n_settlements(int n, SettlementTier tier = SettlementTier::Village) {
    SettlementNetwork net;
    for (int i = 0; i < n; ++i) net.settlements.push_back(Settlement{0.0, 0.0, tier, ""});
    return net;
}

} // namespace

TEST(SettlementsTest, NameOverEmptyNetworkDoesNothing) {
    SettlementNetwork net;
    Settlements::name(net, "nordic", 42);
    EXPECT_TRUE(net.empty());
}

TEST(SettlementsTest, NameAssignsNonEmptyNames) {
    SettlementNetwork net = make_n_settlements(3);
    Settlements::name(net, "nordic", 42);

    for (const auto& s : net.settlements) EXPECT_FALSE(s.name.empty());
}

TEST(SettlementsTest, NameUsesEveryTierNotJustCity) {
    // Naming::city() is the only settlement namer this codebase has (same
    // choice region.lua's own town placement already makes) -- confirm it
    // is actually applied regardless of tier, not just to City-tier records.
    SettlementNetwork net;
    net.settlements.push_back(Settlement{0.0, 0.0, SettlementTier::Capital, ""});
    net.settlements.push_back(Settlement{0.0, 0.0, SettlementTier::Village, ""});
    Settlements::name(net, "nordic", 42);

    EXPECT_FALSE(net.settlements[0].name.empty());
    EXPECT_FALSE(net.settlements[1].name.empty());
}

TEST(SettlementsTest, NameIsDeterministic) {
    SettlementNetwork a = make_n_settlements(4);
    SettlementNetwork b = make_n_settlements(4);
    Settlements::name(a, "nordic", 123);
    Settlements::name(b, "nordic", 123);

    for (std::size_t i = 0; i < a.settlements.size(); ++i)
        EXPECT_EQ(a.settlements[i].name, b.settlements[i].name);
}

TEST(SettlementsTest, DifferentSettlementsGetDifferentSeedsAndTypicallyDifferentNames) {
    SettlementNetwork net = make_n_settlements(5);
    Settlements::name(net, "nordic", 42);

    std::set<std::string> names;
    for (const auto& s : net.settlements) names.insert(s.name);
    EXPECT_GT(names.size(), 1u) << "5 settlements all getting the exact same name would indicate "
                                    "every settlement is using the same seed";
}

TEST(SettlementsTest, DifferentEntropyProducesDifferentNamesForTheSameSettlementIndex) {
    SettlementNetwork a = make_n_settlements(1);
    SettlementNetwork b = make_n_settlements(1);
    Settlements::name(a, "nordic", 1);
    Settlements::name(b, "nordic", 2);

    EXPECT_NE(a.settlements[0].name, b.settlements[0].name);
}

// M340 (MAP22) -- Settlements::name() now wires NameGenerator::dedupe() in,
// so no two settlements within the same network can share a generated
// name. nordic's own city phoneme pool (2 syllables x 12 consonants x 6
// vowels, x 3 suffixes) has on the order of ~15k distinct outputs -- with
// 900 draws the birthday paradox makes at least one raw collision all but
// certain (empirically confirmed: reverting the dedupe() wiring reproduces
// a real collision at this volume), so "every name in this network is
// unique" is a reliable, non-flaky proof that dedupe() is actually wired
// in here, not just unit-tested standalone (M082) without ever being
// called from a real pipeline (the exact gap this task closes).
TEST(SettlementsTest, NameProducesNoDuplicatesEvenAtHighVolume) {
    SettlementNetwork net = make_n_settlements(900);
    Settlements::name(net, "nordic", 42);

    std::set<std::string> names;
    for (const auto& s : net.settlements) names.insert(s.name);
    EXPECT_EQ(names.size(), net.settlements.size())
        << "expected all 900 settlement names to be unique";
}

TEST(SettlementsTest, NameDoesNotTouchPositionOrTier) {
    SettlementNetwork net;
    net.settlements.push_back(Settlement{123.0, 456.0, SettlementTier::Town, ""});

    Settlements::name(net, "nordic", 42);

    EXPECT_DOUBLE_EQ(net.settlements[0].x, 123.0);
    EXPECT_DOUBLE_EQ(net.settlements[0].z, 456.0);
    EXPECT_EQ(net.settlements[0].tier, SettlementTier::Town);
}

// --- M148: Settlements::appendLabels() ---

TEST(SettlementsTest, AppendLabelsOverEmptyNetworkAppendsNothing) {
    std::vector<PlaceLabel> labels;
    Settlements::appendLabels(labels, SettlementNetwork{});
    EXPECT_TRUE(labels.empty());
}

TEST(SettlementsTest, AppendLabelsCopiesNamePositionAndTierAsKind) {
    SettlementNetwork net;
    net.settlements.push_back(Settlement{100.0, 200.0, SettlementTier::Capital, "Vorhavn"});

    std::vector<PlaceLabel> labels;
    Settlements::appendLabels(labels, net);

    ASSERT_EQ(labels.size(), 1u);
    EXPECT_EQ(labels[0].name, "Vorhavn");
    EXPECT_DOUBLE_EQ(labels[0].pos[0], 100.0);
    EXPECT_DOUBLE_EQ(labels[0].pos[1], 200.0);
    EXPECT_EQ(labels[0].kind, "capital");
}

TEST(SettlementsTest, AppendLabelsMapsEachTierToItsOwnLowercaseKind) {
    SettlementNetwork net;
    net.settlements.push_back(Settlement{0.0, 0.0, SettlementTier::Capital, "a"});
    net.settlements.push_back(Settlement{0.0, 0.0, SettlementTier::City, "b"});
    net.settlements.push_back(Settlement{0.0, 0.0, SettlementTier::Town, "c"});
    net.settlements.push_back(Settlement{0.0, 0.0, SettlementTier::Village, "d"});

    std::vector<PlaceLabel> labels;
    Settlements::appendLabels(labels, net);

    ASSERT_EQ(labels.size(), 4u);
    EXPECT_EQ(labels[0].kind, "capital");
    EXPECT_EQ(labels[1].kind, "city");
    EXPECT_EQ(labels[2].kind, "town");
    EXPECT_EQ(labels[3].kind, "village");
}

TEST(SettlementsTest, AppendLabelsWithAnUnnamedSettlementProducesAnEmptyNameLabel) {
    SettlementNetwork net;
    net.settlements.push_back(Settlement{0.0, 0.0, SettlementTier::Town, ""});

    std::vector<PlaceLabel> labels;
    Settlements::appendLabels(labels, net);

    ASSERT_EQ(labels.size(), 1u);
    EXPECT_TRUE(labels[0].name.empty());
}

TEST(SettlementsTest, AppendLabelsIsAdditiveAndPreservesExistingLabels) {
    std::vector<PlaceLabel> labels;
    labels.push_back(PlaceLabel{"Aeland", {{0.0, 0.0}}, "country"});

    SettlementNetwork net;
    net.settlements.push_back(Settlement{1.0, 2.0, SettlementTier::City, "Aldburg"});
    Settlements::appendLabels(labels, net);

    ASSERT_EQ(labels.size(), 2u);
    EXPECT_EQ(labels[0].kind, "country");
    EXPECT_EQ(labels[1].kind, "city");
    EXPECT_EQ(labels[1].name, "Aldburg");
}
