// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP9 tests. M139: Countries::grow() -- region growth from capitals.
// M140: borders prefer natural features (rivers, ridges, coasts).
// M141: Countries::name() -- culture + name per country.
// M149: every country has exactly one capital (integration, over a real
// Settlements::place() -> Countries::grow() pipeline).
// M151: borders follow a ridge/river, sampled across multiple rows.

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <set>
#include <vector>

#include "Map/Countries.hpp"

using namespace MeshWorld::Map;

namespace {

Settlement capital_at(double x, double z) {
    Settlement s;
    s.x    = x;
    s.z    = z;
    s.tier = SettlementTier::Capital;
    return s;
}

} // namespace

TEST(CountriesTest, DefaultNetworkIsEmpty) {
    CountryNetwork net;
    EXPECT_TRUE(net.empty());
}

TEST(CountriesTest, GrowOverEmptyElevationReturnsEmptyNetwork) {
    FieldGrid        elevation;  // default-constructed: empty
    SettlementNetwork settlements;
    settlements.settlements.push_back(capital_at(50.0, 50.0));

    const CountryNetwork net = Countries::grow(settlements, elevation, 0.0, 0.0, 0.0, 100.0, 100.0);
    EXPECT_TRUE(net.empty());
}

TEST(CountriesTest, GrowWithNoCapitalsReturnsEmptyNetwork) {
    FieldGrid elevation;
    elevation.w = elevation.h = 4;
    elevation.data.assign(16, 50.0f);

    SettlementNetwork settlements;
    Settlement        town;
    town.tier = SettlementTier::Town;
    settlements.settlements.push_back(town);

    const CountryNetwork net = Countries::grow(settlements, elevation, 0.0, 0.0, 0.0, 400.0, 400.0);
    EXPECT_TRUE(net.empty());
}

TEST(CountriesTest, GrowWithOneCapitalClaimsAllLandNoOcean) {
    constexpr int W = 5, H = 5;
    FieldGrid elevation;
    elevation.w = W;
    elevation.h = H;
    elevation.data.assign(static_cast<std::size_t>(W * H), 50.0f);  // uniform flat land, no ocean

    SettlementNetwork settlements;
    settlements.settlements.push_back(capital_at(250.0, 250.0));  // roughly the grid center

    const CountryNetwork net = Countries::grow(settlements, elevation, /*sea_level_m=*/-100.0,
                                                0.0, 0.0, 500.0, 500.0);

    ASSERT_EQ(net.countries.size(), 1u);
    EXPECT_EQ(net.countries[0].territory.size(), static_cast<std::size_t>(W * H));
}

// One capital claims the entire W x H grid (no ocean): the traced border
// must be the grid's own outer rectangle -- a single closed loop of
// (W+1)*2 + (H+1)*2 - 4 grid-corner points (the perimeter of a (W+1)x(H+1)
// corner lattice), all within [0, 500) x [0, 500) (half-open, matching
// MapValidator::check_features_in_bounds()'s own convention -- see the
// kBorderEdgeEpsM nudge in trace_owner_border()).
TEST(CountriesTest, BorderTracesTheOuterRectangleWhenOneCapitalOwnsEverything) {
    constexpr int W = 5, H = 5;
    FieldGrid elevation;
    elevation.w = W;
    elevation.h = H;
    elevation.data.assign(static_cast<std::size_t>(W * H), 50.0f);

    SettlementNetwork settlements;
    settlements.settlements.push_back(capital_at(250.0, 250.0));

    const CountryNetwork net = Countries::grow(settlements, elevation, -100.0, 0.0, 0.0, 500.0, 500.0);

    ASSERT_EQ(net.countries.size(), 1u);
    const auto& border = net.countries[0].border;
    ASSERT_GE(border.size(), 4u);
    EXPECT_EQ(border.front()[0], border.back()[0]);
    EXPECT_EQ(border.front()[1], border.back()[1]);

    for (const auto& p : border) {
        EXPECT_GE(p[0], 0.0);
        EXPECT_LT(p[0], 500.0);
        EXPECT_GE(p[1], 0.0);
        EXPECT_LT(p[1], 500.0);
    }

    // Perimeter length in grid-edge units: 2*W + 2*H (a (W+1)x(H+1) corner
    // lattice's outer rectangle), plus the repeated closing point.
    EXPECT_EQ(border.size(), static_cast<std::size_t>(2 * W + 2 * H + 1));
}

