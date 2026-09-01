// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cstdint>
#include <vector>

namespace MeshWorld {

// S103/S104 (sky/day-night/weather, 2026-07-11) — sun/moon sky position
// derived from TimeOfDay::hours(). Pure functions, no rendering
// dependencies -- same "pure logic in the root build, thin renderer glue in
// the app" split TimeOfDay.hpp itself and MAP11's M175-177 already
// established; later S3xx/S4xx rendering tasks (apps/mesh-world-app) will
// call these, but don't need to live there themselves.
//
// Deliberately a SIMPLE, stylized arc, not real solar astronomy: no
// latitude, season, or hemisphere modeling (matches the task's own "simple
// sinusoidal arc" wording) -- sunrise/noon/sunset land on the round hours
// the S-series plan names (06:00/12:00/18:00) by construction, not as an
// approximation of any real place on Earth.
struct SkyAngle {
    // Degrees above (positive) or below (negative) the horizon. +90 =
    // straight up (zenith), -90 = straight down (nadir, directly opposite
    // the zenith point in the sky).
    double elevation_deg{0.0};
    // Compass bearing in [0, 360): 0 = North, 90 = East, 180 = South,
    // 270 = West.
    double azimuth_deg{0.0};
};

// S103 — the sun's position for a given hour of day (0-24, wraps like
// TimeOfDay::hours() itself). Rises due East (azimuth 90) at 06:00,
// zeniths due South (elevation 90, azimuth 180) at 12:00, sets due West
// (azimuth 270) at 18:00, reaches its lowest point (elevation -90, azimuth
// 0/North) at midnight -- one full elevation sine cycle and one full
// azimuth sweep per 24h, in phase with each other.
SkyAngle sun_position(double hours);

// S104 — the moon's position as the sun's antipode: always on the exact
// opposite side of the sky (elevation negated, azimuth rotated 180°), so
// it's above the horizon roughly whenever the sun is below it (and vice
// versa) -- the same real-world sun/moon relationship a full moon
// approximates. Does NOT model the moon's own ~29.5-day orbital drift
// relative to the sun (a real moon isn't exactly antipodal most nights) --
// a deliberate v1 simplification matching this task's own "as the sun's
// antipode" wording; S402's separate phase calculation is what actually
// varies night to night, not this position.
SkyAngle moon_position(double hours);

// S402 — the moon's phase as a fraction through its cycle, in [0, 1):
// 0.0 (and, by wraparound, 1.0) = new moon, 0.25 = first quarter, 0.5 =
// full moon, 0.75 = last quarter. `day` is TimeOfDay::day()'s integer
// counter; `hours` is TimeOfDay::hours() (0-24) -- combined into a
// continuous "day progress" (day + hours/24) so the phase advances
// smoothly through each day instead of jumping once at every midnight
// rollover. `lunar_cycle_days` defaults to 8 in-game days per full cycle
// -- short enough to be noticeable within a normal play session, long
// enough to still feel gradual; open to a different value.
double moon_phase_fraction(int day, double hours, double lunar_cycle_days = 8.0);

// S501 -- a fixed, seeded set of star positions (same SkyAngle -- an
// elevation/azimuth pair -- sun_position()/moon_position() use), generated
// once from `seed` and otherwise constant: the same seed always produces
// the exact same stars, in the exact same order (S501's own "not a
// reshuffle every frame" requirement, and S503/render_stars()'s own
// "reveal more stars from a stable ordering" fade technique both depend on
// this). Uses a small local hash (splitmix64's finalizer), not
// Map::noise's hash2i()/to_unit_float() -- deliberately kept independent
// of the Map:: subsystem, matching how TimeOfDay/CelestialPosition/
// SkyColor/WorldRenderer's own sun+moon code never reaches into Map::
// either.
//
// `count` is clamped to [0, max_count] -- S502's own render-performance cap
// (proposed default 500-1000; 800 is the midpoint) -- enforced here, in the
// generator itself, so S504's "star count never exceeds the configured cap"
// is an invariant of the data, not just caller discipline.
//
// Each star's azimuth is uniform in [0, 360); elevation is
// asin(2u-1) for u uniform in [0,1), NOT uniform in [-90, 90] directly --
// the asin transform is what makes the resulting points evenly scattered
// over the full sky *sphere* rather than bunched up near the poles
// (zenith/nadir) the way naive uniform-elevation sampling would produce.
std::vector<SkyAngle> generate_star_field(std::uint64_t seed, int count, int max_count = 800);

} // namespace MeshWorld
