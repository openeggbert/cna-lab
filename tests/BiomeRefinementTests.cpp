// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP8 tests. M128: coastal/beach band refinement.

#include <gtest/gtest.h>

#include "Map/BiomeRefinement.hpp"
#include "Map/Hydrology.hpp"
#include "Map/Volcanism.hpp"
#include "ZoneType.hpp"

using namespace MeshWorld::Map;
using MeshWorld::ZoneType;

namespace {

constexpr std::uint8_t OCEAN            = static_cast<std::uint8_t>(ZoneType::ocean);
constexpr std::uint8_t BEACH            = static_cast<std::uint8_t>(ZoneType::beach);
constexpr std::uint8_t MEADOW           = static_cast<std::uint8_t>(ZoneType::meadow);
constexpr std::uint8_t MOUNTAIN         = static_cast<std::uint8_t>(ZoneType::mountain);
constexpr std::uint8_t SWAMP            = static_cast<std::uint8_t>(ZoneType::swamp);
constexpr std::uint8_t BADLANDS         = static_cast<std::uint8_t>(ZoneType::badlands);
constexpr std::uint8_t MESA             = static_cast<std::uint8_t>(ZoneType::mesa);
constexpr std::uint8_t ROCKY_DESERT     = static_cast<std::uint8_t>(ZoneType::rocky_desert);
constexpr std::uint8_t CANYON           = static_cast<std::uint8_t>(ZoneType::canyon);
constexpr std::uint8_t TIDAL_FLAT       = static_cast<std::uint8_t>(ZoneType::tidal_flat);
constexpr std::uint8_t SEA_CLIFF        = static_cast<std::uint8_t>(ZoneType::sea_cliff);
constexpr std::uint8_t SAVANNA          = static_cast<std::uint8_t>(ZoneType::savanna);
constexpr std::uint8_t STEPPE           = static_cast<std::uint8_t>(ZoneType::steppe);
constexpr std::uint8_t PRAIRIE          = static_cast<std::uint8_t>(ZoneType::prairie);
constexpr std::uint8_t CHAPARRAL        = static_cast<std::uint8_t>(ZoneType::chaparral);
constexpr std::uint8_t SHRUBLAND        = static_cast<std::uint8_t>(ZoneType::shrubland);
constexpr std::uint8_t FOREST           = static_cast<std::uint8_t>(ZoneType::forest);
constexpr std::uint8_t RIPARIAN_FOREST  = static_cast<std::uint8_t>(ZoneType::riparian_forest);
constexpr std::uint8_t VOLCANIC         = static_cast<std::uint8_t>(ZoneType::volcanic);
constexpr std::uint8_t GEOTHERMAL       = static_cast<std::uint8_t>(ZoneType::geothermal);
constexpr std::uint8_t ASH_PLAIN        = static_cast<std::uint8_t>(ZoneType::ash_plain);
constexpr std::uint8_t VOLCANIC_ISLAND  = static_cast<std::uint8_t>(ZoneType::volcanic_island);

// A 5x5 grid: column 0 is ocean, columns 1-4 are land (meadow), flat at
// 10 m above a sea level of 0.
BiomeGrid make_coastline_biome(int w = 5, int h = 5) {
    BiomeGrid g;
    g.w = w;
    g.h = h;
    g.data.assign(static_cast<std::size_t>(w * h), MEADOW);
    for (int y = 0; y < h; ++y) g.data[static_cast<std::size_t>(y * w)] = OCEAN;
    return g;
}

FieldGrid make_flat_land_elevation(int w, int h, float land_elev, float ocean_elev) {
    FieldGrid g;
    g.w = w;
    g.h = h;
    g.data.assign(static_cast<std::size_t>(w * h), land_elev);
    for (int y = 0; y < h; ++y)
        g.data[static_cast<std::size_t>(y * w)] = ocean_elev;  // column 0, matches make_coastline_biome
    return g;
}

} // namespace

TEST(BiomeRefinementTest, CoastalBeachOverEmptyGridIsANoOp) {
    BiomeGrid  biome;
    FieldGrid  elevation;
    BiomeRefinement::applyCoastalBeach(biome, elevation, 0.0);
    EXPECT_TRUE(biome.empty());
}

TEST(BiomeRefinementTest, CoastalBeachMismatchedGridSizesIsANoOp) {
    BiomeGrid  biome = make_coastline_biome(5, 5);
    const BiomeGrid original = biome;
    FieldGrid  elevation = make_flat_land_elevation(4, 4, 10.0f, -10.0f);

    BiomeRefinement::applyCoastalBeach(biome, elevation, 0.0);
    EXPECT_EQ(biome.data, original.data);
}

TEST(BiomeRefinementTest, ZeroOrNegativeRadiusIsANoOp) {
    BiomeGrid  biome = make_coastline_biome();
    const BiomeGrid original = biome;
    FieldGrid  elevation = make_flat_land_elevation(5, 5, 10.0f, -10.0f);

    BiomeRefinement::applyCoastalBeach(biome, elevation, 0.0, /*radius_cells=*/0);
    EXPECT_EQ(biome.data, original.data);
}

