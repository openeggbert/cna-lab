// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M235 (MAP16, 2026-07-10) — ZoneType grew from 12 to 52 values. This is a
// pure mechanical-correctness test for that expansion: every ordinal has a
// name, round-trips through to_string()/zone_from_string(), has a distinct
// color and a resolvable ascii char, and the fixed-size arrays that key off
// ZoneType ordinals (ZONE_NAMES, zone_rgb_color()'s/zone_ascii_char()'s own
// tables) all agree on exactly 52 entries. Per-biome classification logic
// (M236-M275) is out of scope here — that's each new value's own test.

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <set>

#include "PlanetMapLogic.hpp"
#include "WorldMap.hpp"
#include "ZoneType.hpp"

using namespace MeshWorld;

namespace {

// Every ZoneType value, in enum-declaration order — must be kept in sync
// with include/ZoneType.hpp by hand (there's no reflection in C++). A
// missing/extra entry here would itself be a real bug this test should
// catch (via the size checks below), not just a maintenance annoyance.
constexpr std::array<ZoneType, 52> kAllZones = {
    ZoneType::city, ZoneType::jungle, ZoneType::desert, ZoneType::forest,
    ZoneType::ocean, ZoneType::mountain, ZoneType::tundra, ZoneType::swamp,
    ZoneType::cave, ZoneType::meadow, ZoneType::beach,

    ZoneType::savanna, ZoneType::steppe, ZoneType::prairie, ZoneType::chaparral,
    ZoneType::shrubland,

    ZoneType::taiga, ZoneType::temperate_rainforest, ZoneType::mixed_forest,
    ZoneType::cloud_forest, ZoneType::mangrove, ZoneType::bamboo_forest,
    ZoneType::riparian_forest, ZoneType::tropical_dry_forest,

    ZoneType::marsh, ZoneType::floodplain, ZoneType::bog, ZoneType::muskeg,

    ZoneType::dunes, ZoneType::rocky_desert, ZoneType::cold_desert,
    ZoneType::salt_flat, ZoneType::badlands, ZoneType::mesa, ZoneType::canyon,
    ZoneType::oasis,

    ZoneType::glacier, ZoneType::permafrost, ZoneType::alpine_meadow,
    ZoneType::ice_cap,

    ZoneType::volcanic, ZoneType::geothermal, ZoneType::ash_plain,
    ZoneType::volcanic_island,

    ZoneType::coral_reef, ZoneType::kelp_forest, ZoneType::deep_ocean,
    ZoneType::lagoon, ZoneType::fjord, ZoneType::tidal_flat, ZoneType::sea_cliff,

    ZoneType::empty,
};

} // namespace

TEST(ZoneTypeTest, ExactlyFiftyTwoValues) {
    EXPECT_EQ(kAllZones.size(), 52u);
    EXPECT_EQ(ZONE_NAMES.size(), 52u);
}

TEST(ZoneTypeTest, EmptyIsStillTheLastOrdinal) {
    // MapValidator's max_valid sentinel (src/MapValidator.cpp) is
    // static_cast<uint8_t>(ZoneType::empty) -- this must never regress.
    EXPECT_EQ(static_cast<int>(kAllZones.back()), 51);
    EXPECT_EQ(kAllZones.back(), ZoneType::empty);
}

TEST(ZoneTypeTest, ToStringRoundTripsThroughZoneFromString) {
    for (const ZoneType z : kAllZones) {
        const std::string name = to_string(z);
        EXPECT_FALSE(name.empty());
        EXPECT_EQ(zone_from_string(name), z) << "round-trip failed for '" << name << "'";
    }
}

TEST(ZoneTypeTest, AllNamesAreUnique) {
    std::set<std::string> names;
    for (const ZoneType z : kAllZones) names.insert(to_string(z));
    EXPECT_EQ(names.size(), kAllZones.size()) << "at least one to_string() collision";
}

TEST(ZoneTypeTest, ZoneFromStringThrowsOnUnknownName) {
    EXPECT_THROW(zone_from_string("not_a_real_zone"), std::invalid_argument);
}

TEST(ZoneTypeTest, ZoneNamesArrayMatchesToStringByOrdinal) {
    for (std::size_t i = 0; i < kAllZones.size(); ++i) {
        EXPECT_EQ(std::string(ZONE_NAMES[i]), to_string(kAllZones[i]))
            << "ZONE_NAMES[" << i << "] disagrees with to_string() for the same ordinal";
    }
}

TEST(ZoneTypeTest, EveryOrdinalHasAResolvableRgbColor) {
    // zone_rgb_color() falls back to magenta {255,0,255} for an out-of-range
    // ordinal -- every real ordinal in [0,52) must resolve to something else.
    for (int i = 0; i < static_cast<int>(kAllZones.size()); ++i) {
        const auto color = zone_rgb_color(i);
        const bool is_fallback_magenta = (color[0] == 255 && color[1] == 0 && color[2] == 255);
        EXPECT_FALSE(is_fallback_magenta) << "ordinal " << i << " (" << to_string(kAllZones[static_cast<std::size_t>(i)])
                                           << ") resolved to the unknown-ordinal fallback color";
    }
}

TEST(ZoneTypeTest, AllRgbColorsAreDistinct) {
    std::set<std::array<std::uint8_t, 3>> colors;
    for (int i = 0; i < static_cast<int>(kAllZones.size()); ++i) colors.insert(zone_rgb_color(i));
    EXPECT_EQ(colors.size(), kAllZones.size()) << "at least two ZoneType values share an RGB color";
}

TEST(ZoneTypeTest, EveryOrdinalHasAResolvableAsciiChar) {
    for (int i = 0; i < static_cast<int>(kAllZones.size()); ++i) {
        EXPECT_NE(zone_ascii_char(i), '?')
            << "ordinal " << i << " (" << to_string(kAllZones[static_cast<std::size_t>(i)])
            << ") resolved to the unknown-ordinal fallback char";
    }
}

TEST(ZoneTypeTest, AllAsciiCharsAreDistinct) {
    std::set<char> chars;
    for (int i = 0; i < static_cast<int>(kAllZones.size()); ++i) chars.insert(zone_ascii_char(i));
    EXPECT_EQ(chars.size(), kAllZones.size()) << "at least two ZoneType values share an ascii char";
}

TEST(ZoneTypeTest, WorldMapZoneColorHandlesEveryValueWithoutTheDeadDefault) {
    // WorldMap::zone_color() is an exhaustive switch (enforced by
    // -Werror=switch at compile time) with a {0,0,0} fallback after it that
    // should be genuinely unreachable for any real ZoneType value.
    for (const ZoneType z : kAllZones) {
        const auto color = WorldMap::zone_color(z);
        const bool is_dead_default = (color[0] == 0.f && color[1] == 0.f && color[2] == 0.f);
        if (z != ZoneType::empty) {  // empty legitimately isn't near-black
            EXPECT_FALSE(is_dead_default) << to_string(z) << " hit WorldMap::zone_color()'s dead default";
        }
    }
}
