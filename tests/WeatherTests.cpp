// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// Sky/day-night/weather tests. S601-S604: Weather state machine (transition
// timer, crossfade, temperature-gated snow/rain selection).

#include <gtest/gtest.h>

#include "Weather.hpp"

using namespace MeshWorld;

TEST(WeatherTest, DefaultConstructionStartsClearAndFullySettled) {
    Weather w(/*seed=*/1);
    EXPECT_EQ(w.state(), WeatherState::Clear);
    EXPECT_EQ(w.previous_state(), WeatherState::Clear);
    EXPECT_FLOAT_EQ(w.transition_progress(), 1.0f);
}

TEST(WeatherTest, ZeroOrNegativeElapsedIsANoOp) {
    Weather w(/*seed=*/1);
    w.advance(0.0, /*temperature_c=*/10.0);
    w.advance(-5.0, /*temperature_c=*/10.0);
    EXPECT_EQ(w.state(), WeatherState::Clear);
    EXPECT_FLOAT_EQ(w.transition_progress(), 1.0f);
}

TEST(WeatherTest, NonPositiveDayLengthIsANoOp) {
    Weather w(/*seed=*/1, /*day_length_real_minutes=*/0.0);
    w.advance(100000.0, /*temperature_c=*/10.0);
    EXPECT_EQ(w.state(), WeatherState::Clear) << "a zero/negative day length has no valid rate -- must not divide by zero";
}

TEST(WeatherTest, LongAdvanceCompletesATransitionAndItsCrossfade) {
    // 24 real minutes/day -> 60 real seconds/in-game hour; 20 in-game hours
    // is comfortably past both the max 8h transition window and the 0.5h
    // crossfade duration, in a single call.
    Weather w(/*seed=*/42, /*day_length_real_minutes=*/24.0);
    w.advance(/*elapsed_real_seconds=*/20.0 * 60.0, /*temperature_c=*/15.0);
    EXPECT_FLOAT_EQ(w.transition_progress(), 1.0f);
}

TEST(WeatherTest, TransitionResetsProgressNearZeroThenReachesOneAfterCrossfadeDuration) {
    Weather w(/*seed=*/42, /*day_length_real_minutes=*/24.0);

    // Step in small increments (1 real second = 1/60 in-game hour at this
    // day length -- far smaller than kCrossfadeHours=0.5h) until a GENUINE
    // state change is observed -- pick_next_state() can legitimately reroll
    // the same state it's already in, which reads as fully settled instead
    // (see transition_progress()'s own doc comment), so keep stepping
    // rather than assuming the first transition changes anything. Small
    // steps also keep any "overshoot" past the transition boundary tiny,
    // so progress right after the change is still close to 0.
    bool changed = false;
    for (int i = 0; i < 1000 && !changed; ++i) {
        w.advance(/*elapsed_real_seconds=*/1.0, /*temperature_c=*/15.0);
        changed = (w.previous_state() != w.state());
    }
    ASSERT_TRUE(changed) << "never observed a genuine state change in 1000 one-second steps "
                            "(well past the max 8h initial transition window)";
    EXPECT_LT(w.transition_progress(), 0.1f)
        << "overshoot from a single 1-second step should be a small fraction of the 0.5h crossfade";

    // Continue stepping until the crossfade completes -- must happen well
    // within kCrossfadeHours (0.5h = 30 real seconds at this day length) of
    // further real time; 120 one-second steps is a generous margin.
    bool reached_full = false;
    for (int i = 0; i < 120 && !reached_full; ++i) {
        w.advance(1.0, /*temperature_c=*/15.0);
        if (w.transition_progress() >= 1.0f) reached_full = true;
    }
    EXPECT_TRUE(reached_full) << "crossfade never completed within a generous margin past its own duration";
}

TEST(WeatherTest, NeverSelectsSnowAboveFreezing) {
    // S604 -- a synthetic warm-biome scenario: drive many transitions at a
    // temperature well above freezing and confirm Snow never appears.
    Weather w(/*seed=*/123, /*day_length_real_minutes=*/24.0);
    for (int i = 0; i < 500; ++i) {
        w.advance(/*elapsed_real_seconds=*/9.0 * 60.0, /*temperature_c=*/20.0);
        EXPECT_NE(w.state(), WeatherState::Snow) << "iteration " << i;
    }
}

