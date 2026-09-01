// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// Sky/day-night/weather tests. S103: sun position from time-of-day. S104:
// moon position as the sun's antipode. S402: moon_phase_fraction().

#include <gtest/gtest.h>

#include "CelestialPosition.hpp"

using namespace MeshWorld;

TEST(CelestialPositionTest, SunAtSunriseIsOnTheEasternHorizon) {
    const SkyAngle sun = sun_position(6.0);
    EXPECT_NEAR(sun.elevation_deg, 0.0, 1e-9);
    EXPECT_NEAR(sun.azimuth_deg, 90.0, 1e-9);
}

TEST(CelestialPositionTest, SunAtNoonIsAtZenithFacingSouth) {
    const SkyAngle sun = sun_position(12.0);
    EXPECT_NEAR(sun.elevation_deg, 90.0, 1e-9);
    EXPECT_NEAR(sun.azimuth_deg, 180.0, 1e-9);
}

TEST(CelestialPositionTest, SunAtSunsetIsOnTheWesternHorizon) {
    const SkyAngle sun = sun_position(18.0);
    EXPECT_NEAR(sun.elevation_deg, 0.0, 1e-9);
    EXPECT_NEAR(sun.azimuth_deg, 270.0, 1e-9);
}

TEST(CelestialPositionTest, SunAtMidnightIsAtNadirFacingNorth) {
    const SkyAngle sun = sun_position(0.0);
    EXPECT_NEAR(sun.elevation_deg, -90.0, 1e-9);
    EXPECT_NEAR(sun.azimuth_deg, 0.0, 1e-9);
}

TEST(CelestialPositionTest, SunPositionIsPeriodicAcross24Hours) {
    const SkyAngle at_0  = sun_position(0.0);
    const SkyAngle at_24 = sun_position(24.0);
    EXPECT_NEAR(at_0.elevation_deg, at_24.elevation_deg, 1e-9);
    EXPECT_NEAR(at_0.azimuth_deg, at_24.azimuth_deg, 1e-9);
}

TEST(CelestialPositionTest, SunElevationStaysWithinPlusMinus90Degrees) {
    for (double h = 0.0; h < 24.0; h += 0.5) {
        const SkyAngle sun = sun_position(h);
        EXPECT_GE(sun.elevation_deg, -90.0) << "hours=" << h;
        EXPECT_LE(sun.elevation_deg, 90.0) << "hours=" << h;
    }
}

TEST(CelestialPositionTest, SunAzimuthAlwaysInZeroTo360Range) {
    for (double h = 0.0; h < 24.0; h += 0.5) {
        const SkyAngle sun = sun_position(h);
        EXPECT_GE(sun.azimuth_deg, 0.0) << "hours=" << h;
        EXPECT_LT(sun.azimuth_deg, 360.0) << "hours=" << h;
    }
}

// --- S104: moon_position() ---

TEST(CelestialPositionTest, MoonIsAtZenithWhenSunIsAtNadir) {
    // Midnight: sun is at its lowest point, so the moon (its antipode) is
    // directly overhead -- matches a full moon's real behavior.
    const SkyAngle moon = moon_position(0.0);
    EXPECT_NEAR(moon.elevation_deg, 90.0, 1e-9);
    EXPECT_NEAR(moon.azimuth_deg, 180.0, 1e-9);
}

TEST(CelestialPositionTest, MoonIsAtNadirWhenSunIsAtZenith) {
    // Noon: sun is directly overhead, so the moon is on the far side of the
    // world, not visible.
    const SkyAngle moon = moon_position(12.0);
    EXPECT_NEAR(moon.elevation_deg, -90.0, 1e-9);
    EXPECT_NEAR(moon.azimuth_deg, 0.0, 1e-9);
}

TEST(CelestialPositionTest, MoonElevationIsAlwaysTheNegationOfSunElevation) {
    for (double h = 0.0; h < 24.0; h += 0.5) {
        const SkyAngle sun  = sun_position(h);
        const SkyAngle moon = moon_position(h);
        EXPECT_NEAR(moon.elevation_deg, -sun.elevation_deg, 1e-9) << "hours=" << h;
    }
}