TEST(CountriesTest, BorderIsDeterministic) {
    constexpr int W = 6, H = 6;
    FieldGrid elevation;
    elevation.w = W;
    elevation.h = H;
    elevation.data.assign(static_cast<std::size_t>(W * H), 50.0f);
    for (int y = 0; y < H; ++y) elevation.data[static_cast<std::size_t>(y * W)] = -10.0f;  // column 0 ocean

    SettlementNetwork settlements;
    settlements.settlements.push_back(capital_at(350.0, 300.0));

    const CountryNetwork a = Countries::grow(settlements, elevation, 0.0, 0.0, 0.0, 600.0, 600.0);
    const CountryNetwork b = Countries::grow(settlements, elevation, 0.0, 0.0, 0.0, 600.0, 600.0);

    ASSERT_EQ(a.countries[0].border.size(), b.countries[0].border.size());
    for (std::size_t i = 0; i < a.countries[0].border.size(); ++i) {
        EXPECT_DOUBLE_EQ(a.countries[0].border[i][0], b.countries[0].border[i][0]);
        EXPECT_DOUBLE_EQ(a.countries[0].border[i][1], b.countries[0].border[i][1]);
    }
}

TEST(CountriesTest, TwoCapitalsProduceTwoDistinctNonEmptyBorders) {
    constexpr int W = 10, H = 4;
    FieldGrid elevation;
    elevation.w = W;
    elevation.h = H;
    elevation.data.assign(static_cast<std::size_t>(W * H), 50.0f);

    SettlementNetwork settlements;
    settlements.settlements.push_back(capital_at(150.0, 150.0));  // cell (1,1)
    settlements.settlements.push_back(capital_at(850.0, 150.0));  // cell (8,1)

    const CountryNetwork net = Countries::grow(settlements, elevation, -100.0, 0.0, 0.0, 1000.0, 400.0);

    ASSERT_EQ(net.countries.size(), 2u);
    EXPECT_GE(net.countries[0].border.size(), 4u);
    EXPECT_GE(net.countries[1].border.size(), 4u);
    EXPECT_NE(net.countries[0].border, net.countries[1].border);

    for (const auto& country : net.countries) {
        for (const auto& p : country.border) {
            EXPECT_GE(p[0], 0.0);
            EXPECT_LT(p[0], 1000.0);
            EXPECT_GE(p[1], 0.0);
            EXPECT_LT(p[1], 400.0);
        }
    }
}

TEST(CountriesTest, GrowNeverClaimsOceanCells) {
    constexpr int W = 6, H = 6;
    FieldGrid elevation;
    elevation.w = W;
    elevation.h = H;
    elevation.data.assign(static_cast<std::size_t>(W * H), 50.0f);
    for (int y = 0; y < H; ++y) elevation.data[static_cast<std::size_t>(y * W)] = -10.0f;  // column 0 is ocean

    SettlementNetwork settlements;
    settlements.settlements.push_back(capital_at(350.0, 300.0));  // column ~3-4, well inland

    const CountryNetwork net =
        Countries::grow(settlements, elevation, 0.0, 0.0, 0.0, 600.0, 600.0);

    ASSERT_EQ(net.countries.size(), 1u);
    constexpr double cell_w = 600.0 / W;
    for (const auto& p : net.countries[0].territory) {
        const int gx = static_cast<int>(p[0] / cell_w);
        EXPECT_GT(gx, 0) << "ocean column claimed as territory";
    }
    // Every non-ocean cell should have been claimed: (W-1) columns * H rows.
    EXPECT_EQ(net.countries[0].territory.size(), static_cast<std::size_t>((W - 1) * H));
}