TEST(WeatherTest, NeverSelectsRainAtOrBelowFreezing) {
    // The mirror image of S604's own requirement: a synthetic cold-biome
    // scenario must never select Rain (Snow takes its place).
    Weather w(/*seed=*/456, /*day_length_real_minutes=*/24.0);
    for (int i = 0; i < 500; ++i) {
        w.advance(/*elapsed_real_seconds=*/9.0 * 60.0, /*temperature_c=*/-10.0);
        EXPECT_NE(w.state(), WeatherState::Rain) << "iteration " << i;
    }
}

TEST(WeatherTest, ExactlyFreezingCountsAsColdForSnowSelection) {
    // kFreezingC's own boundary is inclusive ("at/below freezing").
    Weather w(/*seed=*/789, /*day_length_real_minutes=*/24.0);
    for (int i = 0; i < 500; ++i) {
        w.advance(/*elapsed_real_seconds=*/9.0 * 60.0, /*temperature_c=*/Weather::kFreezingC);
        EXPECT_NE(w.state(), WeatherState::Rain) << "iteration " << i;
    }
}

TEST(WeatherTest, WarmScenarioCanStillReachEveryNonSnowState) {
    // Not just "never snow" -- confirms the warm pool is actually reachable
    // in full (Clear/PartlyCloudy/Overcast/Rain all occur over a long run),
    // so NeverSelectsSnowAboveFreezing isn't vacuously true from a pool
    // that's accidentally empty/broken.
    Weather w(/*seed=*/321, /*day_length_real_minutes=*/24.0);
    bool saw_clear = false, saw_partly = false, saw_overcast = false, saw_rain = false;
    for (int i = 0; i < 2000; ++i) {
        w.advance(/*elapsed_real_seconds=*/9.0 * 60.0, /*temperature_c=*/20.0);
        switch (w.state()) {
            case WeatherState::Clear: saw_clear = true; break;
            case WeatherState::PartlyCloudy: saw_partly = true; break;
            case WeatherState::Overcast: saw_overcast = true; break;
            case WeatherState::Rain: saw_rain = true; break;
            case WeatherState::Snow: FAIL() << "Snow selected above freezing at iteration " << i; break;
        }
    }
    EXPECT_TRUE(saw_clear);
    EXPECT_TRUE(saw_partly);
    EXPECT_TRUE(saw_overcast);
    EXPECT_TRUE(saw_rain);
}

TEST(WeatherTest, RepeatedSmallAdvancesAccumulateSimilarlyToOneLargeAdvance) {
    // Same "no truncation from small per-frame steps" property
    // TimeOfDayTest's own RepeatedSmallAdvancesAccumulateTheSameAsOneLargeAdvance
    // checks -- can't assert bit-identical state here (rng_ draws differ
    // between many-small-steps and one-large-step, both legitimately valid),
    // but the crossfade timer itself must not drift: after the exact same
    // total elapsed time and temperature, both must be fully settled
    // (transition_progress 1.0) once well past both the max transition
    // window and the crossfade duration.
    Weather a(/*seed=*/99, /*day_length_real_minutes=*/24.0);
    for (int i = 0; i < 100; ++i) a.advance(6.0, /*temperature_c=*/20.0);  // 100*6 = 600s = 10 in-game h

    Weather b(/*seed=*/99, /*day_length_real_minutes=*/24.0);
    b.advance(600.0, /*temperature_c=*/20.0);

    EXPECT_FLOAT_EQ(a.transition_progress(), 1.0f);
    EXPECT_FLOAT_EQ(b.transition_progress(), 1.0f);
}

TEST(WeatherTest, SameSeedProducesTheSameSequenceOfStates) {
    // Not a determinism REQUIREMENT for weather the way S501's star field
    // has one (see Weather.hpp's own doc comment on why) -- but the
    // underlying RNG must still be genuinely deterministic given the same
    // seed and the same call sequence, or tests like NeverSelectsSnow
    // above/reproducing a specific bug report would be unreliable.
    Weather a(/*seed=*/2024, /*day_length_real_minutes=*/24.0);
    Weather b(/*seed=*/2024, /*day_length_real_minutes=*/24.0);
    for (int i = 0; i < 50; ++i) {
        a.advance(30.0, /*temperature_c=*/5.0);
        b.advance(30.0, /*temperature_c=*/5.0);
        ASSERT_EQ(a.state(), b.state()) << "iteration " << i;
        ASSERT_EQ(a.previous_state(), b.previous_state()) << "iteration " << i;
        ASSERT_FLOAT_EQ(a.transition_progress(), b.transition_progress()) << "iteration " << i;
    }
}