TEST(BiomeRefinementTest, LandWithinRadiusOfOceanBecomesBeach) {
    BiomeGrid  biome     = make_coastline_biome();
    FieldGrid  elevation = make_flat_land_elevation(5, 5, 10.0f, -10.0f);

    BiomeRefinement::applyCoastalBeach(biome, elevation, /*sea_level_m=*/0.0, /*radius_cells=*/1);

    for (int y = 0; y < 5; ++y) {
        EXPECT_EQ(biome.at(0, y), OCEAN) << "y=" << y << " ocean column must stay ocean";
        EXPECT_EQ(biome.at(1, y), BEACH) << "y=" << y << " column 1 is adjacent to ocean";
        // Columns 2+ are more than 1 cell from the ocean column -> unchanged.
        EXPECT_EQ(biome.at(2, y), MEADOW) << "y=" << y;
        EXPECT_EQ(biome.at(3, y), MEADOW) << "y=" << y;
        EXPECT_EQ(biome.at(4, y), MEADOW) << "y=" << y;
    }
}

TEST(BiomeRefinementTest, RadiusControlsHowFarBeachReaches) {
    BiomeGrid  biome     = make_coastline_biome();
    FieldGrid  elevation = make_flat_land_elevation(5, 5, 10.0f, -10.0f);

    BiomeRefinement::applyCoastalBeach(biome, elevation, 0.0, /*radius_cells=*/2);

    for (int y = 0; y < 5; ++y) {
        EXPECT_EQ(biome.at(1, y), BEACH) << "y=" << y;
        EXPECT_EQ(biome.at(2, y), BEACH) << "y=" << y << " now within radius 2";
        EXPECT_EQ(biome.at(3, y), MEADOW) << "y=" << y << " still out of range";
    }
}

TEST(BiomeRefinementTest, HighElevationCoastDoesNotBecomeBeach) {
    // Same coastline layout, but the land is classified as mountain and
    // sits far above sea level -- a cliff, not a beach.
    BiomeGrid biome = make_coastline_biome();
    for (auto& v : biome.data)
        if (v == MEADOW) v = MOUNTAIN;

    FieldGrid elevation = make_flat_land_elevation(5, 5, /*land_elev=*/3000.0f, -10.0f);

    BiomeRefinement::applyCoastalBeach(biome, elevation, /*sea_level_m=*/0.0, /*radius_cells=*/1,
                                        /*max_beach_elevation_m=*/50.0);

    for (int y = 0; y < 5; ++y) EXPECT_EQ(biome.at(1, y), MOUNTAIN) << "y=" << y << " cliff stays mountain";
}

TEST(BiomeRefinementTest, OceanCellsAreNeverReclassified) {
    BiomeGrid  biome     = make_coastline_biome();
    FieldGrid  elevation = make_flat_land_elevation(5, 5, 10.0f, -10.0f);

    BiomeRefinement::applyCoastalBeach(biome, elevation, 0.0, /*radius_cells=*/4);

    for (int y = 0; y < 5; ++y) EXPECT_EQ(biome.at(0, y), OCEAN) << "y=" << y;
}

// M236-M275 (MAP16, 2026-07-10): classify() now produces 6 distinct
// underwater outcomes (see ZoneType.hpp's is_ocean_family()), not always
// plain ZoneType::ocean -- this proves applyCoastalBeach() was updated to
// recognize all of them, not just the literal ocean ordinal. Regression
// test for a real bug found the same day: before the fix, a kelp_forest
// (or any of the other 4 new underwater sub-types) neighbor was invisible
// to both the "skip ocean cells" check and the "is this land cell near
// ocean" scan, so land next to anything but plain ocean silently never
// became beach, and (worse) the underwater cell itself could be
// overwritten to beach.
TEST(BiomeRefinementTest, NonLiteralOceanFamilyCellsStillTriggerAndAreNeverReclassified) {
    const std::uint8_t kelp_forest = static_cast<std::uint8_t>(ZoneType::kelp_forest);
    BiomeGrid  biome     = make_coastline_biome();
    biome.data[0] = kelp_forest;  // column 0, row 0 -- was OCEAN, now a different ocean-family value
    FieldGrid  elevation = make_flat_land_elevation(5, 5, 10.0f, -10.0f);

    BiomeRefinement::applyCoastalBeach(biome, elevation, 0.0, /*radius_cells=*/1);

    EXPECT_EQ(biome.at(0, 0), kelp_forest) << "the kelp_forest cell itself must never become beach";
    EXPECT_EQ(biome.at(1, 0), BEACH) << "its land neighbor must still become beach";
}

TEST(BiomeRefinementTest, EdgeCellsAreEligibleForBeach) {
    // Deliberately verifies biome refinement does NOT apply elevation's
    // edge-protection discipline: (0,0) is ocean and is literally on the
    // grid's edge, and its land neighbor (1,0) -- also on the edge -- must
    // still become beach.
    BiomeGrid  biome     = make_coastline_biome();
    FieldGrid  elevation = make_flat_land_elevation(5, 5, 10.0f, -10.0f);

    BiomeRefinement::applyCoastalBeach(biome, elevation, 0.0, /*radius_cells=*/1);

    EXPECT_EQ(biome.at(1, 0), BEACH) << "top row, adjacent to ocean";
    EXPECT_EQ(biome.at(1, 4), BEACH) << "bottom row, adjacent to ocean";
}