TEST(CountriesTest, GrowSplitsLandBetweenTwoCapitalsAndEachOwnsItsOwnCell) {
    constexpr int W = 10, H = 4;
    FieldGrid elevation;
    elevation.w = W;
    elevation.h = H;
    elevation.data.assign(static_cast<std::size_t>(W * H), 50.0f);  // uniform flat land, no ocean

    // Cell centers are at (gx+0.5)*100 / (gy+0.5)*100 -- place capitals
    // exactly on cell (1,1) and (8,1) centers so there's no ambiguity about
    // which cell they snap to.
    constexpr double world_extent_x = 1000.0, world_extent_z = 400.0;  // 100 m cells
    SettlementNetwork settlements;
    settlements.settlements.push_back(capital_at(150.0, 150.0));  // cell (1,1)
    settlements.settlements.push_back(capital_at(850.0, 150.0));  // cell (8,1)

    const CountryNetwork net = Countries::grow(settlements, elevation, /*sea_level_m=*/-100.0,
                                                0.0, 0.0, world_extent_x, world_extent_z);

    ASSERT_EQ(net.countries.size(), 2u);
    EXPECT_FALSE(net.countries[0].territory.empty());
    EXPECT_FALSE(net.countries[1].territory.empty());

    // Every land cell (all of them here) must belong to exactly one country.
    std::size_t total = net.countries[0].territory.size() + net.countries[1].territory.size();
    EXPECT_EQ(total, static_cast<std::size_t>(W * H));

    // Each capital's own cell belongs to its own country.
    bool country0_has_own_capital_cell = false;
    for (const auto& p : net.countries[0].territory)
        if (std::abs(p[0] - 150.0) < 1e-6 && std::abs(p[1] - 150.0) < 1e-6)
            country0_has_own_capital_cell = true;
    EXPECT_TRUE(country0_has_own_capital_cell);

    bool country1_has_own_capital_cell = false;
    for (const auto& p : net.countries[1].territory)
        if (std::abs(p[0] - 850.0) < 1e-6 && std::abs(p[1] - 150.0) < 1e-6)
            country1_has_own_capital_cell = true;
    EXPECT_TRUE(country1_has_own_capital_cell);
}

TEST(CountriesTest, GrowDoesNotCrossAnOceanGapBetweenIslands) {
    // 7 columns: 0-1 land (island A, capital A), 2-4 ocean, 5-6 land (island
    // B, capital B). No land path connects the two islands.
    constexpr int W = 7, H = 3;
    FieldGrid elevation;
    elevation.w = W;
    elevation.h = H;
    elevation.data.assign(static_cast<std::size_t>(W * H), -10.0f);
    for (int y = 0; y < H; ++y) {
        elevation.data[static_cast<std::size_t>(y * W + 0)] = 50.0f;
        elevation.data[static_cast<std::size_t>(y * W + 1)] = 50.0f;
        elevation.data[static_cast<std::size_t>(y * W + 5)] = 50.0f;
        elevation.data[static_cast<std::size_t>(y * W + 6)] = 50.0f;
    }

    constexpr double world_extent_x = 700.0, world_extent_z = 300.0;  // 100 m cells
    SettlementNetwork settlements;
    settlements.settlements.push_back(capital_at(50.0, 150.0));   // island A, column 0
    settlements.settlements.push_back(capital_at(650.0, 150.0));  // island B, column 6

    const CountryNetwork net = Countries::grow(settlements, elevation, /*sea_level_m=*/0.0,
                                                0.0, 0.0, world_extent_x, world_extent_z);

    ASSERT_EQ(net.countries.size(), 2u);
    // Each island has 2 columns x 3 rows = 6 land cells.
    EXPECT_EQ(net.countries[0].territory.size(), 6u);
    EXPECT_EQ(net.countries[1].territory.size(), 6u);

    constexpr double cell_w = world_extent_x / W;
    for (const auto& p : net.countries[0].territory) {
        const int gx = static_cast<int>(p[0] / cell_w);
        EXPECT_LE(gx, 1) << "country 0 (island A) claimed a cell past its own island";
    }
    for (const auto& p : net.countries[1].territory) {
        const int gx = static_cast<int>(p[0] / cell_w);
        EXPECT_GE(gx, 5) << "country 1 (island B) claimed a cell past its own island";
    }
}

TEST(CountriesTest, GrowTerritoryHasNoDuplicateCells) {
    constexpr int W = 8, H = 8;
    FieldGrid elevation;
    elevation.w = W;
    elevation.h = H;
    elevation.data.assign(static_cast<std::size_t>(W * H), 50.0f);

    SettlementNetwork settlements;
    settlements.settlements.push_back(capital_at(150.0, 150.0));
    settlements.settlements.push_back(capital_at(650.0, 650.0));

    const CountryNetwork net = Countries::grow(settlements, elevation, -100.0, 0.0, 0.0, 800.0, 800.0);

    std::set<std::pair<double, double>> seen;
    for (const auto& country : net.countries)
        for (const auto& p : country.territory) {
            const auto key = std::make_pair(p[0], p[1]);
            EXPECT_TRUE(seen.insert(key).second) << "cell (" << p[0] << "," << p[1]
                                                  << ") claimed by more than one country";
        }
}

