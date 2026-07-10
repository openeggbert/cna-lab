#include "DayNightCycle.hpp"

#include <cmath>

namespace CnaCraft::Worlds {

float ComputeTimeOfDay(float elapsedSeconds, float dayLengthSeconds) {
    if (dayLengthSeconds <= 0.0f) return 0.5f; // Craft's own day_length<=0 branch: pinned to noon
    float t = elapsedSeconds / dayLengthSeconds;
    return t - std::floor(t); // wrap to [0, 1)
}

float ComputeDaylight(float elapsedSeconds, float dayLengthSeconds) {
    const float t = ComputeTimeOfDay(elapsedSeconds, dayLengthSeconds);
    // Note the degenerate dayLength<=0 case now evaluates the curve at
    // t=0.5 (noon) and returns ~1.0 full daylight -- matching Craft's real
    // behavior (get_daylight computes from time_of_day()'s pinned 0.5, it
    // doesn't short-circuit); an earlier version of this function returned
    // a literal 0.5 brightness there, which matched nothing in Craft.
    if (t < 0.5f) {
        const float x = (t - 0.25f) * 100.0f;
        return 1.0f / (1.0f + std::pow(2.0f, -x));
    }
    const float x = (t - 0.85f) * 100.0f;
    return 1.0f - 1.0f / (1.0f + std::pow(2.0f, -x));
}

}
