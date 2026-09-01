// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Map/BiomeClassifier.hpp"

namespace MeshWorld::Map {

// Threshold constants (all temperatures in °C, elevations in metres).
namespace {
constexpr double MOUNTAIN_ELEV_M = 2500.0;  // above sea level: bare rock, regardless of climate
constexpr double ALPINE_ELEV_M   = 1500.0;  // above sea level: alpine tundra, regardless of climate
constexpr double TUNDRA_TEMP_C   = -5.0;
constexpr double JUNGLE_TEMP_C   = 20.0;
constexpr double JUNGLE_MOIST    = 0.6;
constexpr double DESERT_TEMP_C   = 20.0;
constexpr double DESERT_MOIST    = 0.3;
constexpr double SWAMP_TEMP_C    = 10.0;
constexpr double SWAMP_MOIST     = 0.8;
constexpr double SWAMP_LOWLAND_ELEV_M = 300.0;  // above sea level: swamps need a low-lying floodplain
constexpr double FOREST_TEMP_C   =  5.0;
constexpr double FOREST_MOIST    = 0.5;

// M236-M275 (MAP16, 2026-07-10) — thresholds for the 32 new biomes reachable
// through classify() (5 ocean-family sub-types by depth/temperature, 27
// land-family sub-types slotted into the original 9-branch cascade's own
// gaps and — where a gap didn't exist without disturbing an existing
// tests/BiomeClassifierTests.cpp assertion — into a deliberately
// *narrower* corner of an existing branch, with that assertion's expected
// value updated and a comment explaining why (see that file). Where an
// existing test's own input values are themselves the most natural example
// of a new, more specific biome (e.g. elevation≈0 + moisture=1.0 IS
// mangrove's own definition, not just "jungle"), the reclassification is
// intentional, not a preservation failure.
//
// All 8 of the original 40 new ZoneType values that classify() alone can't
// reach are now reachable end-to-end (as of 2026-07-11), just never from
// classify() itself -- same "exists but not wired up" state ZoneType::cave
// had for a long time before MAP21 (M331 finally closed that gap -- see
// CAVE_CAVITY_THRESHOLD below):
//
// `canyon`/`tidal_flat`/`sea_cliff` (M259/M274/M275) -- a pure per-cell
// function fundamentally can't compute local slope/roughness or
// distance-to-coast, so they're produced by two new BiomeRefinement
// neighbor-scan passes (applyCanyonCarving()/applyCoastalReliefRefinement())
// that run right after classify()'s own cascade below, same pipeline
// position as applyCoastalBeach()/applySwampFlatnessCheck().
//
// `riparian_forest` (M247) -- needs a distance-to-river signal
// (Hydrology output isn't available inside classify()); reachable via a
// third BiomeRefinement pass, applyRiparianForest(), taking the traced
// HydrologyNetwork directly.
//
// `volcanic`/`geothermal`/`ash_plain`/`volcanic_island` (M265-268) --
// needed an actual volcanism-seeding algorithm (a MountainRanges::
// generate()-shaped generator, not a climate rule): new Map::Volcanism
// class seeds point/radial hotspots; a fourth BiomeRefinement pass,
// applyVolcanicBiomes(), reclassifies land within a hotspot's reach based
// on active/dormant state, distance to center, and coastal adjacency.

// Ocean family (elevation_m < sea_level_m).
constexpr double DEEP_OCEAN_DEPTH_M = 3000.0;  // below this depth: abyssal
constexpr double SHALLOW_DEPTH_M    = 50.0;    // shallow-water band for reef/kelp/lagoon/fjord
constexpr double LAGOON_DEPTH_M     = 8.0;     // shallower still: calm, right off the shore
constexpr double FJORD_TEMP_C       = 2.0;     // cold shallow water, regardless of depth within the band
constexpr double REEF_TEMP_C        = 24.0;    // warm shallow water (deeper than lagoon) -> reef

// Mountain-tier (elev_above_sea >= MOUNTAIN_ELEV_M): coldest peaks split off
// from plain bare-rock mountain. Both thresholds sit below every existing
// MountainPriorityOverTundra/HighElevationIsMountain test's temperature
// (10, -2, -10 °C), so those three are untouched.
constexpr double GLACIER_TEMP_C  = -15.0;
constexpr double ICE_CAP_TEMP_C  = -20.0;

// M331 (MAP21) — cave: a rare, sparse pocket within otherwise-plain
// mountain terrain (not its own broad climate band the way every other new
// biome above is — a deterministic function of climate alone cannot express
// "small localized pockets", which is why this needs the caller-supplied
// cavity_noise score instead). 0.88 is deliberately high (see
// MapBuilder::setBiomeField()'s own doc comment on the Worley-noise scoring
// this is checked against) so caves stay a small minority of mountain-tier
// cells, not a competing climate band. Checked AFTER ice_cap/glacier so
// polar mountain peaks are never reclassified as caves regardless of
// cavity_noise -- caves are a temperate/dry-mountain feature here, not a
// glacial one.
constexpr double CAVE_CAVITY_THRESHOLD = 0.88;

// Elevation bands used below the alpine threshold (0 <= elev_above_sea < 1500).
constexpr double LOWLAND_ELEV_M  = 300.0;  // matches SWAMP_LOWLAND_ELEV_M exactly, intentionally
constexpr double MIDLAND_ELEV_M  = 800.0;
constexpr double HIGHLAND_ELEV_M = 1200.0; // sub-alpine, below ALPINE_ELEV_M's hard 1500 cutoff

// Hot band (temp >= JUNGLE_TEMP_C(20)).
// Jungle-family: cloud_forest carved out by elevation band only. Bounded
// above by HIGHLAND_ELEV_M, not open-ended up to the alpine threshold --
// JustBelowAlpineThresholdUsesNormalClimateRules (elev_above=1499) must
// still resolve to plain jungle, not cloud_forest. Both existing jungle
// tests (elev 1 and 100) are well under MIDLAND_ELEV_M either way.
constexpr double CLOUD_FOREST_ELEV_MIN = MIDLAND_ELEV_M;
constexpr double CLOUD_FOREST_ELEV_MAX = HIGHLAND_ELEV_M;
// Desert-family (moisture < DESERT_MOIST(0.3)): both existing desert tests
// (moisture 0 and 0.2) are deliberately absorbed into the new, more
// specific salt_flat/rocky_desert sub-types below — see the file header
// comment and BiomeClassifierTests.cpp's own updated comments.
constexpr double SALT_FLAT_MOIST_MAX    = 0.05;
constexpr double DUNES_MOIST_MAX        = 0.15;
constexpr double BADLANDS_ELEV_MIN      = LOWLAND_ELEV_M;
constexpr double ROCKY_DESERT_MOIST_MAX = 0.25;
// "Middle ground" (DESERT_MOIST <= moisture < JUNGLE_MOIST, i.e. 0.3-0.6):
// entirely unclaimed by any existing test (desert tests are < 0.3, jungle
// tests are >= 0.6), free to redesign in full.
constexpr double MANGROVE_MOIST_MIN            = 0.5;
constexpr double OASIS_MOIST_MIN               = 0.55;
constexpr double BAMBOO_FOREST_MOIST_MIN       = 0.5;
constexpr double TROPICAL_DRY_FOREST_MOIST_MIN = 0.4;

// Warm band (SWAMP_TEMP_C(10) <= temp < JUNGLE_TEMP_C(20)): existing
// swamp/forest/meadow carve-outs preserved exactly; new biomes claim only
// the moisture < FOREST_MOIST(0.5) gap (previously always meadow at
// elev > SWAMP_LOWLAND_ELEV_M, or meadow/swamp split at or below it).
// Deliberately narrow (< 50 m, not SWAMP_LOWLAND_ELEV_M's own 300): marsh's
// moisture range (0.55-0.8) otherwise overlaps forest's own established
// >= FOREST_MOIST(0.5) territory -- TemperateWetIsForest's cls(100, 12,
// 0.6) must stay forest, and elev_above=100 sits well outside this band.
constexpr double MARSH_ELEV_MAX       = 50.0;
constexpr double MARSH_MOIST_MIN      = 0.55;  // open (non-forested) wetland, below swamp's own 0.8
constexpr double FLOODPLAIN_MOIST_MIN = 0.45;
constexpr double PRAIRIE_MOIST_MIN    = 0.3;
constexpr double SHRUBLAND_MOIST_MAX  = 0.1;
constexpr double CHAPARRAL_MOIST_MAX  = 0.2;

// Mild band (FOREST_TEMP_C(5) <= temp < SWAMP_TEMP_C(10)): existing
// forest/meadow carve-outs preserved exactly (moist >= 0.5 -> forest-
// family, else meadow); new biomes claim only the moist >= 0.5 side.
// 0.85/0.92, not something lower: must stay above the existing forest
// test's own 0.8 moisture value (tests/BiomeClassifierTests.cpp's
// TemperateWetIsForest, cls(1, 5, 0.8)) or that assertion would flip.
constexpr double BOG_MOIST_MIN                 = 0.85;
constexpr double TEMPERATE_RAINFOREST_MOIST_MIN = 0.92;
constexpr double MIXED_FOREST_MOIST_MIN        = 0.65;

// Cool-mid band (TUNDRA_TEMP_C(-5) <= temp < FOREST_TEMP_C(5)): the
// original cascade never produced anything but meadow here (temp < 5 fails
// forest's own threshold, temp < 10 fails swamp's), so entirely free.
constexpr double TAIGA_MOIST       = 0.65;
constexpr double MUSKEG_MOIST_MIN  = 0.45;
constexpr double COLD_DESERT_MOIST = 0.2;
constexpr double PERMAFROST_MOIST  = 0.2;
constexpr double ALPINE_MEADOW_MOIST = 0.55;
} // namespace

MeshWorld::ZoneType BiomeClassifier::classify(double elevation_m,
                                               double temperature_c,
                                               double moisture,
                                               double sea_level_m,
                                               double cavity_noise) {
    using Z = MeshWorld::ZoneType;
    const double elev_above = elevation_m - sea_level_m;

    // ── Ocean family ──────────────────────────────────────────────────
    if (elevation_m < sea_level_m) {
        const double depth = sea_level_m - elevation_m;
        if (depth > DEEP_OCEAN_DEPTH_M) return Z::deep_ocean;
        if (depth <= SHALLOW_DEPTH_M) {
            if (temperature_c < FJORD_TEMP_C) return Z::fjord;
            if (depth <= LAGOON_DEPTH_M) return Z::lagoon;
            if (temperature_c >= REEF_TEMP_C) return Z::coral_reef;
            return Z::kelp_forest;
        }
        return Z::ocean;
    }

    // ── Mountain-tier ─────────────────────────────────────────────────
    if (elev_above >= MOUNTAIN_ELEV_M) {
        if (temperature_c < ICE_CAP_TEMP_C) return Z::ice_cap;
        if (temperature_c < GLACIER_TEMP_C) return Z::glacier;
        if (cavity_noise >= CAVE_CAVITY_THRESHOLD) return Z::cave;
        return Z::mountain;
    }

    // ── Alpine-tier (unchanged, M129) ────────────────────────────────
    if (elev_above >= ALPINE_ELEV_M)
        return Z::tundra;

    // ── Very cold (unchanged threshold; below it is untouched tundra) ──
    if (temperature_c < TUNDRA_TEMP_C)
        return Z::tundra;

    // ── Hot band (temp >= JUNGLE_TEMP_C) ────────────────────────────
    if (temperature_c >= JUNGLE_TEMP_C) {
        if (moisture >= JUNGLE_MOIST) {
            if (elev_above >= CLOUD_FOREST_ELEV_MIN && elev_above < CLOUD_FOREST_ELEV_MAX) return Z::cloud_forest;
            return Z::jungle;
        }
        if (moisture < DESERT_MOIST) {
            if (elev_above >= MIDLAND_ELEV_M) return Z::mesa;
            if (moisture < SALT_FLAT_MOIST_MAX) return Z::salt_flat;
            if (moisture < DUNES_MOIST_MAX) return Z::dunes;
            if (elev_above >= BADLANDS_ELEV_MIN && moisture < ROCKY_DESERT_MOIST_MAX) return Z::badlands;
            if (moisture < ROCKY_DESERT_MOIST_MAX) return Z::rocky_desert;
            return Z::desert;
        }
        // DESERT_MOIST <= moisture < JUNGLE_MOIST (0.3-0.6): free of any
        // existing test (desert tests are all < 0.3, jungle tests >= 0.6).
        if (elev_above < LOWLAND_ELEV_M && moisture >= MANGROVE_MOIST_MIN) return Z::mangrove;
        if (elev_above >= MIDLAND_ELEV_M && moisture >= OASIS_MOIST_MIN) return Z::oasis;
        if (moisture >= BAMBOO_FOREST_MOIST_MIN) return Z::bamboo_forest;
        if (moisture >= TROPICAL_DRY_FOREST_MOIST_MIN) return Z::tropical_dry_forest;
        return Z::savanna;
    }

    // ── Warm band (SWAMP_TEMP_C <= temp < JUNGLE_TEMP_C) ────────────
    if (temperature_c >= SWAMP_TEMP_C) {
        if (moisture >= SWAMP_MOIST && elev_above <= SWAMP_LOWLAND_ELEV_M)
            return Z::swamp;
        if (moisture >= MARSH_MOIST_MIN && moisture < SWAMP_MOIST && elev_above <= MARSH_ELEV_MAX)
            return Z::marsh;
        if (moisture >= FOREST_MOIST)
            return Z::forest;
        // FOREST_MOIST unreachable below this point -- new biomes claim
        // only moisture < FOREST_MOIST, previously always meadow.
        if (moisture >= FLOODPLAIN_MOIST_MIN && elev_above <= SWAMP_LOWLAND_ELEV_M)
            return Z::floodplain;
        if (elev_above >= MIDLAND_ELEV_M) {
            if (moisture < SHRUBLAND_MOIST_MAX) return Z::shrubland;
            if (moisture < CHAPARRAL_MOIST_MAX) return Z::chaparral;
            if (moisture < PRAIRIE_MOIST_MIN) return Z::steppe;
            return Z::prairie;
        }
        return Z::meadow;
    }

    // ── Mild band (FOREST_TEMP_C <= temp < SWAMP_TEMP_C) ────────────
    if (temperature_c >= FOREST_TEMP_C) {
        if (moisture >= FOREST_MOIST) {
            if (elev_above < MIDLAND_ELEV_M && moisture >= TEMPERATE_RAINFOREST_MOIST_MIN)
                return Z::temperate_rainforest;
            if (elev_above < LOWLAND_ELEV_M && moisture >= BOG_MOIST_MIN) return Z::bog;
            if (elev_above >= MIDLAND_ELEV_M && moisture >= MIXED_FOREST_MOIST_MIN) return Z::mixed_forest;
            return Z::forest;
        }
        return Z::meadow;
    }

    // ── Cool-mid band (TUNDRA_TEMP_C <= temp < FOREST_TEMP_C): entirely
    //    new, the original cascade always fell through to meadow here. ──
    if (elev_above >= HIGHLAND_ELEV_M) {
        if (moisture < PERMAFROST_MOIST) return Z::permafrost;
        if (moisture >= ALPINE_MEADOW_MOIST) return Z::alpine_meadow;
        return Z::steppe;
    }
    if (moisture >= TAIGA_MOIST) return Z::taiga;
    if (elev_above < LOWLAND_ELEV_M && moisture >= MUSKEG_MOIST_MIN) return Z::muskeg;
    if (moisture < COLD_DESERT_MOIST) return Z::cold_desert;
    if (elev_above >= MIDLAND_ELEV_M) return Z::steppe;
    return Z::meadow;
}

} // namespace MeshWorld::Map
