// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Weather.hpp"

#include <algorithm>
#include <array>

namespace MeshWorld {

namespace {
double roll_transition_duration_hours(std::mt19937_64& rng) {
    std::uniform_real_distribution<double> dist(Weather::kMinTransitionHours,
                                                 Weather::kMaxTransitionHours);
    return dist(rng);
}
} // namespace

Weather::Weather(std::uint64_t seed, double day_length_real_minutes)
    : day_length_real_minutes_(day_length_real_minutes), rng_(seed) {
    current_transition_duration_hours_ = roll_transition_duration_hours(rng_);
}

float Weather::transition_progress() const {
    // Nothing to crossfade FROM (construction, or a reroll that happened to
    // pick the same state again) -- always fully settled, regardless of how
    // much time has elapsed since.
    if (previous_state_ == state_) return 1.0f;
    if (current_transition_duration_hours_ <= 0.0) return 1.0f;
    return static_cast<float>(std::min(1.0, hours_since_transition_started_ / kCrossfadeHours));
}

WeatherState Weather::pick_next_state(double temperature_c) {
    // S603 -- snow only selectable at/below freezing; rain fills the same
    // "precipitation" slot otherwise. May re-pick the same state the
    // machine is already in -- a genuine, honest possibility of this being
    // a timer-driven reroll, not a guaranteed change.
    static constexpr std::array<WeatherState, 4> kWarmPool = {
        WeatherState::Clear, WeatherState::PartlyCloudy, WeatherState::Overcast, WeatherState::Rain};
    static constexpr std::array<WeatherState, 4> kColdPool = {
        WeatherState::Clear, WeatherState::PartlyCloudy, WeatherState::Overcast, WeatherState::Snow};

    const auto& pool = (temperature_c <= kFreezingC) ? kColdPool : kWarmPool;
    std::uniform_int_distribution<std::size_t> pick(0, pool.size() - 1);
    return pool[pick(rng_)];
}

WindState Weather::pick_next_wind(WeatherState state) {
    const bool windy = (state == WeatherState::Overcast || state == WeatherState::Rain ||
                         state == WeatherState::Snow);
    std::uniform_real_distribution<double> dir_dist(0.0, 360.0);
    std::uniform_real_distribution<float>  strength_dist(windy ? kWindyMinStrength : 0.0f,
                                                          windy ? 1.0f : kCalmMaxStrength);
    WindState w;
    w.direction_deg = dir_dist(rng_);
    w.strength       = strength_dist(rng_);
    return w;
}

WindState Weather::wind() const {
    const float t = transition_progress();

    WindState result;
    result.strength = previous_wind_.strength + (target_wind_.strength - previous_wind_.strength) * t;

    // Interpolate along the shorter angular path (e.g. 350deg -> 10deg
    // sweeps forward through 360/0, not backward through 180).
    double delta = target_wind_.direction_deg - previous_wind_.direction_deg;
    while (delta > 180.0) delta -= 360.0;
    while (delta < -180.0) delta += 360.0;
    double dir = previous_wind_.direction_deg + delta * static_cast<double>(t);
    while (dir < 0.0) dir += 360.0;
    while (dir >= 360.0) dir -= 360.0;
    result.direction_deg = dir;

    return result;
}

void Weather::advance(double elapsed_real_seconds, double temperature_c) {
    if (elapsed_real_seconds <= 0.0 || day_length_real_minutes_ <= 0.0) return;

    // Same real-seconds -> in-game-hours conversion as TimeOfDay::advance().
    const double hours_per_real_second = 24.0 / (day_length_real_minutes_ * 60.0);
    double remaining_hours             = elapsed_real_seconds * hours_per_real_second;

    // Walks forward transition-interval by transition-interval rather than
    // adding remaining_hours to hours_since_transition_started_ in one
    // shot, so a single large advance() call (e.g. after a long pause)
    // correctly rolls through however many transitions it actually spans --
    // any leftover time past a boundary carries into the NEW interval's own
    // hours_since_transition_started_, instead of being discarded (which
    // would otherwise leave transition_progress() stuck at 0 right after a
    // transition even when the elapsed time was easily enough to also
    // finish that transition's own crossfade). Same "don't just handle the
    // common per-frame case" precedent TimeOfDay::advance()'s own
    // multi-day-wrap handling sets.
    while (remaining_hours > 0.0) {
        const double hours_left_in_current =
            current_transition_duration_hours_ - hours_since_transition_started_;
        if (remaining_hours < hours_left_in_current) {
            hours_since_transition_started_ += remaining_hours;
            remaining_hours = 0.0;
        } else {
            remaining_hours -= hours_left_in_current;
            previous_state_                    = state_;
            state_                              = pick_next_state(temperature_c);
            // S901 -- a new wind target is rolled every time a weather
            // transition fires ("paired with the weather-transition
            // timer"), biased by the NEW state.
            previous_wind_                      = target_wind_;
            target_wind_                        = pick_next_wind(state_);
            hours_since_transition_started_     = 0.0;
            current_transition_duration_hours_  = roll_transition_duration_hours(rng_);
        }
    }
}

} // namespace MeshWorld
