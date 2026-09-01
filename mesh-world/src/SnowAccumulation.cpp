// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "SnowAccumulation.hpp"

#include <algorithm>

namespace MeshWorld {

void SnowAccumulation::advance(double elapsed_real_seconds, WeatherState weather, double temperature_c,
                                double day_length_real_minutes) {
    if (elapsed_real_seconds <= 0.0 || day_length_real_minutes <= 0.0) return;

    // Same real-seconds -> in-game-hours conversion as
    // TimeOfDay::advance()/Weather::advance().
    const double hours_per_real_second = 24.0 / (day_length_real_minutes * 60.0);
    const double elapsed_hours         = elapsed_real_seconds * hours_per_real_second;

    const bool actively_snowing = (weather == WeatherState::Snow && temperature_c <= kFreezingC);

    if (actively_snowing) {
        depth_ = std::min(1.0f, depth_ + kAccumulationRatePerHour * static_cast<float>(elapsed_hours));
        hours_since_last_snowfall_ = 0.0;
        return;
    }

    hours_since_last_snowfall_ += elapsed_hours;

    const bool should_melt =
        (temperature_c > kFreezingC) || (hours_since_last_snowfall_ >= kNoFreshSnowMeltDelayHours);
    if (should_melt) {
        depth_ = std::max(0.0f, depth_ - kMeltRatePerHour * static_cast<float>(elapsed_hours));
    }
}

} // namespace MeshWorld