// M135 — a soundness sweep over an irregular coastline (not just the
// straight lines the tests above use): every resulting beach cell must
// have an ocean neighbor within radius_cells. Guaranteed by construction
// (applyCoastalBeach only ever sets beach inside its own near_ocean
// branch), but this pins the invariant explicitly against a less trivial
// layout rather than relying on that being obvious from the implementation.
TEST(BiomeRefinementTest, EveryBeachCellHasAnOceanNeighborWithinRadius) {
    constexpr int W = 8, H = 8;
    BiomeGrid     biome;
    biome.w = W;
    biome.h = H;
    biome.data.assign(static_cast<std::size_t>(W * H), MEADOW);

    auto set_biome = [&](int x, int y, std::uint8_t v) {
        biome.data[static_cast<std::size_t>(y * W + x)] = v;
    };
    for (int y = 0; y < H; ++y) set_biome(0, y, OCEAN);      // left column
    for (int x = 0; x < 4; ++x) set_biome(x, H - 1, OCEAN);  // bottom-left row (an L-shaped coast)
    set_biome(6, 2, OCEAN);                                  // an isolated inland ocean cell
    set_biome(3, 5, MOUNTAIN);                               // a peak elsewhere; shouldn't matter here

    FieldGrid elevation;
    elevation.w = W;
    elevation.h = H;
    elevation.data.assign(static_cast<std::size_t>(W * H), 10.0f);
    auto set_elev = [&](int x, int y, float v) { elevation.data[static_cast<std::size_t>(y * W + x)] = v; };
    for (int y = 0; y < H; ++y) set_elev(0, y, -10.0f);
    for (int x = 0; x < 4; ++x) set_elev(x, H - 1, -10.0f);
    set_elev(6, 2, -10.0f);
    set_elev(3, 5, 3000.0f);

    constexpr int radius = 1;
    BiomeRefinement::applyCoastalBeach(biome, elevation, /*sea_level_m=*/0.0, radius);

    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            if (biome.at(x, y) != BEACH) continue;

            bool has_ocean_neighbor = false;
            for (int dy = -radius; dy <= radius && !has_ocean_neighbor; ++dy) {
                const int ny = y + dy;
                if (ny < 0 || ny >= H) continue;
                for (int dx = -radius; dx <= radius; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    const int nx = x + dx;
                    if (nx < 0 || nx >= W) continue;
                    if (biome.at(nx, ny) == OCEAN) {
                        has_ocean_neighbor = true;
                        break;
                    }
                }
            }
            EXPECT_TRUE(has_ocean_neighbor) << "beach cell (" << x << "," << y << ") has no ocean neighbor";
        }
    }
}

// --- M130: applySwampFlatnessCheck() ---

TEST(BiomeRefinementTest, SwampFlatnessOverEmptyGridIsANoOp) {
    BiomeGrid biome;
    FieldGrid elevation;
    BiomeRefinement::applySwampFlatnessCheck(biome, elevation, 150.0);
    EXPECT_TRUE(biome.empty());
}

TEST(BiomeRefinementTest, SwampFlatnessMismatchedGridSizesIsANoOp) {
    BiomeGrid biome;
    biome.w = biome.h = 3;
    biome.data.assign(9, SWAMP);
    const BiomeGrid original = biome;
    FieldGrid elevation;
    elevation.w = elevation.h = 2;
    elevation.data.assign(4, 0.0f);

    BiomeRefinement::applySwampFlatnessCheck(biome, elevation, 150.0);
    EXPECT_EQ(biome.data, original.data);
}

TEST(BiomeRefinementTest, FlatSwampStaysSwamp) {
    BiomeGrid biome;
    biome.w = biome.h = 3;
    biome.data.assign(9, SWAMP);
    FieldGrid elevation;
    elevation.w = elevation.h = 3;
    elevation.data.assign(9, 10.0f);  // perfectly flat

    BiomeRefinement::applySwampFlatnessCheck(biome, elevation, /*max_local_relief_m=*/150.0);

    for (auto v : biome.data) EXPECT_EQ(v, SWAMP);
}

TEST(BiomeRefinementTest, SteepSwampIsDemotedToMeadow) {
    BiomeGrid biome;
    biome.w = biome.h = 3;
    biome.data.assign(9, SWAMP);
    FieldGrid elevation;
    elevation.w = elevation.h = 3;
    // Bottom-right corner (2,2) sits 500 m above the rest -- a real slope,
    // not a floodplain. Every cell whose 3x3 neighborhood reaches (2,2)
    // sees that relief; (0,0) is far enough away that its own neighborhood
    // never touches (2,2) and stays perfectly flat.
    elevation.data = {10, 10, 10,
                       10, 10, 10,
                       10, 10, 510};

    BiomeRefinement::applySwampFlatnessCheck(biome, elevation, /*max_local_relief_m=*/150.0);

    EXPECT_EQ(biome.at(0, 0), SWAMP) << "far corner's neighborhood never reaches the 510 cell";
    EXPECT_EQ(biome.at(1, 1), MEADOW) << "center's neighborhood includes the 510 cell";
    EXPECT_EQ(biome.at(2, 2), MEADOW) << "the high cell's own neighborhood includes the flat 10s too";
}