TEST(CountriesTest, GrowIsDeterministic) {
    constexpr int W = 8, H = 8;
    FieldGrid elevation;
    elevation.w = W;
    elevation.h = H;
    elevation.data.assign(static_cast<std::size_t>(W * H), 50.0f);
    for (int y = 0; y < H; ++y) elevation.data[static_cast<std::size_t>(y * W)] = -10.0f;

    SettlementNetwork settlements;
    settlements.settlements.push_back(capital_at(250.0, 250.0));
    settlements.settlements.push_back(capital_at(650.0, 650.0));

    const CountryNetwork a = Countries::grow(settlements, elevation, 0.0, 0.0, 0.0, 800.0, 800.0);
    const CountryNetwork b = Countries::grow(settlements, elevation, 0.0, 0.0, 0.0, 800.0, 800.0);

    ASSERT_EQ(a.countries.size(), b.countries.size());
    for (std::size_t i = 0; i < a.countries.size(); ++i) {
        ASSERT_EQ(a.countries[i].territory.size(), b.countries[i].territory.size());
        for (std::size_t j = 0; j < a.countries[i].territory.size(); ++j) {
            EXPECT_DOUBLE_EQ(a.countries[i].territory[j][0], b.countries[i].territory[j][0]);
            EXPECT_DOUBLE_EQ(a.countries[i].territory[j][1], b.countries[i].territory[j][1]);
        }
    }
}

TEST(CountriesTest, CountryStartsWithNoNameOrCulture) {
    FieldGrid elevation;
    elevation.w = elevation.h = 3;
    elevation.data.assign(9, 50.0f);

    SettlementNetwork settlements;
    settlements.settlements.push_back(capital_at(150.0, 150.0));

    const CountryNetwork net = Countries::grow(settlements, elevation, -100.0, 0.0, 0.0, 300.0, 300.0);
    ASSERT_EQ(net.countries.size(), 1u);
    EXPECT_TRUE(net.countries[0].name.empty());
    EXPECT_TRUE(net.countries[0].culture.empty());
}

// --- M140: borders prefer natural features ---

namespace {

// A 9x1 corridor (only cardinal movement is possible -- no row above/below
// to move diagonally into), uniform flat land, no ocean. West capital at
// column 3, East capital at column 8. Column 4 is the single "wall" cell.
//
// Baseline (no wall considered): cost(west->col c) = |c-3|, cost(east->col
// c) = |c-8|. At column 5: west costs 2, east costs 3 -- west wins.
//
// With column 4 marked as a natural feature and a high multiplier: west's
// path to column 5 must enter column 4 (cost 1*multiplier) then column 5
// (cost 1) = multiplier+1. East's path to column 5 (8->7->6->5) never
// touches column 4: cost 3. For multiplier >= 3, east now wins column 5
// instead -- while west still wins column 4 itself (west's cost to reach
// it, 1*multiplier, is cheaper than east's, 3 normal steps + multiplier
// entering it = 3+multiplier), since west approaches the wall directly
// and only pays the toll once.
FieldGrid make_corridor(int width) {
    FieldGrid g;
    g.w = width;
    g.h = 1;
    g.data.assign(static_cast<std::size_t>(width), 50.0f);
    return g;
}

SettlementNetwork make_west_east_capitals(double world_extent) {
    SettlementNetwork s;
    const double cell = world_extent / 9.0;
    s.settlements.push_back(capital_at((3 + 0.5) * cell, 0.5 * cell));  // column 3
    s.settlements.push_back(capital_at((8 + 0.5) * cell, 0.5 * cell));  // column 8
    return s;
}

int column_owner(const CountryNetwork& net, double world_extent, int column) {
    const double cell   = world_extent / 9.0;
    const double target = (column + 0.5) * cell;
    for (std::size_t i = 0; i < net.countries.size(); ++i)
        for (const auto& p : net.countries[i].territory)
            if (std::abs(p[0] - target) < 1e-6) return static_cast<int>(i);
    return -1;  // not found
}

} // namespace

TEST(CountriesTest, BaselineWithoutWallGivesColumn5ToWest) {
    constexpr double world_extent = 900.0;  // 9x1, 100 m cells
    const FieldGrid  elevation    = make_corridor(9);
    const SettlementNetwork settlements = make_west_east_capitals(world_extent);

    const CountryNetwork net =
        Countries::grow(settlements, elevation, -100.0, 0.0, 0.0, world_extent, 100.0);

    EXPECT_EQ(column_owner(net, world_extent, 5), 0) << "west (country 0) should win column 5 without a wall";
}

