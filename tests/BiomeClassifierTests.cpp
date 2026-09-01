// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M063 tests: BiomeClassifier threshold coverage.
// M236-M275 (MAP16, 2026-07-10): classify() grew from 12 to (32 of 40 new,
// see BiomeClassifier.cpp's own header comment) reachable outcomes. Three
// original tests below (BelowSeaLevelIsOcean, OceanPriorityIsAbsolute,
// HotDryIsDesert) had specific input values that are themselves now the
// most natural example of a more specific new biome (e.g. depth=1m + very
// cold IS fjord, not generic "ocean"; moisture=0 at hot/dry/lowland IS
// salt_flat, not generic "desert") -- updated in place with a comment
// explaining the reclassification, plus a fresh assertion added to each
// proving the original, broader biome is still reachable via different,
// less extreme input values.

#include <gtest/gtest.h>
#include "Map/BiomeClassifier.hpp"

using MeshWorld::ZoneType;
using MeshWorld::Map::BiomeClassifier;

// Helper: call with sea_level_m = 0 unless stated otherwise.
static ZoneType cls(double elev, double temp, double moist, double sea = 0.0) {
    return BiomeClassifier::classify(elev, temp, moist, sea);
}

// M331 (MAP21) — same as cls(), but also passing a cavity_noise score.
static ZoneType cls_cavity(double elev, double temp, double moist, double sea, double cavity) {
    return BiomeClassifier::classify(elev, temp, moist, sea, cavity);
}

TEST(BiomeClassifierTest, BelowSeaLevelIsOcean) {
    // depth=1m, mild temp -> shallow, calm water right off the shore: now
    // lagoon specifically (M236-M275's ocean-family subtyping), not plain
    // ocean. depth=500m stays plain ocean (mid-depth band, untouched).
    // depth=50m at moderate temp -> kelp_forest (shallow, temperate).
    EXPECT_EQ(cls(-1.0,  20.0, 0.5), ZoneType::lagoon);
    EXPECT_EQ(cls(-500.0, 5.0, 0.0), ZoneType::ocean);
    EXPECT_EQ(cls( 50.0, 10.0, 0.5, 100.0), ZoneType::kelp_forest);  // sea_level=100, depth=50
    // Plain "ocean" is still reachable: mid-depth band (50m < depth <= 3000m).
    EXPECT_EQ(cls(-800.0, 15.0, 0.5), ZoneType::ocean);
}

TEST(BiomeClassifierTest, HighElevationIsMountain) {
    EXPECT_EQ(cls(2500.0, 10.0, 0.5), ZoneType::mountain);
    EXPECT_EQ(cls(4000.0, -2.0, 0.1), ZoneType::mountain);
}

TEST(BiomeClassifierTest, ColdLandIsTundra) {
    EXPECT_EQ(cls(100.0, -6.0, 0.0), ZoneType::tundra);
    EXPECT_EQ(cls(  1.0, -20.0, 0.9), ZoneType::tundra);
}

TEST(BiomeClassifierTest, HotWetIsJungle) {
    EXPECT_EQ(cls(100.0, 25.0, 0.7), ZoneType::jungle);
    EXPECT_EQ(cls(  1.0, 20.0, 1.0), ZoneType::jungle);
}

TEST(BiomeClassifierTest, HotDryIsDesert) {
    // moisture=0.0 (bone dry) is now salt_flat specifically; moisture=0.2
    // at low elevation is now rocky_desert (M236-M275's desert subtyping,
    // both more specific than the original catch-all "desert").
    EXPECT_EQ(cls(100.0, 25.0, 0.0), ZoneType::salt_flat);
    EXPECT_EQ(cls(  1.0, 20.0, 0.2), ZoneType::rocky_desert);
    // Plain "desert" is still reachable: moderate dryness (0.25-0.3) at
    // low-mid elevation, below badlands' own elevation requirement.
    EXPECT_EQ(cls(100.0, 25.0, 0.27), ZoneType::desert);
}

TEST(BiomeClassifierTest, WarmVeryWetIsSwamp) {
    EXPECT_EQ(cls(50.0, 15.0, 0.9), ZoneType::swamp);
    EXPECT_EQ(cls(10.0, 10.0, 1.0), ZoneType::swamp);
}