TEST(BiomeRefinementTest, NonSwampCellsAreNeverTouched) {
    BiomeGrid biome;
    biome.w = biome.h = 3;
    biome.data.assign(9, MEADOW);
    const BiomeGrid original = biome;
    FieldGrid elevation;
    elevation.w = elevation.h = 3;
    elevation.data = {10, 10, 10, 10, 10, 10, 10, 10, 5000};  // extreme relief, irrelevant

    BiomeRefinement::applySwampFlatnessCheck(biome, elevation, 150.0);
    EXPECT_EQ(biome.data, original.data);
}

// --- M259: applyCanyonCarving() ---

TEST(BiomeRefinementTest, CanyonCarvingOverEmptyGridIsANoOp) {
    BiomeGrid biome;
    FieldGrid elevation;
    BiomeRefinement::applyCanyonCarving(biome, elevation, 80.0);
    EXPECT_TRUE(biome.empty());
}

TEST(BiomeRefinementTest, CanyonCarvingMismatchedGridSizesIsANoOp) {
    BiomeGrid biome;
    biome.w = biome.h = 3;
    biome.data.assign(9, BADLANDS);
    const BiomeGrid original = biome;
    FieldGrid elevation;
    elevation.w = elevation.h = 2;
    elevation.data.assign(4, 0.0f);

    BiomeRefinement::applyCanyonCarving(biome, elevation, 80.0);
    EXPECT_EQ(biome.data, original.data);
}

TEST(BiomeRefinementTest, FlatBadlandsStaysBadlands) {
    BiomeGrid biome;
    biome.w = biome.h = 3;
    biome.data.assign(9, BADLANDS);
    FieldGrid elevation;
    elevation.w = elevation.h = 3;
    elevation.data.assign(9, 500.0f);  // perfectly flat

    BiomeRefinement::applyCanyonCarving(biome, elevation, /*steep_relief_m=*/80.0);

    for (auto v : biome.data) EXPECT_EQ(v, BADLANDS);
}

TEST(BiomeRefinementTest, SteepBadlandsBecomesCanyon) {
    BiomeGrid biome;
    biome.w = biome.h = 3;
    biome.data.assign(9, BADLANDS);
    FieldGrid elevation;
    elevation.w = elevation.h = 3;
    // Same shape as SteepSwampIsDemotedToMeadow above: (2,2) sits 500 m
    // above the rest, a real slope.
    elevation.data = {500, 500, 500,
                       500, 500, 500,
                       500, 500, 1000};

    BiomeRefinement::applyCanyonCarving(biome, elevation, /*steep_relief_m=*/80.0);

    EXPECT_EQ(biome.at(0, 0), BADLANDS) << "far corner's neighborhood never reaches the 1000 cell";
    EXPECT_EQ(biome.at(1, 1), CANYON) << "center's neighborhood includes the 1000 cell";
    EXPECT_EQ(biome.at(2, 2), CANYON) << "the high cell's own neighborhood includes the flat 500s too";
}

TEST(BiomeRefinementTest, SteepMesaBecomesCanyon) {
    BiomeGrid biome;
    biome.w = biome.h = 3;
    biome.data.assign(9, MESA);
    FieldGrid elevation;
    elevation.w = elevation.h = 3;
    elevation.data = {500, 500, 500, 500, 500, 500, 500, 500, 1000};

    BiomeRefinement::applyCanyonCarving(biome, elevation, 80.0);
    EXPECT_EQ(biome.at(1, 1), CANYON);
}

TEST(BiomeRefinementTest, SteepRockyDesertBecomesCanyon) {
    BiomeGrid biome;
    biome.w = biome.h = 3;
    biome.data.assign(9, ROCKY_DESERT);
    FieldGrid elevation;
    elevation.w = elevation.h = 3;
    elevation.data = {500, 500, 500, 500, 500, 500, 500, 500, 1000};

    BiomeRefinement::applyCanyonCarving(biome, elevation, 80.0);
    EXPECT_EQ(biome.at(1, 1), CANYON);
}

TEST(BiomeRefinementTest, NonDesertCellsAreNeverTouchedByCanyonCarving) {
    BiomeGrid biome;
    biome.w = biome.h = 3;
    biome.data.assign(9, MEADOW);
    const BiomeGrid original = biome;
    FieldGrid elevation;
    elevation.w = elevation.h = 3;
    elevation.data = {10, 10, 10, 10, 10, 10, 10, 10, 5000};  // extreme relief, irrelevant

    BiomeRefinement::applyCanyonCarving(biome, elevation, 80.0);
    EXPECT_EQ(biome.data, original.data);
}

// --- M274/M275: applyCoastalReliefRefinement() ---

TEST(BiomeRefinementTest, CoastalReliefOverEmptyGridIsANoOp) {
    BiomeGrid biome;
    FieldGrid elevation;
    BiomeRefinement::applyCoastalReliefRefinement(biome, elevation, 0.0);
    EXPECT_TRUE(biome.empty());
}