TEST(CelestialPositionTest, MoonAzimuthIsAlways180DegreesFromSunAzimuth) {
    for (double h = 0.0; h < 24.0; h += 0.5) {
        const SkyAngle sun  = sun_position(h);
        const SkyAngle moon = moon_position(h);
        double         diff = moon.azimuth_deg - sun.azimuth_deg;
        while (diff < 0.0) diff += 360.0;
        while (diff >= 360.0) diff -= 360.0;
        EXPECT_NEAR(diff, 180.0, 1e-9) << "hours=" << h;
    }
}

TEST(CelestialPositionTest, MoonAzimuthAlwaysInZeroTo360Range) {
    for (double h = 0.0; h < 24.0; h += 0.5) {
        const SkyAngle moon = moon_position(h);
        EXPECT_GE(moon.azimuth_deg, 0.0) << "hours=" << h;
        EXPECT_LT(moon.azimuth_deg, 360.0) << "hours=" << h;
    }
}

TEST(CelestialPositionTest, MoonIsAboveHorizonRoughlyWhenSunIsBelowIt) {
    // "Visible roughly when the sun is below the horizon" (S104's own
    // wording) -- not an exact claim (both cross the horizon at the same
    // two instants, 06:00/18:00), but everywhere else sun-up implies
    // moon-down and vice versa, since they're exact antipodes.
    int agreement = 0, total = 0;
    for (double h = 0.25; h < 24.0; h += 0.5) {  // avoid the exact crossing points
        const SkyAngle sun  = sun_position(h);
        const SkyAngle moon = moon_position(h);
        ++total;
        if ((sun.elevation_deg > 0.0) != (moon.elevation_deg > 0.0)) ++agreement;
    }
    EXPECT_EQ(agreement, total) << "sun-above/moon-below (or vice versa) should hold at every sampled hour";
}

// --- S402: moon_phase_fraction() ---

TEST(CelestialPositionTest, PhaseAtDayZeroHourZeroIsNewMoon) {
    EXPECT_NEAR(moon_phase_fraction(/*day=*/0, /*hours=*/0.0, /*lunar_cycle_days=*/8.0), 0.0, 1e-9);
}

TEST(CelestialPositionTest, PhaseAtHalfTheCycleIsFullMoon) {
    // 8-day default cycle -> day 4 is exactly the halfway point.
    EXPECT_NEAR(moon_phase_fraction(/*day=*/4, /*hours=*/0.0, /*lunar_cycle_days=*/8.0), 0.5, 1e-9);
}

TEST(CelestialPositionTest, PhaseAtOneFullCycleWrapsBackToNewMoon) {
    EXPECT_NEAR(moon_phase_fraction(/*day=*/8, /*hours=*/0.0, /*lunar_cycle_days=*/8.0), 0.0, 1e-9);
    // Also holds for multiple full cycles.
    EXPECT_NEAR(moon_phase_fraction(/*day=*/24, /*hours=*/0.0, /*lunar_cycle_days=*/8.0), 0.0, 1e-9);
}

TEST(CelestialPositionTest, PhaseAdvancesSmoothlyWithinASingleDay) {
    // Half a day into an 8-day cycle: 0.5/8 = 0.0625, not a jump straight
    // to day 1's own 1/8 = 0.125 -- proves hours contributes continuously,
    // not just the integer day counter.
    EXPECT_NEAR(moon_phase_fraction(/*day=*/0, /*hours=*/12.0, /*lunar_cycle_days=*/8.0), 0.0625, 1e-9);
}

TEST(CelestialPositionTest, PhaseWithADifferentCycleLengthScalesAccordingly) {
    // A 4-day cycle: day 2 is the halfway (full moon) point, matching the
    // same "half the cycle length" relationship as the 8-day default.
    EXPECT_NEAR(moon_phase_fraction(/*day=*/2, /*hours=*/0.0, /*lunar_cycle_days=*/4.0), 0.5, 1e-9);
}

