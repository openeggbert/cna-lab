// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cstdint>
#include <random>

namespace MeshWorld {

// S601 -- the five weather states this S-series backlog names. `rain` and
// `snow` are mutually exclusive "precipitation" states, gated by
// temperature (S603) rather than ever chosen together.
enum class WeatherState { Clear, PartlyCloudy, Overcast, Rain, Snow };

// S901 -- wind direction + strength. `direction_deg` uses the same compass-
// bearing convention SkyAngle::azimuth_deg does (0/North, 90/East, ...);
// `strength` is normalized [0,1] (0 = dead calm, 1 = strongest wind this
// simulation ever produces), not a real m/s wind speed -- consumers
// (S902's tree sway, S904's cloud drift/particle drift) each scale it by
// their own stylized constant.
struct WindState {
    double direction_deg{0.0};
    float  strength{0.0f};
};

// S601-S603 -- an entropy-driven weather state machine. Pure state, no
// rendering dependencies -- same "pure logic in the root build, thin
// renderer glue in the app" split TimeOfDay/CelestialPosition/SkyColor/
// WorldRenderer's own pure functions already established.
//
// Deliberately NOT seeded from world_entropy/generate_star_field()'s own
// deterministic pattern: unlike stars (S501's own "same seed always shows
// the same stars" requirement -- a fixed sky decoration), weather is
// ephemeral session state with no persistence/consistency requirement,
// closer to TimeOfDay's own "resets fresh every time a world is
// (re-)entered" precedent. The project's own "world state is
// non-reproducible by design, entropy is time-based" principle (see
// NEXT.md) argues for a real, non-reproducible seed at the app call site
// (e.g. steady_clock, same pattern PlanetWorld/PersistentWorldMap already
// use) -- the constructor still accepts an explicit seed so tests stay
// deterministic.
class Weather {
public:
    // S602 -- how long (in-game hours) a weather state lasts before the
    // next transition is rolled; randomized per transition within this
    // range each time.
    static constexpr double kMinTransitionHours = 3.0;
    static constexpr double kMaxTransitionHours = 8.0;

    // S602 -- how long (in-game hours) a transition's crossfade lasts once
    // triggered ("a short crossfade instead of an instant hard cut").
    static constexpr double kCrossfadeHours = 0.5;

    // S603 -- at/below this local temperature (Celsius), a rolled
    // precipitation state is `snow`; above it, `rain`.
    static constexpr double kFreezingC = 0.0;

    // `seed` drives the internal RNG (state selection + transition
    // duration); `day_length_real_minutes` matches TimeOfDay's own
    // real-seconds-to-in-game-hours conversion, so `advance()` below can
    // take the same wall-clock delta time TimeOfDay::advance() does.
    explicit Weather(std::uint64_t seed, double day_length_real_minutes = 24.0);

    // The state fully arrived at once transition_progress() reaches 1;
    // while a crossfade is in progress, this is the NEW/target state (same
    // "what's true going forward" bias TimeOfDay::hours() has after a wrap).
    WeatherState state() const { return state_; }

    // The state being faded FROM. Equal to state() whenever
    // transition_progress() is 1 (no active crossfade) -- callers that
    // don't care about crossfading (S7xx/S8xx polish, not this task) can
    // just always read state() and ignore this.
    WeatherState previous_state() const { return previous_state_; }

    // S901 -- current wind, gradually interpolated between the previously-
    // rolled wind and a freshly-rolled target using the EXACT SAME
    // crossfade progress transition_progress() computes -- "paired with the
    // weather-transition timer (S602)" per this task's own wording: a new
    // wind target is rolled every time pick_next_state() fires (see
    // pick_next_wind()'s own doc comment), not on a separate timer.
    // `direction_deg` interpolates along the shorter angular path (never
    // spinning the "wrong way" around through 360°); `strength` interpolates
    // linearly.
    WindState wind() const;

    // 0 = a transition just started (still effectively reads as
    // previous_state()), 1 = fully arrived at state() (S602's own "short
    // crossfade instead of an instant hard cut"), or there's nothing to
    // crossfade from at all (construction, or a reroll that happened to
    // pick the same state again -- see pick_next_state()'s own doc
    // comment). Derived from hours_since_transition_started_ rather than
    // stored/incremented directly, so it stays correct even when a single
    // advance() call spans past a transition boundary (the leftover time
    // past that boundary still counts toward the NEW crossfade, not
    // discarded).
    float transition_progress() const;

    // Advances the weather clock by `elapsed_real_seconds` (wall-clock
    // time, e.g. a frame's delta time -- same unit TimeOfDay::advance()
    // takes) and rolls a new transition once the current one's randomized
    // duration elapses. `temperature_c` is the LOCAL temperature (the
    // map-layer temperature field at the player's current position, where
    // available) at the moment this call is made -- re-sampled on every
    // call so a transition rolled after the player has moved to a
    // different biome uses that biome's own temperature, not a stale one
    // from world-load time. Gates snow eligibility each time a new target
    // state is picked (S603). A single call spanning more than one
    // transition period (e.g. after a long pause) rolls multiple
    // transitions in sequence, not just one -- same "don't just handle the
    // common case" precedent TimeOfDay::advance()'s own multi-day-wrap
    // handling sets.
    void advance(double elapsed_real_seconds, double temperature_c);

    // S901 -- below this strength, `overcast`/`rain`/`snow`'s own bias
    // range starts; `clear`/`partly_cloudy` never roll above it. Keeps the
    // "bias toward stronger wind" requirement simple: two disjoint ranges
    // rather than a continuous weighted distribution.
    static constexpr float kCalmMaxStrength = 0.5f;
    static constexpr float kWindyMinStrength = 0.4f;

private:
    WeatherState pick_next_state(double temperature_c);
    // S901 -- rolls a new wind target every time pick_next_state() fires,
    // biased by the NEW weather state: `overcast`/`rain`/`snow` sample from
    // [kWindyMinStrength, 1], `clear`/`partly_cloudy` from [0, kCalmMaxStrength]
    // -- the two ranges deliberately overlap slightly (0.4-0.5) rather than
    // hard-partitioning at a single point, so the "windy" tier doesn't
    // always feel like a hard floor.
    WindState pick_next_wind(WeatherState state);

    double           day_length_real_minutes_;
    std::mt19937_64  rng_;
    WeatherState     previous_state_{WeatherState::Clear};
    WeatherState     state_{WeatherState::Clear};
    WindState        previous_wind_{};
    WindState        target_wind_{};
    // How far into the CURRENT transition interval we are, and how long
    // that interval was rolled to be (in [kMinTransitionHours,
    // kMaxTransitionHours)) -- transition_progress() and "is it time for
    // the next transition" are both derived from these two, rather than
    // tracked as separate incrementally-updated fields, so large advance()
    // calls that span a transition boundary stay exactly correct (see
    // transition_progress()'s own doc comment).
    double           hours_since_transition_started_{0.0};
    double           current_transition_duration_hours_;
};

} // namespace MeshWorld