TEST(BiomeRefinementTest, CoastalReliefMismatchedGridSizesIsANoOp) {
    BiomeGrid biome = make_coastline_biome(5, 5);
    const BiomeGrid original = biome;
    FieldGrid elevation = make_flat_land_elevation(4, 4, 10.0f, -10.0f);

    BiomeRefinement::applyCoastalReliefRefinement(biome, elevation, 0.0);
    EXPECT_EQ(biome.data, original.data);
}

TEST(BiomeRefinementTest, CoastalReliefZeroOrNegativeRadiusIsANoOp) {
    BiomeGrid biome = make_coastline_biome();
    const BiomeGrid original = biome;
    FieldGrid elevation = make_flat_land_elevation(5, 5, 10.0f, -10.0f);

    BiomeRefinement::applyCoastalReliefRefinement(biome, elevation, 0.0, /*radius_cells=*/0);
    EXPECT_EQ(biome.data, original.data);
}

TEST(BiomeRefinementTest, FlatLowCoastBecomesTidalFlat) {
    // Column 0 ocean, columns 1-4 land, perfectly flat land at 2 m -- well
    // within the beach elevation band and (once ocean neighbors are
    // excluded from the relief scan) perfectly flat.
    BiomeGrid biome     = make_coastline_biome(5, 5);
    FieldGrid elevation = make_flat_land_elevation(5, 5, /*land_elev=*/2.0f, /*ocean_elev=*/-10.0f);

    BiomeRefinement::applyCoastalReliefRefinement(biome, elevation, /*sea_level_m=*/0.0,
                                                    /*radius_cells=*/1);

    for (int y = 0; y < 5; ++y) {
        EXPECT_EQ(biome.at(0, y), OCEAN) << "y=" << y;
        EXPECT_EQ(biome.at(1, y), TIDAL_FLAT) << "y=" << y << " flat land right at the coast";
        EXPECT_EQ(biome.at(2, y), MEADOW) << "y=" << y << " outside the coastal radius, untouched";
    }
}

TEST(BiomeRefinementTest, SteepLandNearCoastBecomesSeaCliff) {
    // 3x3: column 0 ocean; columns 1-2 land, with (2,2) bumped 600 m above
    // the rest of the LAND -- the ocean's own -10 m depth must not be what
    // triggers this (that's the local_land_relief_m() masking under test).
    BiomeGrid biome;
    biome.w = biome.h = 3;
    biome.data = {OCEAN, MEADOW, MEADOW,
                  OCEAN, MEADOW, MEADOW,
                  OCEAN, MEADOW, MEADOW};
    FieldGrid elevation;
    elevation.w = elevation.h = 3;
    elevation.data = {-10, 10, 10,
                       -10, 10, 10,
                       -10, 10, 610};

    BiomeRefinement::applyCoastalReliefRefinement(biome, elevation, /*sea_level_m=*/0.0,
                                                    /*radius_cells=*/1);

    EXPECT_EQ(biome.at(1, 1), SEA_CLIFF) << "center's land neighborhood includes the 610 m bump";
    EXPECT_EQ(biome.at(1, 0), TIDAL_FLAT) << "top land cell's land neighborhood stays flat (10s only)";
    EXPECT_EQ(biome.at(2, 2), MEADOW) << "the bumped cell itself is 2 cells from the coast, untouched";
}

TEST(BiomeRefinementTest, TooHighCoastBecomesSeaCliffEvenIfFlat) {
    BiomeGrid  biome     = make_coastline_biome(5, 5);
    FieldGrid  elevation = make_flat_land_elevation(5, 5, /*land_elev=*/3000.0f, /*ocean_elev=*/-10.0f);

    BiomeRefinement::applyCoastalReliefRefinement(biome, elevation, /*sea_level_m=*/0.0,
                                                    /*radius_cells=*/1,
                                                    /*max_coastal_elevation_m=*/50.0);

    for (int y = 0; y < 5; ++y)
        EXPECT_EQ(biome.at(1, y), SEA_CLIFF) << "y=" << y << " too high above sea level for a beach";
}

TEST(BiomeRefinementTest, ModerateCoastalBandLeftUnchanged) {
    // Same 3x3 shape as SteepLandNearCoastBecomesSeaCliff, but the land
    // bump is only 40 m (not 600 m) -- land-only relief at (1,1)/(1,2)
    // lands strictly between the default flat/steep thresholds (15/80),
    // and both cells' own elevation stays within the beach band (<= 50 m
    // above sea level), so neither branch should fire.
    BiomeGrid biome;
    biome.w = biome.h = 3;
    biome.data = {OCEAN, BEACH, BEACH,
                  OCEAN, BEACH, BEACH,
                  OCEAN, BEACH, BEACH};  // pre-set by an earlier applyCoastalBeach() pass
    FieldGrid elevation;
    elevation.w = elevation.h = 3;
    // Same layout as SteepLandNearCoastBecomesSeaCliff (bump at (2,2)), but
    // 50 m instead of 610 m -- both (1,1)'s and (1,2)'s land-only 3x3
    // window includes (2,2), giving each a 40 m relief (50-10).
    elevation.data = {-10, 10, 10,
                       -10, 10, 10,
                       -10, 10, 50};

    BiomeRefinement::applyCoastalReliefRefinement(biome, elevation, /*sea_level_m=*/0.0,
                                                    /*radius_cells=*/1);

    EXPECT_EQ(biome.at(1, 1), BEACH) << "land-only relief (40 m) sits between the two thresholds";
    EXPECT_EQ(biome.at(1, 2), BEACH) << "same relief, and its own 10 m elevation is still <= the cap";
}

