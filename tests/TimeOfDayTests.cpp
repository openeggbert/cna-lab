// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// Sky/day-night/weather tests. S101/S102/S105: TimeOfDay clock (advancement,
// 24h wraparound, day counter). S106's own wraparound/day-counter test lives
// here as AdvanceWrapsAt24HoursAndIncrementsDay /
// AdvanceAcrossMultipleDayWrapsIncrementsDayCorrectly below.

#include <gtest/gtest.h>

#include "TimeOfDay.hpp"

using namespace MeshWorld;

TEST(TimeOfDayTest, DefaultDayLengthConstantMatchesDocumentedValue) {
    // Pins the documented "24 real minutes per in-game day" default against
    // silent drift -- same "verify a magic number stays pinned" discipline
    // MountainRangesTest's own alpine-threshold test already uses.
    EXPECT_DOUBLE_EQ(TimeOfDay::kDefaultDayLengthRealMinutes, 24.0);
}

TEST(TimeOfDayTest, DefaultConstructionStartsAtMidMorning) {
    TimeOfDay t;
    EXPECT_DOUBLE_EQ(t.hours(), 8.0) << "a fresh world should start in daylight, not midnight";
    EXPECT_EQ(t.day(), 0);
}

TEST(TimeOfDayTest, CustomStartingHourIsRespected) {
    TimeOfDay t(TimeOfDay::kDefaultDayLengthRealMinutes, /*starting_hours=*/14.5);
    EXPECT_DOUBLE_EQ(t.hours(), 14.5);
    EXPECT_EQ(t.day(), 0);
}

TEST(TimeOfDayTest, StartingHourAtOrAbove24WrapsAndIncrementsDay) {
    TimeOfDay t(TimeOfDay::kDefaultDayLengthRealMinutes, /*starting_hours=*/26.0);
    EXPECT_DOUBLE_EQ(t.hours(), 2.0);
    EXPECT_EQ(t.day(), 1);
}

TEST(TimeOfDayTest, NegativeStartingHourWrapsIntoValidRange) {
    TimeOfDay t(TimeOfDay::kDefaultDayLengthRealMinutes, /*starting_hours=*/-2.0);
    EXPECT_DOUBLE_EQ(t.hours(), 22.0);
    EXPECT_GE(t.hours(), 0.0);
    EXPECT_LT(t.hours(), 24.0);
}

TEST(TimeOfDayTest, AdvanceIncreasesHoursProportionally) {
    // day_length=24 real minutes -> 1 in-game hour per 60 real seconds.
    TimeOfDay t(/*day_length_real_minutes=*/24.0, /*starting_hours=*/0.0);
    t.advance(60.0);
    EXPECT_NEAR(t.hours(), 1.0, 1e-9);
    EXPECT_EQ(t.day(), 0);
}

TEST(TimeOfDayTest, AdvanceWrapsAt24HoursAndIncrementsDay) {
    TimeOfDay t(/*day_length_real_minutes=*/24.0, /*starting_hours=*/23.0);
    t.advance(120.0);  // +2 in-game hours -> 25.0, wraps to 1.0
    EXPECT_NEAR(t.hours(), 1.0, 1e-9);
    EXPECT_EQ(t.day(), 1);
}

TEST(TimeOfDayTest, AdvanceAcrossMultipleDayWrapsIncrementsDayCorrectly) {
    // 24 real minutes = 1440 real seconds per in-game day. Advancing by
    // 2.5 in-game days' worth (3600 real seconds = 60 in-game hours) must
    // increment day() by exactly 2 (two full 24h wraps), not once, and not
    // fail to wrap at all.
    TimeOfDay t(/*day_length_real_minutes=*/24.0, /*starting_hours=*/0.0);
    t.advance(3600.0);
    EXPECT_NEAR(t.hours(), 12.0, 1e-9);
    EXPECT_EQ(t.day(), 2);
}

TEST(TimeOfDayTest, ZeroOrNegativeElapsedIsANoOp) {
    TimeOfDay t(TimeOfDay::kDefaultDayLengthRealMinutes, /*starting_hours=*/10.0);
    t.advance(0.0);
    t.advance(-5.0);
    EXPECT_DOUBLE_EQ(t.hours(), 10.0);
    EXPECT_EQ(t.day(), 0);
}

TEST(TimeOfDayTest, NonPositiveDayLengthIsANoOp) {
    TimeOfDay t(/*day_length_real_minutes=*/0.0, /*starting_hours=*/5.0);
    t.advance(1000.0);
    EXPECT_DOUBLE_EQ(t.hours(), 5.0) << "a zero/negative day length has no valid rate -- must not divide by zero";
    EXPECT_EQ(t.day(), 0);
}

TEST(TimeOfDayTest, CustomDayLengthAffectsAdvanceRate) {
    // Half the default day length in "real minutes per in-game day" doubling
    // -- wait, day_length_real_minutes=48 means an in-game day takes TWICE
    // as long in real time, i.e. HALF the hourly rate of the 24-minute
    // default.
    TimeOfDay t(/*day_length_real_minutes=*/48.0, /*starting_hours=*/0.0);
    t.advance(60.0);
    EXPECT_NEAR(t.hours(), 0.5, 1e-9);
}

TEST(TimeOfDayTest, RepeatedSmallAdvancesAccumulateTheSameAsOneLargeAdvance) {
    // A per-frame advance() loop must accumulate identically to one
    // equivalent large advance -- proves there's no truncation/rounding bug
    // from calling advance() many times with small deltas (the real usage
    // pattern, once per frame).
    TimeOfDay a(/*day_length_real_minutes=*/24.0, /*starting_hours=*/0.0);
    for (int i = 0; i < 100; ++i) a.advance(6.0);  // 100 * 6.0 = 600 real seconds

    TimeOfDay b(/*day_length_real_minutes=*/24.0, /*starting_hours=*/0.0);
    b.advance(600.0);

    EXPECT_NEAR(a.hours(), b.hours(), 1e-6);
    EXPECT_EQ(a.day(), b.day());
}
