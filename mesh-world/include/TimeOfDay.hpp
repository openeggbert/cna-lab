// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

namespace MeshWorld {

// S101/S102/S105 (sky/day-night/weather, 2026-07-11) — the world's day/night
// clock. Pure state, no rendering dependencies -- lives in the root build
// (MeshWorldLib) rather than apps/mesh-world-app, same "pure logic in the
// root build, thin renderer glue in the app" split MAP11's M175-177
// established for compute_visible_placement_instances(). Later S-series
// tasks (sun/moon position, sky color, weather transitions) will read
// hours()/day() from apps/mesh-world-app but don't need to live there
// themselves.
//
// Not persisted (S101's own scope): like camera_/vel_y_/on_ground_ in
// apps/mesh-world-app's WorldApp, this resets to its starting hour every
// time a world is (re-)entered, the same "explore-session state, not saved
// world state" precedent those fields already establish. Real persistence
// would be separate, later work if wanted.
class TimeOfDay {
public:
    // S102 — 24 real minutes per in-game day (1 real minute = 1 in-game
    // hour, an easy mental mapping). Proposed default; easy to retune here
    // if a playtest says it feels too fast/slow.
    static constexpr double kDefaultDayLengthRealMinutes = 24.0;

    // Starts at mid-morning (08:00) by default so a freshly (re-)entered
    // world begins in daylight rather than at pitch-dark midnight.
    explicit TimeOfDay(double day_length_real_minutes = kDefaultDayLengthRealMinutes,
                        double starting_hours = 8.0);

    // Advances the clock by `elapsed_real_seconds` (wall-clock time, e.g. a
    // frame's delta time). Wraps hours() into [0,24) and increments day()
    // exactly once per full 24h wrap -- a single call spanning more than
    // one day (e.g. after a long pause) still increments day() correctly,
    // not just once. A non-positive elapsed time is a no-op.
    void advance(double elapsed_real_seconds);

    // Current hour of day, always in [0, 24).
    double hours() const { return hours_; }

    // Integer day counter, starting at 0, incrementing once per 24h wrap.
    // Feeds the moon-phase calculation (S402) and the weather-transition
    // timer (S602).
    int day() const { return day_; }

private:
    double day_length_real_minutes_;
    double hours_;
    int    day_{0};
};

} // namespace MeshWorld