TEST(BiomeClassifierTest, TemperateWetIsForest) {
    EXPECT_EQ(cls(100.0, 12.0, 0.6), ZoneType::forest);
    EXPECT_EQ(cls(  1.0,  5.0, 0.8), ZoneType::forest);
}

TEST(BiomeClassifierTest, TemperateDryIsMeadow) {
    EXPECT_EQ(cls(100.0, 15.0, 0.3), ZoneType::meadow);
    EXPECT_EQ(cls(  1.0,  8.0, 0.0), ZoneType::meadow);
}

// Mountain check takes priority over tundra.
TEST(BiomeClassifierTest, MountainPriorityOverTundra) {
    EXPECT_EQ(cls(3000.0, -10.0, 0.0), ZoneType::mountain);
}

// M331 (MAP21) — ZoneType::cave, reachable via the new cavity_noise
// parameter: a mountain-tier cell with a high cavity score becomes a cave
// instead of plain mountain.
TEST(BiomeClassifierTest, MountainTierCaveIsReachableViaCavityNoise) {
    EXPECT_EQ(cls_cavity(3000.0, 10.0, 0.5, 0.0, 0.95), ZoneType::cave);
}

// Without an explicit cavity score (the pre-M331 default, cavity_noise=0.0
// via classify()'s own default argument), mountain-tier cells are
// completely unaffected -- every pre-M331 caller/test keeps working exactly
// as before.
TEST(BiomeClassifierTest, MountainStaysMountainWithoutAnExplicitCavityScore) {
    EXPECT_EQ(cls(3000.0, 10.0, 0.5), ZoneType::mountain);
    EXPECT_EQ(cls_cavity(3000.0, 10.0, 0.5, 0.0, 0.0), ZoneType::mountain);
}

// A cavity score just below CAVE_CAVITY_THRESHOLD (0.88) still resolves to
// plain mountain -- caves must stay a small minority of mountain-tier
// cells, not a coin-flip.
TEST(BiomeClassifierTest, CavityScoreJustBelowThresholdIsStillMountain) {
    EXPECT_EQ(cls_cavity(3000.0, 10.0, 0.5, 0.0, 0.87), ZoneType::mountain);
    EXPECT_EQ(cls_cavity(3000.0, 10.0, 0.5, 0.0, 0.88), ZoneType::cave)
        << "threshold itself is inclusive";
}

// ice_cap/glacier take priority over cave regardless of cavity_noise --
// caves are a temperate/dry-mountain feature in this classifier, not a
// glacial one.
TEST(BiomeClassifierTest, IceCapAndGlacierTakePriorityOverCaveRegardlessOfCavityNoise) {
    EXPECT_EQ(cls_cavity(3000.0, -25.0, 0.5, 0.0, 1.0), ZoneType::ice_cap);
    EXPECT_EQ(cls_cavity(3000.0, -18.0, 0.5, 0.0, 1.0), ZoneType::glacier);
}

// cavity_noise has no effect below mountain-tier elevation -- it's checked
// only inside the mountain-tier branch, never a hidden factor elsewhere in
// the cascade.
TEST(BiomeClassifierTest, CavityNoiseHasNoEffectBelowMountainTier) {
    EXPECT_EQ(cls_cavity(100.0, 12.0, 0.6, 0.0, 1.0), ZoneType::forest)
        << "same as TemperateWetIsForest, unaffected by a maxed-out cavity score";
}

// M129 — elevation alone forces alpine tundra between the alpine and
// mountain thresholds, regardless of how warm/wet the cell otherwise is.
TEST(BiomeClassifierTest, HighElevationBelowMountainIsAlpineTundra) {
    EXPECT_EQ(cls(2000.0, 25.0, 0.8), ZoneType::tundra) << "would be jungle without the alpine rule";
    EXPECT_EQ(cls(1500.0, 20.0, 0.0), ZoneType::tundra) << "at the alpine threshold itself";
    EXPECT_EQ(cls(2499.0, 30.0, 1.0), ZoneType::tundra) << "just below the mountain threshold";
}

// Below the alpine threshold, normal climate rules still apply unchanged.
TEST(BiomeClassifierTest, JustBelowAlpineThresholdUsesNormalClimateRules) {
    EXPECT_EQ(cls(1499.0, 25.0, 0.8), ZoneType::jungle);
}

