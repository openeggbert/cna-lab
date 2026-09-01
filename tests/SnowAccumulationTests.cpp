// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// Sky/day-night/weather tests. S1001-S1005: snow accumulation on outdoor
// models (accumulate/melt state machine).

#include <gtest/gtest.h>

#include "SnowAccumulation.hpp"

using namespace MeshWorld;

TEST(SnowAccumulationTest, StartsAtZeroDepth) {
    SnowAccumulation snow;
    EXPECT_FLOAT_EQ(snow.depth(), 0.0f);
}

TEST(SnowAccumulationTest, ZeroOrNegativeElapsedIsANoOp) {
    SnowAccumulation snow;
    snow.advance(0.0, WeatherState::Snow, -10.0);
    snow.advance(-5.0, WeatherState::Snow, -10.0);
    EXPECT_FLOAT_EQ(snow.depth(), 0.0f);
}

TEST(SnowAccumulationTest, NonPositiveDayLengthIsANoOp) {
    SnowAccumulation snow;
    snow.advance(100000.0, WeatherState::Snow, -10.0, /*day_length_real_minutes=*/0.0);
    EXPECT_FLOAT_EQ(snow.depth(), 0.0f) << "a zero/negative day length has no valid rate -- must not divide by zero";
}

TEST(SnowAccumulationTest, AccumulatesWhileActivelySnowingAndFreezing) {
    SnowAccumulation snow;
    // 24 real minutes/day -> 60 real seconds/in-game hour; 2 in-game hours
    // of continuous snow at kAccumulationRatePerHour=1/4 per hour -> 0.5.
    snow.advance(2.0 * 60.0, WeatherState::Snow, /*temperature_c=*/-5.0, /*day_length_real_minutes=*/24.0);
    EXPECT_NEAR(snow.depth(), 0.5f, 1e-4f);
}

TEST(SnowAccumulationTest, ReachesFullAccumulationAndClamps) {
    SnowAccumulation snow;
    // Comfortably more than the 4h needed to reach full accumulation.
    snow.advance(20.0 * 60.0, WeatherState::Snow, -5.0, 24.0);
    EXPECT_FLOAT_EQ(snow.depth(), 1.0f);
}

TEST(SnowAccumulationTest, DoesNotAccumulateWhenSnowingButAboveFreezing) {
    SnowAccumulation snow;
    snow.advance(20.0 * 60.0, WeatherState::Snow, /*temperature_c=*/5.0, 24.0);
    EXPECT_FLOAT_EQ(snow.depth(), 0.0f);
}

TEST(SnowAccumulationTest, DoesNotAccumulateForOtherWeatherStatesEvenIfFreezing) {
    for (WeatherState s : {WeatherState::Clear, WeatherState::PartlyCloudy, WeatherState::Overcast,
                            WeatherState::Rain}) {
        SnowAccumulation snow;
        snow.advance(20.0 * 60.0, s, /*temperature_c=*/-10.0, 24.0);
        EXPECT_FLOAT_EQ(snow.depth(), 0.0f) << "weather=" << static_cast<int>(s);
    }
}

TEST(SnowAccumulationTest, MeltsWhenTemperatureRisesAboveFreezing) {
    SnowAccumulation snow;
    snow.advance(20.0 * 60.0, WeatherState::Snow, -5.0, 24.0);  // fully accumulate
    ASSERT_FLOAT_EQ(snow.depth(), 1.0f);

    // 3 in-game hours at kMeltRatePerHour=1/6 per hour -> -0.5.
    snow.advance(3.0 * 60.0, WeatherState::Clear, /*temperature_c=*/5.0, 24.0);
    EXPECT_NEAR(snow.depth(), 0.5f, 1e-4f);
}

TEST(SnowAccumulationTest, MeltsAfterLongEnoughWithoutFreshSnowfallEvenIfStillFreezing) {
    // S1003's own literal "OR enough time passes without fresh snowfall" --
    // still below freezing the whole time, but no active snowfall for
    // longer than kNoFreshSnowMeltDelayHours.
    SnowAccumulation snow;
    snow.advance(20.0 * 60.0, WeatherState::Snow, -5.0, 24.0);  // fully accumulate
    ASSERT_FLOAT_EQ(snow.depth(), 1.0f);

    // 13 in-game hours of Clear (still below freezing) -- past the 12h
    // no-fresh-snow melt delay, so melting should have started.
    snow.advance(13.0 * 60.0, WeatherState::Clear, /*temperature_c=*/-5.0, 24.0);
    EXPECT_LT(snow.depth(), 1.0f);
}

TEST(SnowAccumulationTest, DoesNotMeltDuringABriefLullStillBelowFreezing) {
    // A short gap (well under the 12h delay) between snow spells, still
    // freezing, must NOT start melting yet.
    SnowAccumulation snow;
    snow.advance(20.0 * 60.0, WeatherState::Snow, -5.0, 24.0);  // fully accumulate
    ASSERT_FLOAT_EQ(snow.depth(), 1.0f);

    snow.advance(1.0 * 60.0, WeatherState::Clear, /*temperature_c=*/-5.0, 24.0);
    EXPECT_FLOAT_EQ(snow.depth(), 1.0f);
}

TEST(SnowAccumulationTest, MeltNeverGoesNegative) {
    SnowAccumulation snow;
    snow.advance(2.0 * 60.0, WeatherState::Snow, -5.0, 24.0);
    ASSERT_GT(snow.depth(), 0.0f);
    snow.advance(1000.0 * 60.0, WeatherState::Clear, /*temperature_c=*/20.0, 24.0);
    EXPECT_FLOAT_EQ(snow.depth(), 0.0f);
}

TEST(SnowAccumulationTest, FreshSnowfallResumingResetsTheNoSnowClock) {
    // Accumulate, let it partially melt from time-without-snow, then resume
    // snowing -- must accumulate again, not stay stuck melting.
    SnowAccumulation snow;
    snow.advance(20.0 * 60.0, WeatherState::Snow, -5.0, 24.0);
    ASSERT_FLOAT_EQ(snow.depth(), 1.0f);

    snow.advance(13.0 * 60.0, WeatherState::Clear, -5.0, 24.0);  // starts melting (time-based)
    const float melted_depth = snow.depth();
    ASSERT_LT(melted_depth, 1.0f);

    snow.advance(20.0 * 60.0, WeatherState::Snow, -5.0, 24.0);  // snowing resumes
    EXPECT_FLOAT_EQ(snow.depth(), 1.0f);
}
