#pragma once

namespace CnaCraft::Worlds {

// Matches Craft's own day length (`DAY_LENGTH` in src/config.h): 600 seconds
// (10 minutes) per full day/night cycle.
constexpr float kDefaultDayLengthSeconds = 600.0f;

// Craft's own `time_of_day()` (src/main.c): elapsed time as a fraction of
// the day cycle, wrapped to [0, 1) — 0 is midnight, 0.25 dawn, 0.5 noon,
// ~0.85 dusk. This is the `timer` value Craft feeds its sky/block shaders
// (the sky texture's U coordinate — plan.md §12.1 item 33) and the input
// ComputeDaylight's brightness curve is defined over. Returns 0.5 (noon)
// when dayLengthSeconds <= 0, matching Craft's own day_length<=0 branch.
float ComputeTimeOfDay(float elapsedSeconds, float dayLengthSeconds = kDefaultDayLengthSeconds);

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
