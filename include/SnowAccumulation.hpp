// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include "Weather.hpp"  // WeatherState

namespace MeshWorld {

// S1003 -- a single global snow-depth scalar (0 = bare ground, 1 = fully
// accumulated), not tracked per-object: with potentially hundreds of trees/
// props/roofs in view, per-instance accumulation bookkeeping would be real
// complexity for no visible benefit (S1002's own overlays all just scale
// their thickness by this one shared value). Pure state, no rendering
// dependencies -- same "pure logic in the root build, thin renderer glue in
// the app" split every earlier S-task established.
class SnowAccumulation {
public:
    // S603's own freezing threshold, reused here rather than duplicated.
    static constexpr double kFreezingC = Weather::kFreezingC;

    // Builds from 0 to fully accumulated (1.0) over this many continuous
    // in-game hours of snowfall -- comfortably reachable within a single
    // `WeatherState::Snow` spell (S602's own [3,8)h transition window).
    static constexpr float kAccumulationRatePerHour = 1.0f / 4.0f;

    // Melts from fully accumulated to bare over this many in-game hours
    // once melting conditions apply -- slower than accumulation, so snow
    // visibly lingers a while after conditions turn favorable again.
    static constexpr float kMeltRatePerHour = 1.0f / 6.0f;

    // "Enough time passes without fresh snowfall" (S1003's own wording) --
    // melting-by-elapsed-time-alone only kicks in after this many
    // continuous in-game hours without snow, even if still below freezing,
    // so a brief lull mid-snowstorm doesn't immediately start melting.
    static constexpr double kNoFreshSnowMeltDelayHours = 12.0;

    SnowAccumulation() = default;

    // Current accumulation, [0,1].
    float depth() const { return depth_; }

    // Advances by `elapsed_real_seconds` (wall-clock time, e.g. a frame's
    // delta time -- same unit TimeOfDay::advance()/Weather::advance() take).
    // `weather`/`temperature_c` are the CURRENT values at the moment this
    // call is made (re-sampled every call, not cached). `day_length_real_minutes`
    // matches TimeOfDay's own real-seconds-to-in-game-hours conversion.
    //
    // Accumulates while `weather == Snow` AND `temperature_c <= kFreezingC`
    // (S1003's own "builds up while WeatherState == snow and temperature is
    // at/below freezing"). Melts once EITHER `temperature_c > kFreezingC`
    // OR `kNoFreshSnowMeltDelayHours` have passed since the last moment it
    // was actively accumulating (S1003's own literal "OR" wording -- a
    // deliberate stylized simplification, not a claim that real snow melts
    // from time alone while still below freezing).
    void advance(double elapsed_real_seconds, WeatherState weather, double temperature_c,
                 double day_length_real_minutes = 24.0);

private:
    float  depth_{0.0f};
    double hours_since_last_snowfall_{kNoFreshSnowMeltDelayHours};  // starts "long since snowed"
};

} // namespace MeshWorld