TEST(BiomeRefinementTest, NonCoastalCellsAreNeverTouchedByCoastalRelief) {
    BiomeGrid biome;
    biome.w = biome.h = 3;
    biome.data.assign(9, MEADOW);
    const BiomeGrid original = biome;
    FieldGrid elevation;
    elevation.w = elevation.h = 3;
    elevation.data = {10, 10, 10, 10, 10, 10, 10, 10, 5000};  // extreme relief, but never near ocean

    BiomeRefinement::applyCoastalReliefRefinement(biome, elevation, 0.0, /*radius_cells=*/1);
    EXPECT_EQ(biome.data, original.data);
}

TEST(BiomeRefinementTest, OceanCellsAreNeverReclassifiedByCoastalRelief) {
    BiomeGrid  biome     = make_coastline_biome();
    FieldGrid  elevation = make_flat_land_elevation(5, 5, 10.0f, -10.0f);

    BiomeRefinement::applyCoastalReliefRefinement(biome, elevation, 0.0, /*radius_cells=*/4);

    for (int y = 0; y < 5; ++y) EXPECT_EQ(biome.at(0, y), OCEAN) << "y=" << y;
}

// --- M247: applyRiparianForest() ---

namespace {
// 5x5 grid over world bounds [0,50]x[0,50] -> 10 m cells, all `fill`.
BiomeGrid make_uniform_biome(std::uint8_t fill) {
    BiomeGrid g;
    g.w = g.h = 5;
    g.data.assign(25, fill);
    return g;
}

HydrologyNetwork make_single_point_network(double x, double z) {
    HydrologyNetwork net;
    RiverSegment seg;
    seg.points.push_back(RiverPoint{x, z, 1.0f});
    net.rivers.push_back(seg);
    return net;
}
} // namespace

TEST(BiomeRefinementTest, RiparianForestOverEmptyGridIsANoOp) {
    BiomeGrid biome;
    HydrologyNetwork network = make_single_point_network(0.0, 0.0);
    BiomeRefinement::applyRiparianForest(biome, network, 0, 0, 0.0, 0.0, 50.0, 50.0);
    EXPECT_TRUE(biome.empty());
}

TEST(BiomeRefinementTest, RiparianForestMismatchedGridDimsIsANoOp) {
    BiomeGrid biome = make_uniform_biome(SAVANNA);
    const BiomeGrid original = biome;
    HydrologyNetwork network = make_single_point_network(25.0, 5.0);

    BiomeRefinement::applyRiparianForest(biome, network, 4, 4, 0.0, 0.0, 50.0, 50.0);
    EXPECT_EQ(biome.data, original.data);
}

TEST(BiomeRefinementTest, RiparianForestDegenerateBoundsIsANoOp) {
    BiomeGrid biome = make_uniform_biome(SAVANNA);
    const BiomeGrid original = biome;
    HydrologyNetwork network = make_single_point_network(25.0, 5.0);

    BiomeRefinement::applyRiparianForest(biome, network, 5, 5, 50.0, 0.0, 50.0, 50.0);  // x1 == x0
    EXPECT_EQ(biome.data, original.data);
}

TEST(BiomeRefinementTest, RiparianForestZeroOrNegativeRadiusIsANoOp) {
    BiomeGrid biome = make_uniform_biome(SAVANNA);
    const BiomeGrid original = biome;
    HydrologyNetwork network = make_single_point_network(25.0, 5.0);

    BiomeRefinement::applyRiparianForest(biome, network, 5, 5, 0.0, 0.0, 50.0, 50.0,
                                          /*radius_cells=*/0);
    EXPECT_EQ(biome.data, original.data);
}

TEST(BiomeRefinementTest, RiparianForestNoRiversIsANoOp) {
    BiomeGrid biome = make_uniform_biome(SAVANNA);
    const BiomeGrid original = biome;
    HydrologyNetwork network;  // no rivers at all

    BiomeRefinement::applyRiparianForest(biome, network, 5, 5, 0.0, 0.0, 50.0, 50.0);
    EXPECT_EQ(biome.data, original.data);
}

TEST(BiomeRefinementTest, RiparianForestSegmentWithNoPointsIsANoOp) {
    BiomeGrid biome = make_uniform_biome(SAVANNA);
    const BiomeGrid original = biome;
    HydrologyNetwork network;
    network.rivers.push_back(RiverSegment{});  // a real segment, but zero points

    BiomeRefinement::applyRiparianForest(biome, network, 5, 5, 0.0, 0.0, 50.0, 50.0);
    EXPECT_EQ(biome.data, original.data);
}

