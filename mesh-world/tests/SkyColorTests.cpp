// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// Sky/day-night/weather tests. S201/S204: sky_color() keyframe table +
// interpolation.

#include <gtest/gtest.h>

#include "SkyColor.hpp"

using namespace MeshWorld;

namespace {
constexpr float kEps = 1e-5f;
}

TEST(SkyColorTest, MidnightMatchesItsOwnKeyframe) {
    const auto c = sky_color(0.0);
    EXPECT_NEAR(c[0], 0.02f, kEps);
    EXPECT_NEAR(c[1], 0.02f, kEps);
    EXPECT_NEAR(c[2], 0.08f, kEps);
}

TEST(SkyColorTest, DawnMatchesItsOwnKeyframe) {
    const auto c = sky_color(6.0);
    EXPECT_NEAR(c[0], 0.90f, kEps);
    EXPECT_NEAR(c[1], 0.55f, kEps);
    EXPECT_NEAR(c[2], 0.40f, kEps);
}

TEST(SkyColorTest, NoonMatchesItsOwnKeyframe) {
    const auto c = sky_color(12.0);
    EXPECT_NEAR(c[0], 0.45f, kEps);
    EXPECT_NEAR(c[1], 0.65f, kEps);
    EXPECT_NEAR(c[2], 0.95f, kEps);
}

TEST(SkyColorTest, DuskMatchesItsOwnKeyframe) {
    const auto c = sky_color(18.0);
    EXPECT_NEAR(c[0], 0.85f, kEps);
    EXPECT_NEAR(c[1], 0.40f, kEps);
    EXPECT_NEAR(c[2], 0.30f, kEps);
}

TEST(SkyColorTest, WrapsCorrectlyAcross24Hours) {
    const auto at_0  = sky_color(0.0);
    const auto at_24 = sky_color(24.0);
    for (int i = 0; i < 3; ++i) EXPECT_NEAR(at_0[i], at_24[i], kEps) << "channel " << i;
}

TEST(SkyColorTest, NegativeHoursWrapsIntoValidRange) {
    const auto negative = sky_color(-2.0);
    const auto wrapped   = sky_color(22.0);
    for (int i = 0; i < 3; ++i) EXPECT_NEAR(negative[i], wrapped[i], kEps) << "channel " << i;
}

// Regression test for a real user-reported bug (2026-07-11): "why is the
// sky pink?" -- TimeOfDay's own default starting hour (08:00) used to sit
// close enough to dawn's own warm glow (only the original 4 keyframes
// existed then, so dawn->noon interpolated linearly over a full 6h span)
// that the sky rendered as genuinely pink at the very start of a fresh
// world. The fix added keyframes tightly bracketing dawn/dusk so the glow
// fades out within about an hour instead of lingering for several -- pins
// the exact value at 08:00 so this can't silently regress.
TEST(SkyColorTest, DefaultStartingHourIsBlueNotPink) {
    const auto c = sky_color(8.0);
    EXPECT_NEAR(c[0], 0.538889f, 1e-5f);
    EXPECT_NEAR(c[1], 0.65f, 1e-5f);
    EXPECT_NEAR(c[2], 0.861111f, 1e-5f);
    // The real substance of the bug: blue (c[2]) must clearly dominate red
    // (c[0]) by this hour, not the other way around like the old pink
    // result had.
    EXPECT_GT(c[2], c[0]) << "sky at 08:00 must read as blue, not pink/orange";
}

// The 4 new intermediate keyframes tightly bracketing dawn/dusk (matches
// their own exact values).
TEST(SkyColorTest, PreDawnMatchesItsOwnKeyframe) {
    const auto c = sky_color(5.0);
    EXPECT_NEAR(c[0], 0.05f, kEps);
    EXPECT_NEAR(c[1], 0.05f, kEps);
    EXPECT_NEAR(c[2], 0.12f, kEps);
}

TEST(SkyColorTest, MorningMatchesItsOwnKeyframe) {
    const auto c = sky_color(7.5);
    EXPECT_NEAR(c[0], 0.55f, kEps);
    EXPECT_NEAR(c[1], 0.65f, kEps);
    EXPECT_NEAR(c[2], 0.85f, kEps);
}

TEST(SkyColorTest, AfternoonMatchesItsOwnKeyframe) {
    const auto c = sky_color(16.5);
    EXPECT_NEAR(c[0], 0.55f, kEps);
    EXPECT_NEAR(c[1], 0.65f, kEps);
    EXPECT_NEAR(c[2], 0.85f, kEps);
}

TEST(SkyColorTest, EveningMatchesItsOwnKeyframe) {
    const auto c = sky_color(19.5);
    EXPECT_NEAR(c[0], 0.10f, kEps);
    EXPECT_NEAR(c[1], 0.08f, kEps);
    EXPECT_NEAR(c[2], 0.18f, kEps);
}

// Hand-worked interpolation math for the midpoint of 4 representative
// segments (the same 4 sample hours the original 4-keyframe test suite
// used, recomputed for the segments they now fall into with 9 keyframes).
TEST(SkyColorTest, InterpolatesLinearlyBetweenMidnightAndPreDawn) {
    const auto c = sky_color(3.0);  // halfway between 0.0 and 5.0
    EXPECT_NEAR(c[0], 0.038f, kEps);
    EXPECT_NEAR(c[1], 0.038f, kEps);
    EXPECT_NEAR(c[2], 0.104f, kEps);
}

TEST(SkyColorTest, InterpolatesLinearlyBetweenMorningAndNoon) {
    const auto c = sky_color(9.0);  // between 7.5 and 12.0
    EXPECT_NEAR(c[0], 0.516667f, 1e-5f);
    EXPECT_NEAR(c[1], 0.65f, kEps);
    EXPECT_NEAR(c[2], 0.883333f, 1e-5f);
}

TEST(SkyColorTest, InterpolatesLinearlyBetweenNoonAndAfternoon) {
    const auto c = sky_color(15.0);  // between 12.0 and 16.5
    EXPECT_NEAR(c[0], 0.516667f, 1e-5f);
    EXPECT_NEAR(c[1], 0.65f, kEps);
    EXPECT_NEAR(c[2], 0.883333f, 1e-5f);
}

TEST(SkyColorTest, InterpolatesLinearlyBetweenEveningAndMidnight) {
    const auto c = sky_color(21.0);  // between 19.5 and 24.0
    EXPECT_NEAR(c[0], 0.073333f, 1e-5f);
    EXPECT_NEAR(c[1], 0.06f, kEps);
    EXPECT_NEAR(c[2], 0.146667f, 1e-5f);
}

TEST(SkyColorTest, ChannelsAlwaysStayWithinZeroToOneRange) {
    for (double h = 0.0; h < 24.0; h += 0.25) {
        const auto c = sky_color(h);
        for (int i = 0; i < 3; ++i) {
            EXPECT_GE(c[i], 0.0f) << "hours=" << h << " channel " << i;
            EXPECT_LE(c[i], 1.0f) << "hours=" << h << " channel " << i;
        }
    }
}

TEST(SkyColorTest, IsDeterministic) {
    for (double h = 0.0; h < 24.0; h += 1.3) {
        const auto a = sky_color(h);
        const auto b = sky_color(h);
        EXPECT_EQ(a, b) << "hours=" << h;
    }
}
