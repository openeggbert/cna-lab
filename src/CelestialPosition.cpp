// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "CelestialPosition.hpp"

#include <algorithm>
#include <cmath>

namespace MeshWorld {

namespace {
constexpr double kPi = 3.14159265358979323846;

// Wraps a degree value into [0, 360).
double wrap_deg(double deg) {
    double d = std::fmod(deg, 360.0);
    if (d < 0.0) d += 360.0;
    return d;
}

// S501 -- splitmix64's own finalizer/mixing step, used only to seed the
// star field. Deterministic, well-distributed, and self-contained (no
// dependency on Map::noise -- see generate_star_field()'s own doc comment).
std::uint64_t star_hash(std::uint64_t seed, int index) {
    std::uint64_t x = seed + static_cast<std::uint64_t>(index) * 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    x = x ^ (x >> 31);
    return x;
}

// Top 53 bits of the hash -> a double in [0, 1).
double star_unit_float(std::uint64_t h) {
    return static_cast<double>(h >> 11) / static_cast<double>(1ULL << 53);
}
} // namespace

SkyAngle sun_position(double hours) {
    // theta = 0 at 06:00 (sunrise), pi/2 at 12:00 (noon), pi at 18:00
    // (sunset), 3pi/2 at 24:00/00:00 (midnight) -- one full revolution
    // every 24h.
    const double theta = 2.0 * kPi * (hours - 6.0) / 24.0;

    SkyAngle sky;
    sky.elevation_deg = 90.0 * std::sin(theta);
    // azimuth = 90 (East) at theta=0, sweeping through 180 (South) at
    // theta=pi/2, 270 (West) at theta=pi, back to 0/360 (North) at
    // theta=3pi/2 -- in phase with the elevation sine above.
    sky.azimuth_deg = wrap_deg(90.0 + 180.0 * (theta / kPi));
    return sky;
}

SkyAngle moon_position(double hours) {
    const SkyAngle sun = sun_position(hours);
    SkyAngle       moon;
    moon.elevation_deg = -sun.elevation_deg;
    moon.azimuth_deg    = wrap_deg(sun.azimuth_deg + 180.0);
    return moon;
}

double moon_phase_fraction(int day, double hours, double lunar_cycle_days) {
    if (lunar_cycle_days <= 0.0) return 0.0;

    const double continuous_days = static_cast<double>(day) + hours / 24.0;
    double       fraction        = std::fmod(continuous_days, lunar_cycle_days) / lunar_cycle_days;
    if (fraction < 0.0) fraction += 1.0;
    return fraction;
}

std::vector<SkyAngle> generate_star_field(std::uint64_t seed, int count, int max_count) {
    const int clamped_max   = std::max(max_count, 0);
    const int clamped_count = std::clamp(count, 0, clamped_max);

    std::vector<SkyAngle> stars;
    stars.reserve(static_cast<std::size_t>(clamped_count));

    for (int i = 0; i < clamped_count; ++i) {
        const double u_azimuth   = star_unit_float(star_hash(seed, i * 2));
        const double u_elevation = star_unit_float(star_hash(seed, i * 2 + 1));

        SkyAngle star;
        star.azimuth_deg = u_azimuth * 360.0;
        // asin(2u-1), not (u * 180 - 90) -- see generate_star_field()'s own
        // doc comment on why this gives a uniform scatter over the sphere
        // rather than clustering near the zenith/nadir.
        star.elevation_deg = std::asin(2.0 * u_elevation - 1.0) * (180.0 / kPi);
        stars.push_back(star);
    }

    return stars;
}

} // namespace MeshWorld