TEST(BiomeRefinementTest, SavannaNearRiverPointBecomesRiparianForestInACircularPattern) {
    // River point sits exactly at cell (2,0)'s own center (25, 5). At the
    // default radius_cells=1, max distance is 10 m: the 3 orthogonal
    // neighbors (also exactly 10 m away) qualify, but the diagonal neighbor
    // (10*sqrt(2) =~ 14.14 m away) does not -- a circular Euclidean falloff,
    // distinct from the Chebyshev-square test applyCoastalBeach()/
    // applyCoastalReliefRefinement() use (river points are continuous
    // world-space coordinates, not grid-aligned).
    BiomeGrid biome = make_uniform_biome(SAVANNA);
    HydrologyNetwork network = make_single_point_network(25.0, 5.0);

    BiomeRefinement::applyRiparianForest(biome, network, 5, 5, 0.0, 0.0, 50.0, 50.0,
                                          /*radius_cells=*/1);

    EXPECT_EQ(biome.at(2, 0), RIPARIAN_FOREST) << "river point's own cell";
    EXPECT_EQ(biome.at(1, 0), RIPARIAN_FOREST) << "west neighbor, 10 m away";
    EXPECT_EQ(biome.at(3, 0), RIPARIAN_FOREST) << "east neighbor, 10 m away";
    EXPECT_EQ(biome.at(2, 1), RIPARIAN_FOREST) << "south neighbor, 10 m away";
    EXPECT_EQ(biome.at(1, 1), SAVANNA) << "diagonal neighbor, ~14.14 m away -- outside radius";
    EXPECT_EQ(biome.at(0, 0), SAVANNA) << "far corner, well outside radius";
}

TEST(BiomeRefinementTest, RadiusControlsHowFarRiparianForestReaches) {
    BiomeGrid biome = make_uniform_biome(SAVANNA);
    HydrologyNetwork network = make_single_point_network(25.0, 5.0);

    BiomeRefinement::applyRiparianForest(biome, network, 5, 5, 0.0, 0.0, 50.0, 50.0,
                                          /*radius_cells=*/3);  // max distance 30 m

    EXPECT_EQ(biome.at(1, 1), RIPARIAN_FOREST) << "diagonal neighbor (~14.14 m) now within 30 m";
    EXPECT_EQ(biome.at(0, 0), RIPARIAN_FOREST) << "20 m away, within 30 m";
    EXPECT_EQ(biome.at(4, 4), SAVANNA) << "~44.7 m away, still outside even the larger radius";
}

TEST(BiomeRefinementTest, NonTargetBiomesAreNeverTouchedByRiparianForest) {
    BiomeGrid biome = make_uniform_biome(FOREST);
    const BiomeGrid original = biome;
    HydrologyNetwork network = make_single_point_network(25.0, 5.0);  // right at cell (2,0)

    BiomeRefinement::applyRiparianForest(biome, network, 5, 5, 0.0, 0.0, 50.0, 50.0);
    EXPECT_EQ(biome.data, original.data);
}

TEST(BiomeRefinementTest, AllFiveGrasslandFamilyBiomesConvertNearARiver) {
    for (std::uint8_t start : {STEPPE, PRAIRIE, CHAPARRAL, SHRUBLAND}) {
        BiomeGrid biome = make_uniform_biome(start);
        HydrologyNetwork network = make_single_point_network(25.0, 5.0);

        BiomeRefinement::applyRiparianForest(biome, network, 5, 5, 0.0, 0.0, 50.0, 50.0);
        EXPECT_EQ(biome.at(2, 0), RIPARIAN_FOREST) << "starting biome ordinal " << static_cast<int>(start);
    }
}

// --- M265-268: applyVolcanicBiomes() ---

namespace {
VolcanicField make_single_hotspot_field(double x, double z, double radius_m, bool active) {
    VolcanicField field;
    field.hotspots.push_back(VolcanicHotspot{x, z, /*peak_elevation_m=*/2000.0, radius_m, active});
    return field;
}
} // namespace

TEST(BiomeRefinementTest, VolcanicBiomesOverEmptyGridIsANoOp) {
    BiomeGrid biome;
    VolcanicField field = make_single_hotspot_field(0.0, 0.0, 10.0, true);
    BiomeRefinement::applyVolcanicBiomes(biome, field, 0, 0, 0.0, 0.0, 50.0, 50.0);
    EXPECT_TRUE(biome.empty());
}

TEST(BiomeRefinementTest, VolcanicBiomesMismatchedGridDimsIsANoOp) {
    BiomeGrid biome = make_uniform_biome(MEADOW);
    const BiomeGrid original = biome;
    VolcanicField field = make_single_hotspot_field(25.0, 25.0, 15.0, true);

    BiomeRefinement::applyVolcanicBiomes(biome, field, 4, 4, 0.0, 0.0, 50.0, 50.0);
    EXPECT_EQ(biome.data, original.data);
}

TEST(BiomeRefinementTest, VolcanicBiomesDegenerateBoundsIsANoOp) {
    BiomeGrid biome = make_uniform_biome(MEADOW);
    const BiomeGrid original = biome;
    VolcanicField field = make_single_hotspot_field(25.0, 25.0, 15.0, true);

    BiomeRefinement::applyVolcanicBiomes(biome, field, 5, 5, 50.0, 0.0, 50.0, 50.0);  // x1 == x0
    EXPECT_EQ(biome.data, original.data);
}