TEST(CountriesTest, RidgeWallShiftsColumn5ToEastButWestKeepsTheWallItself) {
    constexpr double world_extent = 900.0;
    const FieldGrid  elevation    = make_corridor(9);
    const SettlementNetwork settlements = make_west_east_capitals(world_extent);

    MountainRangeNetwork mountains;
    MountainRange        wall;
    wall.ridge.push_back(RidgePoint{450.0, 50.0, 3000.0});  // column 4 center
    mountains.ranges.push_back(wall);

    const CountryNetwork net =
        Countries::grow(settlements, elevation, -100.0, 0.0, 0.0, world_extent, 100.0,
                         /*hydrology=*/nullptr, &mountains,
                         /*natural_feature_cost_multiplier=*/20.0);

    EXPECT_EQ(column_owner(net, world_extent, 4), 0) << "west still claims the wall cell itself";
    EXPECT_EQ(column_owner(net, world_extent, 5), 1) << "east now claims the cell just past the wall";
}

TEST(CountriesTest, RiverAlsoShiftsTheBorderTheSameWayARidgeDoes) {
    constexpr double world_extent = 900.0;
    const FieldGrid  elevation    = make_corridor(9);
    const SettlementNetwork settlements = make_west_east_capitals(world_extent);

    HydrologyNetwork hydro;
    RiverSegment     river;
    river.points.push_back(RiverPoint{450.0, 50.0, 1.0f});  // column 4 center
    hydro.rivers.push_back(river);

    const CountryNetwork net =
        Countries::grow(settlements, elevation, -100.0, 0.0, 0.0, world_extent, 100.0,
                         &hydro, /*mountains=*/nullptr,
                         /*natural_feature_cost_multiplier=*/20.0);

    EXPECT_EQ(column_owner(net, world_extent, 5), 1) << "a river creates the same border-shifting effect as a ridge";
}

TEST(CountriesTest, MultiplierOfOneMatchesTheNoWallBaseline) {
    // The wall cell is marked, but a multiplier of 1.0 means it costs
    // exactly as much as any other cell -- confirms the *value* of the
    // multiplier drives the effect, not merely passing a non-null network.
    constexpr double world_extent = 900.0;
    const FieldGrid  elevation    = make_corridor(9);
    const SettlementNetwork settlements = make_west_east_capitals(world_extent);

    MountainRangeNetwork mountains;
    MountainRange        wall;
    wall.ridge.push_back(RidgePoint{450.0, 50.0, 3000.0});
    mountains.ranges.push_back(wall);

    const CountryNetwork net =
        Countries::grow(settlements, elevation, -100.0, 0.0, 0.0, world_extent, 100.0,
                         nullptr, &mountains, /*natural_feature_cost_multiplier=*/1.0);

    EXPECT_EQ(column_owner(net, world_extent, 5), 0) << "west should still win column 5 with no real penalty";
}

TEST(CountriesTest, ZeroCoastalRadiusDisablesTheCoastalPenalty) {
    // Column 0 is ocean; with coastal_radius_cells=0, no cell is treated as
    // coastline, so this should behave identically to the plain baseline
    // (west, at column 3, still wins column 5 over east at column 8 -- the
    // ocean at column 0 is irrelevant to this comparison either way).
    constexpr double world_extent = 900.0;
    FieldGrid         elevation   = make_corridor(9);
    elevation.data[0]             = -10.0f;  // column 0 is ocean
    const SettlementNetwork settlements = make_west_east_capitals(world_extent);

    const CountryNetwork net =
        Countries::grow(settlements, elevation, 0.0, 0.0, 0.0, world_extent, 100.0,
                         nullptr, nullptr, /*natural_feature_cost_multiplier=*/20.0,
                         /*coastal_radius_cells=*/0);

    EXPECT_EQ(column_owner(net, world_extent, 5), 0);
}

// --- M141: Countries::name() ---

namespace {

CountryNetwork make_n_countries(int n) {
    CountryNetwork net;
    net.countries.resize(static_cast<std::size_t>(n));
    return net;
}

} // namespace

TEST(CountriesTest, NameOverEmptyNetworkDoesNothing) {
    CountryNetwork net;
    Countries::name(net, 42);
    EXPECT_TRUE(net.empty());
}