// M130 — a warm, very wet cell well above sea level isn't a lowland
// floodplain, so it falls through to whatever the next-best climate match
// is (here: forest) instead of being forced into swamp.
TEST(BiomeClassifierTest, WarmVeryWetFarAboveSeaLevelIsNotSwamp) {
    EXPECT_EQ(cls(500.0, 15.0, 0.9), ZoneType::forest) << "would be swamp if elevation were ignored";
    EXPECT_EQ(cls(301.0, 10.0, 1.0), ZoneType::forest) << "just above the lowland threshold";
}

// Right at and below the lowland threshold, swamp still applies unchanged.
TEST(BiomeClassifierTest, WarmVeryWetAtLowlandThresholdIsStillSwamp) {
    EXPECT_EQ(cls(300.0, 10.0, 1.0), ZoneType::swamp) << "at the lowland threshold itself";
}

// Ocean check takes priority over everything else.
TEST(BiomeClassifierTest, OceanPriorityIsAbsolute) {
    // Still absolute in spirit -- below sea level always resolves to some
    // ocean-family type, never a land biome, regardless of temperature.
    // The SPECIFIC subtype now varies: depth=1m + very cold -> fjord;
    // depth=1m + very hot -> lagoon (both shallow-water subtypes of what
    // used to be undifferentiated "ocean").
    EXPECT_EQ(cls(-1.0, -30.0, 0.0), ZoneType::fjord);
    EXPECT_EQ(cls(-1.0,  35.0, 1.0), ZoneType::lagoon);
}

TEST(BiomeClassifierTest, EveryOceanFamilySubtypeIsReachable) {
    // All 6 ocean-family outcomes (M236-M275's depth/temperature subtyping)
    // are exercised by at least one combination.
    EXPECT_EQ(cls(-4000.0, 10.0, 0.5), ZoneType::deep_ocean);   // depth > 3000m
    EXPECT_EQ(cls(  -20.0, 26.0, 0.5), ZoneType::coral_reef);   // shallow, >8m deep, hot
    EXPECT_EQ(cls(  -20.0, 12.0, 0.5), ZoneType::kelp_forest);  // shallow, >8m deep, temperate
    EXPECT_EQ(cls(   -3.0, 15.0, 0.5), ZoneType::lagoon);       // very shallow (<=8m)
    EXPECT_EQ(cls(  -20.0, -5.0, 0.5), ZoneType::fjord);        // shallow, cold
    EXPECT_EQ(cls( -800.0, 15.0, 0.5), ZoneType::ocean);        // mid-depth
}

TEST(BiomeClassifierTest, LandFamilyNeverAppearsBelowSeaLevel) {
    static constexpr ZoneType kOceanFamily[] = {
        ZoneType::ocean, ZoneType::deep_ocean, ZoneType::coral_reef,
        ZoneType::kelp_forest, ZoneType::lagoon, ZoneType::fjord,
    };
    const auto is_ocean_family = [](ZoneType z) {
        for (const ZoneType o : kOceanFamily) if (z == o) return true;
        return false;
    };
    for (double temp = -40.0; temp <= 40.0; temp += 5.0) {
        for (double moist = 0.0; moist <= 1.0; moist += 0.25) {
            EXPECT_TRUE(is_ocean_family(cls(-100.0, temp, moist)))
                << "temp=" << temp << " moist=" << moist;
        }
    }
}

// ── M236-M275: the 27 new land-family biomes ─────────────────────────
// One reachability assertion per biome, grouped by the climate band each
// sits in (matches BiomeClassifier.cpp's own section comments). Each
// input was chosen to land clearly inside that biome's own threshold gap,
// away from any boundary this file's other tests already exercise.

TEST(BiomeClassifierTest, MountainTierColdSubtypesAreReachable) {
    EXPECT_EQ(cls(3000.0, -17.0, 0.5), ZoneType::glacier);
    EXPECT_EQ(cls(3000.0, -25.0, 0.5), ZoneType::ice_cap);
}

