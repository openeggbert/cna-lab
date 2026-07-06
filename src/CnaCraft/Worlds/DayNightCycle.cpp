#include "DayNightCycle.hpp"

#include <cmath>

namespace CnaCraft::Worlds {

float ComputeDaylight(float elapsedSeconds, float dayLengthSeconds) {
    if (dayLengthSeconds <= 0.0f) return 0.5f;

    float t = elapsedSeconds / dayLengthSeconds;
    t -= std::floor(t); // wrap to [0, 1)

    if (t < 0.5f) {
        const float x = (t - 0.25f) * 100.0f;
        return 1.0f / (1.0f + std::pow(2.0f, -x));
    }
    const float x = (t - 0.85f) * 100.0f;
    return 1.0f - 1.0f / (1.0f + std::pow(2.0f, -x));
}

}