TEST(CountriesTest, NameAssignsNonEmptyCultureAndName) {
    CountryNetwork net = make_n_countries(3);
    Countries::name(net, 42);

    for (const auto& country : net.countries) {
        EXPECT_FALSE(country.culture.empty());
        EXPECT_FALSE(country.name.empty());
    }
}

TEST(CountriesTest, NameCultureIsAKnownCultureId) {
    CountryNetwork net = make_n_countries(10);
    Countries::name(net, 7);

    for (const auto& country : net.countries)
        EXPECT_TRUE(country.culture == "nordic" || country.culture == "romance"
                    || country.culture == "desert")
            << country.culture;
}

TEST(CountriesTest, NameIsDeterministic) {
    CountryNetwork a = make_n_countries(4);
    CountryNetwork b = make_n_countries(4);
    Countries::name(a, 123);
    Countries::name(b, 123);

    for (std::size_t i = 0; i < a.countries.size(); ++i) {
        EXPECT_EQ(a.countries[i].culture, b.countries[i].culture);
        EXPECT_EQ(a.countries[i].name, b.countries[i].name);
    }
}

TEST(CountriesTest, DifferentCountriesGetDifferentSeedsAndTypicallyDifferentNames) {
    CountryNetwork net = make_n_countries(5);
    Countries::name(net, 42);

    std::set<std::string> names;
    for (const auto& country : net.countries) names.insert(country.name);
    EXPECT_GT(names.size(), 1u) << "5 countries all getting the exact same name would indicate "
                                    "every country is using the same seed";
}

TEST(CountriesTest, DifferentEntropyProducesDifferentNamesForTheSameCountryIndex) {
    CountryNetwork a = make_n_countries(1);
    CountryNetwork b = make_n_countries(1);
    Countries::name(a, 1);
    Countries::name(b, 2);

    EXPECT_NE(a.countries[0].name, b.countries[0].name);
}

// M340 (MAP22) -- Countries::name() now wires NameGenerator::dedupe() in,
// so no two countries within the same network can share a generated name,
// even across different cultures (dedupe() compares final assembled
// strings, not culture ids). Every culture's own country phoneme pool
// (2 syllables x its own consonants/vowels, x 3 suffixes) is on the order
// of ~2.7k (desert, the smallest) to ~15k (nordic) distinct outputs -- with
// 900 countries split across 3 cultures, the birthday paradox makes at
// least one raw collision all but certain even for the smallest pool, so
// "every name in this network is unique" reliably proves dedupe() is
// actually wired into the real pipeline here, not just unit-tested
// standalone (M082) without ever being called from it.
TEST(CountriesTest, NameProducesNoDuplicatesEvenAtHighVolume) {
    CountryNetwork net = make_n_countries(900);
    Countries::name(net, 42);

    std::set<std::string> names;
    for (const auto& country : net.countries) names.insert(country.name);
    EXPECT_EQ(names.size(), net.countries.size())
        << "expected all 900 country names to be unique";
}

// M341 (MAP22) -- Countries::name() now blends a country's generated NAME
// toward a close, differently-cultured neighbor's naming style (never its
// stored `culture` field itself -- see Countries.cpp's own doc comment on
// why this is scoped narrowly, an explicit user decision after this task
// was flagged as potentially conflicting with map.md's closed M228
// resolution). Proven here by comparing two 2-country networks built from
// the SAME entropy (so both countries' `culture` fields are identical
// between the pair -- culture is a pure function of index+entropy,
// independent of position) but different capital positions: "close" has
// both capitals at the origin (distance 0, always within the blend
// threshold), "far" places country 1 10,000 km away (always outside it).
// Swept across many entropies rather than one, since blending is
// probabilistic (blend_t never reaches 0.5) -- only entropies where the
// two countries actually rolled different cultures can show any
// difference at all (identical cultures give blend_t=0, nothing to prove).
TEST(CountriesTest, NameBlendsTowardACloseDifferentCultureNeighborAtLeastSometimes) {
    int eligible_trials = 0;
    int differing_trials = 0;
    for (std::uint64_t entropy = 0; entropy < 300; ++entropy) {
        CountryNetwork close = make_n_countries(2);
        CountryNetwork far   = make_n_countries(2);
        far.countries[1].capital_x = 10000000.0;  // 10,000 km -- past kCultureBlendMaxDistanceM

        Countries::name(close, entropy);
        Countries::name(far, entropy);

        // Culture assignment must be position-independent -- a sanity
        // check on the test's own premise, not the thing M341 changes.
        ASSERT_EQ(close.countries[0].culture, far.countries[0].culture);
        ASSERT_EQ(close.countries[1].culture, far.countries[1].culture);
        if (close.countries[0].culture == close.countries[1].culture) continue;

        ++eligible_trials;
        if (close.countries[0].name != far.countries[0].name
            || close.countries[1].name != far.countries[1].name) {
            ++differing_trials;
        }
    }
    ASSERT_GT(eligible_trials, 0) << "no entropy in [0,300) produced two different cultures -- "
                                      "widen the sweep";
    EXPECT_GT(differing_trials, 0)
        << "expected at least one close-neighbor name to differ from its far counterpart, out of "
        << eligible_trials << " culture-differing trials";
}