TEST(CelestialPositionTest, PhaseNonPositiveCycleLengthIsANoOp) {
    EXPECT_DOUBLE_EQ(moon_phase_fraction(/*day=*/3, /*hours=*/6.0, /*lunar_cycle_days=*/0.0), 0.0);
    EXPECT_DOUBLE_EQ(moon_phase_fraction(/*day=*/3, /*hours=*/6.0, /*lunar_cycle_days=*/-5.0), 0.0);
}

TEST(CelestialPositionTest, PhaseAlwaysStaysWithinZeroToOneRange) {
    for (int day = 0; day < 20; ++day) {
        for (double h = 0.0; h < 24.0; h += 3.0) {
            const double phase = moon_phase_fraction(day, h, 8.0);
            EXPECT_GE(phase, 0.0) << "day=" << day << " hours=" << h;
            EXPECT_LT(phase, 1.0) << "day=" << day << " hours=" << h;
        }
    }
}

// ── S501/S504: star field ───────────────────────────────────────────────

TEST(CelestialPositionTest, StarFieldHasRequestedCountWhenUnderTheCap) {
    EXPECT_EQ(generate_star_field(/*seed=*/1, /*count=*/100).size(), 100u);
    EXPECT_EQ(generate_star_field(/*seed=*/1, /*count=*/0).size(), 0u);
}

TEST(CelestialPositionTest, StarFieldCountNeverExceedsTheConfiguredCap) {
    EXPECT_EQ(generate_star_field(/*seed=*/1, /*count=*/5000, /*max_count=*/800).size(), 800u);
    EXPECT_EQ(generate_star_field(/*seed=*/1, /*count=*/900, /*max_count=*/500).size(), 500u);
}

TEST(CelestialPositionTest, StarFieldNegativeCountClampsToZero) {
    EXPECT_EQ(generate_star_field(/*seed=*/1, /*count=*/-10).size(), 0u);
}

TEST(CelestialPositionTest, StarFieldIsByteIdenticalAcrossTwoLoadsOfTheSameSeed) {
    const auto a = generate_star_field(/*seed=*/12345, /*count=*/300);
    const auto b = generate_star_field(/*seed=*/12345, /*count=*/300);
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        EXPECT_DOUBLE_EQ(a[i].elevation_deg, b[i].elevation_deg) << "i=" << i;
        EXPECT_DOUBLE_EQ(a[i].azimuth_deg, b[i].azimuth_deg) << "i=" << i;
    }
}

TEST(CelestialPositionTest, StarFieldDiffersAcrossDifferentSeeds) {
    const auto a = generate_star_field(/*seed=*/1, /*count=*/50);
    const auto b = generate_star_field(/*seed=*/2, /*count=*/50);
    bool any_different = false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].elevation_deg != b[i].elevation_deg || a[i].azimuth_deg != b[i].azimuth_deg) {
            any_different = true;
            break;
        }
    }
    EXPECT_TRUE(any_different);
}

TEST(CelestialPositionTest, StarFieldPositionsStayWithinValidRanges) {
    const auto stars = generate_star_field(/*seed=*/42, /*count=*/500);
    for (const SkyAngle& star : stars) {
        EXPECT_GE(star.elevation_deg, -90.0);
        EXPECT_LE(star.elevation_deg, 90.0);
        EXPECT_GE(star.azimuth_deg, 0.0);
        EXPECT_LT(star.azimuth_deg, 360.0);
    }
}

TEST(CelestialPositionTest, StarFieldIsAPrefixOfALargerRequestFromTheSameSeed) {
    // S501's own "not a reshuffle every frame" -- growing the requested
    // count shouldn't reshuffle the stars already generated, only append.
    const auto small = generate_star_field(/*seed=*/7, /*count=*/20);
    const auto large = generate_star_field(/*seed=*/7, /*count=*/40);
    ASSERT_GE(large.size(), small.size());
    for (std::size_t i = 0; i < small.size(); ++i) {
        EXPECT_DOUBLE_EQ(small[i].elevation_deg, large[i].elevation_deg) << "i=" << i;
        EXPECT_DOUBLE_EQ(small[i].azimuth_deg, large[i].azimuth_deg) << "i=" << i;
    }
}