TEST(WeatherTest, DifferentSeedsCanProduceDifferentSequences) {
    Weather a(/*seed=*/1, /*day_length_real_minutes=*/24.0);
    Weather b(/*seed=*/2, /*day_length_real_minutes=*/24.0);
    bool any_different = false;
    for (int i = 0; i < 200; ++i) {
        a.advance(30.0, /*temperature_c=*/5.0);
        b.advance(30.0, /*temperature_c=*/5.0);
        if (a.state() != b.state()) { any_different = true; break; }
    }
    EXPECT_TRUE(any_different);
}

// --- S901: wind (direction + strength, paired with the transition timer) --

TEST(WeatherTest, DefaultWindIsCalm) {
    Weather w(/*seed=*/1);
    EXPECT_FLOAT_EQ(w.wind().strength, 0.0f);
}

TEST(WeatherTest, WindStrengthIsBiasedByWeatherCategoryOnceSettled) {
    Weather w(/*seed=*/321, /*day_length_real_minutes=*/24.0);
    for (int i = 0; i < 100; ++i) {
        // Force the NEXT transition with one large jump (comfortably
        // exceeds the max 8h window), then step forward in small
        // increments ONLY until the crossfade itself actually settles --
        // NOT a fixed extra jump: a large forcing call can leave
        // hours_since_transition_started_ anywhere up to just under its
        // own rolled duration (not necessarily near 0), so a fixed
        // follow-up step can itself accidentally cross into yet another
        // transition boundary. 1000 one-second steps is a generous budget
        // (>3x the minimum 3h transition window) even if that happens.
        w.advance(20.0 * 60.0, /*temperature_c=*/10.0);
        for (int guard = 0; guard < 1000 && w.transition_progress() < 1.0f; ++guard) {
            w.advance(1.0, /*temperature_c=*/10.0);
        }
        ASSERT_FLOAT_EQ(w.transition_progress(), 1.0f) << "iteration " << i;

        const WindState wind  = w.wind();
        const bool      windy = (w.state() == WeatherState::Overcast || w.state() == WeatherState::Rain ||
                                  w.state() == WeatherState::Snow);
        if (windy) {
            EXPECT_GE(wind.strength, Weather::kWindyMinStrength) << "iteration " << i;
        } else {
            EXPECT_LE(wind.strength, Weather::kCalmMaxStrength) << "iteration " << i;
        }
    }
}

TEST(WeatherTest, WindStaysWithinValidRangesAcrossManySmallAdvances) {
    Weather w(/*seed=*/77, /*day_length_real_minutes=*/24.0);
    for (int i = 0; i < 200; ++i) {
        w.advance(30.0, /*temperature_c=*/10.0);
        const WindState wind = w.wind();
        EXPECT_GE(wind.strength, 0.0f) << "iteration " << i;
        EXPECT_LE(wind.strength, 1.0f) << "iteration " << i;
        EXPECT_GE(wind.direction_deg, 0.0) << "iteration " << i;
        EXPECT_LT(wind.direction_deg, 360.0) << "iteration " << i;
    }
}

TEST(WeatherTest, SameSeedProducesTheSameWindSequence) {
    Weather a(/*seed=*/2024, /*day_length_real_minutes=*/24.0);
    Weather b(/*seed=*/2024, /*day_length_real_minutes=*/24.0);
    for (int i = 0; i < 50; ++i) {
        a.advance(30.0, /*temperature_c=*/5.0);
        b.advance(30.0, /*temperature_c=*/5.0);
        ASSERT_FLOAT_EQ(a.wind().strength, b.wind().strength) << "iteration " << i;
        ASSERT_DOUBLE_EQ(a.wind().direction_deg, b.wind().direction_deg) << "iteration " << i;
    }
}
