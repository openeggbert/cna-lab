#pragma once

namespace CnaCraft::Worlds {

// Matches Craft's own day length (`DAY_LENGTH` in src/config.h): 600 seconds
// (10 minutes) per full day/night cycle.
constexpr float kDefaultDayLengthSeconds = 600.0f;

// Deterministic, dependency-free day/night brightness curve (plan.md §11.3),
// ported from Craft's own `get_daylight()`/`time_of_day()` (src/main.c): two
// sigmoid transitions (dawn centered at 1/4 of the cycle, dusk centered at
// 17/20) bracket a long full-daylight plateau, with a corresponding
// full-night stretch on either side of midnight. `elapsedSeconds` wraps
// automatically (any value, positive or growing without bound, is valid —
// callers can feed GameTime::TotalGameTime directly). Returns a value in
// [0, 1]; 0 is full night, 1 is full day.
float ComputeDaylight(float elapsedSeconds, float dayLengthSeconds = kDefaultDayLengthSeconds);

}
