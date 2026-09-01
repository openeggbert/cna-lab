// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "TimeOfDay.hpp"

namespace MeshWorld {

TimeOfDay::TimeOfDay(double day_length_real_minutes, double starting_hours)
    : day_length_real_minutes_(day_length_real_minutes), hours_(starting_hours) {
    // Normalize an out-of-range starting hour the same way advance() would,
    // so construction and advancement never disagree on what "wrapped"
    // means.
    while (hours_ >= 24.0) { hours_ -= 24.0; ++day_; }
    while (hours_ < 0.0) hours_ += 24.0;
}

void TimeOfDay::advance(double elapsed_real_seconds) {
    if (elapsed_real_seconds <= 0.0 || day_length_real_minutes_ <= 0.0) return;

    const double hours_per_real_second = 24.0 / (day_length_real_minutes_ * 60.0);
    hours_ += elapsed_real_seconds * hours_per_real_second;

    while (hours_ >= 24.0) {
        hours_ -= 24.0;
        ++day_;
    }
}

} // namespace MeshWorld