TEST(BiomeRefinementTest, VolcanicBiomesNoHotspotsIsANoOp) {
    BiomeGrid biome = make_uniform_biome(MEADOW);
    const BiomeGrid original = biome;
    VolcanicField field;  // empty

    BiomeRefinement::applyVolcanicBiomes(biome, field, 5, 5, 0.0, 0.0, 50.0, 50.0);
    EXPECT_EQ(biome.data, original.data);
}

TEST(BiomeRefinementTest, DormantHotspotBecomesAshPlainRegardlessOfDistanceWithinRadius) {
    BiomeGrid biome = make_uniform_biome(MEADOW);
    // Hotspot at cell (2,2)'s own center (25,25), radius 15 m.
    VolcanicField field = make_single_hotspot_field(25.0, 25.0, /*radius_m=*/15.0, /*active=*/false);

    BiomeRefinement::applyVolcanicBiomes(biome, field, 5, 5, 0.0, 0.0, 50.0, 50.0);

    EXPECT_EQ(biome.at(2, 2), ASH_PLAIN) << "at the hotspot's own center";
    EXPECT_EQ(biome.at(1, 1), ASH_PLAIN) << "14.14 m away, within the 15 m radius";
    EXPECT_EQ(biome.at(0, 0), MEADOW) << "28.28 m away, outside the 15 m radius";
}

TEST(BiomeRefinementTest, ActiveInlandHotspotSplitsIntoVolcanicCoreAndGeothermalPeriphery) {
    BiomeGrid biome = make_uniform_biome(MEADOW);  // no ocean anywhere in this grid
    // Hotspot at cell (2,2)'s own center (25,25), radius 15 m -> inner
    // (volcanic) threshold at the default inner_fraction=0.4 is 6 m.
    VolcanicField field = make_single_hotspot_field(25.0, 25.0, /*radius_m=*/15.0, /*active=*/true);

    BiomeRefinement::applyVolcanicBiomes(biome, field, 5, 5, 0.0, 0.0, 50.0, 50.0);

    EXPECT_EQ(biome.at(2, 2), VOLCANIC) << "0 m away, well within the 6 m inner threshold";
    EXPECT_EQ(biome.at(1, 1), GEOTHERMAL) << "14.14 m away: within the 15 m radius but past the inner threshold";
    EXPECT_EQ(biome.at(0, 0), MEADOW) << "28.28 m away, outside the 15 m radius entirely";
}

TEST(BiomeRefinementTest, ActiveHotspotNearCoastBecomesVolcanicIslandInsteadOfVolcanicOrGeothermal) {
    BiomeGrid biome = make_coastline_biome(5, 5);  // column 0 = ocean
    // Hotspot exactly at cell (1,0)'s own center (15,5) -- adjacent to the
    // ocean column, so within coastal_radius_cells=1 of it.
    VolcanicField field = make_single_hotspot_field(15.0, 5.0, /*radius_m=*/15.0, /*active=*/true);

    BiomeRefinement::applyVolcanicBiomes(biome, field, 5, 5, 0.0, 0.0, 50.0, 50.0);

    EXPECT_EQ(biome.at(1, 0), VOLCANIC_ISLAND) << "at the hotspot's own center, and coast-adjacent";
    EXPECT_EQ(biome.at(2, 0), GEOTHERMAL) << "10 m from the hotspot but NOT itself coast-adjacent";
    for (int y = 0; y < 5; ++y) EXPECT_EQ(biome.at(0, y), OCEAN) << "y=" << y << " ocean column untouched";
}

TEST(BiomeRefinementTest, CellsOutsideEveryHotspotRadiusAreNeverTouched) {
    BiomeGrid biome = make_uniform_biome(MEADOW);
    const BiomeGrid original = biome;
    // Hotspot far outside the grid's own world bounds -- no cell qualifies.
    VolcanicField field = make_single_hotspot_field(-10000.0, -10000.0, /*radius_m=*/5.0,
                                                      /*active=*/true);

    BiomeRefinement::applyVolcanicBiomes(biome, field, 5, 5, 0.0, 0.0, 50.0, 50.0);
    EXPECT_EQ(biome.data, original.data);
}

TEST(BiomeRefinementTest, NearestHotspotWinsForVolcanicClassificationWhenOverlapping) {
    // Two active hotspots both reach cell (2,2) (world center 25,25), but
    // the SECOND one in the field (dist 0) is nearer than the FIRST (dist
    // 10) -- proving classification follows true nearest distance, not
    // insertion order.
    BiomeGrid biome = make_uniform_biome(MEADOW);
    VolcanicField field;
    field.hotspots.push_back(VolcanicHotspot{15.0, 25.0, 2000.0, /*radius_m=*/15.0, /*active=*/true});
    field.hotspots.push_back(VolcanicHotspot{25.0, 25.0, 2000.0, /*radius_m=*/12.0, /*active=*/true});

    BiomeRefinement::applyVolcanicBiomes(biome, field, 5, 5, 0.0, 0.0, 50.0, 50.0);

    // The nearer hotspot's own inner threshold (12*0.4=4.8) applies: dist 0
    // < 4.8 -> volcanic. Had the farther, FIRST-listed hotspot won instead
    // (dist 10 against its own 15*0.4=6 threshold), the result would have
    // been geothermal.
    EXPECT_EQ(biome.at(2, 2), VOLCANIC);
}