TEST(CountriesTest, NameDoesNotTouchTerritoryOrCapitalPosition) {
    CountryNetwork net = make_n_countries(1);
    net.countries[0].capital_x = 123.0;
    net.countries[0].capital_z = 456.0;
    net.countries[0].territory = {{1.0, 2.0}, {3.0, 4.0}};

    Countries::name(net, 42);

    EXPECT_DOUBLE_EQ(net.countries[0].capital_x, 123.0);
    EXPECT_DOUBLE_EQ(net.countries[0].capital_z, 456.0);
    ASSERT_EQ(net.countries[0].territory.size(), 2u);
    EXPECT_DOUBLE_EQ(net.countries[0].territory[0][0], 1.0);
}

// --- M149: every country has exactly one capital ---

// Realistic multi-capital scenario via a real Settlements::place() ->
// Countries::grow() pipeline (not a hand-built SettlementNetwork) --
// confirms the 1:1 capital<->country relationship holds end to end, not
// just by construction on a toy fixture.
TEST(CountriesTest, EveryCountryHasExactlyOneCapitalMatchingADistinctSourceSettlement) {
    constexpr int W = 20, H = 20;
    FieldGrid elevation;
    elevation.w = W;
    elevation.h = H;
    elevation.data.assign(static_cast<std::size_t>(W * H), 50.0f);  // uniform flat land, no ocean

    constexpr double world_extent = 2000.0;  // 100 m cells
    const SettlementNetwork settlements = Settlements::place(
        42, elevation, /*sea_level_m=*/-100.0, 0.0, 0.0, world_extent, world_extent,
        /*capital_count=*/4, /*city_count=*/0, /*town_count=*/0,
        /*min_spacing_m=*/300.0);

    std::vector<std::array<double, 2>> capital_positions;
    for (const auto& s : settlements.settlements)
        if (s.tier == SettlementTier::Capital) capital_positions.push_back({s.x, s.z});
    ASSERT_GE(capital_positions.size(), 2u)
        << "fixture should place multiple capitals -- test would be vacuous otherwise";

    const CountryNetwork net =
        Countries::grow(settlements, elevation, -100.0, 0.0, 0.0, world_extent, world_extent);

    // Exactly one country per capital -- no more, no fewer.
    ASSERT_EQ(net.countries.size(), capital_positions.size());

    // Every country's capital matches exactly one distinct input capital
    // (a real bijection, not just a matching count).
    std::set<std::size_t> matched_capitals;
    for (const auto& country : net.countries) {
        bool found = false;
        for (std::size_t i = 0; i < capital_positions.size(); ++i) {
            if (std::abs(country.capital_x - capital_positions[i][0]) < 1e-6 &&
                std::abs(country.capital_z - capital_positions[i][1]) < 1e-6) {
                EXPECT_TRUE(matched_capitals.insert(i).second)
                    << "two countries share input capital index " << i;
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "country capital (" << country.capital_x << "," << country.capital_z
                            << ") doesn't match any input capital";
    }
    EXPECT_EQ(matched_capitals.size(), capital_positions.size());
}

// --- M151: borders follow a ridge/river, sampled across multiple rows ---

namespace {

// Which country owns the cell at exact world position (x, z), or -1 if
// none does. Unlike column_owner() above (which only checks x, valid for
// the 1-row corridor fixture), this checks both coordinates -- needed once
// a fixture has more than one row.
int owner_at(const CountryNetwork& net, double x, double z) {
    for (std::size_t i = 0; i < net.countries.size(); ++i)
        for (const auto& p : net.countries[i].territory)
            if (std::abs(p[0] - x) < 1e-6 && std::abs(p[1] - z) < 1e-6) return static_cast<int>(i);
    return -1;
}

} // namespace

// Same mechanism as BaselineWithoutWallGivesColumn5ToWest/
// RidgeWallShiftsColumn5ButWestKeepsTheWallItself (M140) above, but the
// ridge wall now spans every row of a multi-row grid (not a single 1-row
// corridor), and the check samples several rows instead of relying on one
// hand-picked cell -- plan.md's M151 wording ("sampled") read as "the
// property holds across a sample of points, not just one."
TEST(CountriesTest, BorderTracksAFullHeightRidgeWallAcrossSeveralSampledRows) {
    constexpr int W = 11, H = 5;
    constexpr double cell = 100.0;
    constexpr double world_extent_x = W * cell, world_extent_z = H * cell;

    FieldGrid elevation;
    elevation.w = W;
    elevation.h = H;
    elevation.data.assign(static_cast<std::size_t>(W * H), 50.0f);  // uniform flat land, no ocean

    // Ridge wall at column 5, spanning every row (unlike M140's own
    // single-row corridor test, there is no gap to funnel through).
    MountainRangeNetwork mountains;
    MountainRange        wall;
    for (int row = 0; row < H; ++row)
        wall.ridge.push_back(RidgePoint{(5 + 0.5) * cell, (row + 0.5) * cell, 3000.0});
    mountains.ranges.push_back(wall);

    SettlementNetwork settlements;
    settlements.settlements.push_back(capital_at((1 + 0.5) * cell, (2 + 0.5) * cell));  // west
    settlements.settlements.push_back(capital_at((9 + 0.5) * cell, (2 + 0.5) * cell));  // east

    const CountryNetwork net =
        Countries::grow(settlements, elevation, /*sea_level_m=*/-100.0, 0.0, 0.0,
                         world_extent_x, world_extent_z, /*hydrology=*/nullptr, &mountains,
                         /*natural_feature_cost_multiplier=*/20.0);
    ASSERT_EQ(net.countries.size(), 2u);

    // Sample every row: column 4 (just west of the wall) must stay with the
    // west country, column 6 (just east of the wall) must go to the east
    // country -- the border sits at/near the wall in every sampled row, not
    // just the one row a capital happens to occupy.
    for (int row = 0; row < H; ++row) {
        const double z = (row + 0.5) * cell;
        EXPECT_EQ(owner_at(net, (4 + 0.5) * cell, z), 0) << "row " << row << ": west of the wall";
        EXPECT_EQ(owner_at(net, (6 + 0.5) * cell, z), 1) << "row " << row << ": east of the wall";
    }
}

// Same property, via HydrologyNetwork input instead of MountainRangeNetwork
// -- mirrors RiverAlsoShiftsTheBorderTheSameWayARidgeDoes (M140) above,
// generalized to multiple sampled rows the same way the ridge case just was.
TEST(CountriesTest, BorderTracksAFullHeightRiverAcrossSeveralSampledRows) {
    constexpr int W = 11, H = 5;
    constexpr double cell = 100.0;
    constexpr double world_extent_x = W * cell, world_extent_z = H * cell;

    FieldGrid elevation;
    elevation.w = W;
    elevation.h = H;
    elevation.data.assign(static_cast<std::size_t>(W * H), 50.0f);

    HydrologyNetwork hydro;
    RiverSegment     river;
    for (int row = 0; row < H; ++row)
        river.points.push_back(RiverPoint{(5 + 0.5) * cell, (row + 0.5) * cell, 1.0f});
    hydro.rivers.push_back(river);

    SettlementNetwork settlements;
    settlements.settlements.push_back(capital_at((1 + 0.5) * cell, (2 + 0.5) * cell));
    settlements.settlements.push_back(capital_at((9 + 0.5) * cell, (2 + 0.5) * cell));

    const CountryNetwork net =
        Countries::grow(settlements, elevation, -100.0, 0.0, 0.0, world_extent_x, world_extent_z,
                         &hydro, /*mountains=*/nullptr, /*natural_feature_cost_multiplier=*/20.0);
    ASSERT_EQ(net.countries.size(), 2u);

    for (int row = 0; row < H; ++row) {
        const double z = (row + 0.5) * cell;
        EXPECT_EQ(owner_at(net, (4 + 0.5) * cell, z), 0) << "row " << row << ": west of the river";
        EXPECT_EQ(owner_at(net, (6 + 0.5) * cell, z), 1) << "row " << row << ": east of the river";
    }
}