TEST(BiomeClassifierTest, HotBandDesertFamilySubtypesAreReachable) {
    EXPECT_EQ(cls(1000.0, 25.0, 0.1),  ZoneType::mesa);           // elevated + dry
    EXPECT_EQ(cls( 100.0, 25.0, 0.1),  ZoneType::dunes);          // low-mid dry, not extreme
    EXPECT_EQ(cls( 400.0, 25.0, 0.2),  ZoneType::badlands);       // moderate elevation, moderate dryness
}

TEST(BiomeClassifierTest, HotBandMiddleMoistureGapSubtypesAreReachable) {
    // DESERT_MOIST <= moisture < JUNGLE_MOIST (0.3-0.6): previously always
    // meadow, now savanna/mangrove/oasis/bamboo_forest/tropical_dry_forest.
    EXPECT_EQ(cls(  50.0, 25.0, 0.55), ZoneType::mangrove);           // coastal-lowland, wet
    EXPECT_EQ(cls(1000.0, 25.0, 0.58), ZoneType::oasis);              // elevated, wet spike
    EXPECT_EQ(cls( 400.0, 25.0, 0.52), ZoneType::bamboo_forest);
    EXPECT_EQ(cls( 400.0, 25.0, 0.42), ZoneType::tropical_dry_forest);
    EXPECT_EQ(cls( 400.0, 25.0, 0.32), ZoneType::savanna);
}

TEST(BiomeClassifierTest, WarmBandNewWetlandSubtypesAreReachable) {
    EXPECT_EQ(cls( 10.0, 15.0, 0.60), ZoneType::marsh);       // narrow coastal band, open wetland
    EXPECT_EQ(cls(200.0, 15.0, 0.47), ZoneType::floodplain);  // lowland, seasonal moisture
}

TEST(BiomeClassifierTest, WarmBandHighlandDrySubtypesAreReachable) {
    // Previously-always-meadow gap (moisture < FOREST_MOIST) at elevation
    // >= MIDLAND_ELEV_M, split into 4 dryness tiers.
    EXPECT_EQ(cls(1000.0, 15.0, 0.05), ZoneType::shrubland);
    EXPECT_EQ(cls(1000.0, 15.0, 0.15), ZoneType::chaparral);
    EXPECT_EQ(cls(1000.0, 15.0, 0.25), ZoneType::steppe);
    EXPECT_EQ(cls(1000.0, 15.0, 0.35), ZoneType::prairie);
    // Below MIDLAND_ELEV_M, the same dry gap still falls to meadow unchanged.
    EXPECT_EQ(cls(400.0, 15.0, 0.35), ZoneType::meadow);
}

TEST(BiomeClassifierTest, MildBandWetForestSubtypesAreReachable) {
    EXPECT_EQ(cls( 50.0, 7.0, 0.95), ZoneType::temperate_rainforest);
    EXPECT_EQ(cls( 50.0, 7.0, 0.90), ZoneType::bog);
    EXPECT_EQ(cls(1000.0, 7.0, 0.70), ZoneType::mixed_forest);
}

TEST(BiomeClassifierTest, CoolMidBandIsEntirelyNewAndFullyReachable) {
    // TUNDRA_TEMP_C <= temp < FOREST_TEMP_C (-5 to 5 °C): the original
    // cascade always fell through to meadow here (temp < 5 fails forest's
    // own threshold, temp < 10 fails swamp's) -- now 6 outcomes.
    EXPECT_EQ(cls(  50.0, 0.0, 0.70), ZoneType::taiga);
    EXPECT_EQ(cls(  50.0, 0.0, 0.50), ZoneType::muskeg);
    EXPECT_EQ(cls(  50.0, 0.0, 0.10), ZoneType::cold_desert);
    EXPECT_EQ(cls( 900.0, 0.0, 0.30), ZoneType::steppe);
    EXPECT_EQ(cls(1300.0, 0.0, 0.10), ZoneType::permafrost);
    EXPECT_EQ(cls(1300.0, 0.0, 0.60), ZoneType::alpine_meadow);
    EXPECT_EQ(cls(1300.0, 0.0, 0.35), ZoneType::steppe);
    // Still falls to meadow outside all 6 new carve-outs (low elevation,
    // moderate moisture -- none of taiga/muskeg/cold_desert's own gaps).
    EXPECT_EQ(cls(50.0, 0.0, 0.35), ZoneType::meadow);
}
