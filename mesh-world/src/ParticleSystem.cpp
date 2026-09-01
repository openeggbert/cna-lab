// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "ParticleSystem.hpp"

#include <cmath>

namespace MeshWorld {

namespace {
constexpr double kPi = 3.14159265358979323846;

double unit_double(std::mt19937_64& rng) {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng);
}
} // namespace

int particle_quality_count(ParticleQuality quality) {
    switch (quality) {
        case ParticleQuality::Low: return 200;
        case ParticleQuality::Medium: return 600;
        case ParticleQuality::High: return ParticleSystem::kMaxParticles;
    }
    return 0;
}

ParticleSystem::ParticleSystem(std::uint64_t seed, float spawn_radius_m, float spawn_height_m,
                                float min_height_m)
    : spawn_radius_m_(spawn_radius_m)
    , spawn_height_m_(spawn_height_m)
    , min_height_m_(min_height_m)
    , rng_(seed) {}

void ParticleSystem::respawn(Particle& p) {
    const double radius_frac = std::sqrt(unit_double(rng_));
    const double angle_rad   = unit_double(rng_) * 2.0 * kPi;
    p.x = static_cast<float>(spawn_radius_m_ * radius_frac * std::sin(angle_rad));
    p.z = static_cast<float>(spawn_radius_m_ * radius_frac * std::cos(angle_rad));
    // A little height jitter so recycled particles don't all pop back into
    // existence at exactly the same Y, which would read as a visible
    // "wave" of particles all falling in lockstep.
    p.y = spawn_height_m_ + static_cast<float>(unit_double(rng_)) * 4.0f;

    // S904 -- same azimuth convention sky_angle_to_xyz() (WorldRenderer.cpp)
    // uses for "a unit vector pointing TOWARD this compass direction":
    // x = sin(dir), z = -cos(dir).
    constexpr double kDeg2Rad     = kPi / 180.0;
    const double     wind_dir_rad = current_wind_.direction_deg * kDeg2Rad;

    if (active_weather_ == WeatherState::Rain) {
        // Wind adds a straight horizontal push on top of the fall --
        // heavier wind visibly slants the rain.
        const float drift = current_wind_.strength * ParticleSystem::kRainWindDriftMPerS;
        p.vx = static_cast<float>(std::sin(wind_dir_rad)) * drift;
        p.vy = -ParticleSystem::kRainFallSpeedMPerS;
        p.vz = static_cast<float>(-std::cos(wind_dir_rad)) * drift;
        p.lifetime_s = ParticleSystem::kRainLifetimeS;
    } else if (active_weather_ == WeatherState::Snow) {
        // Drift direction is biased toward the wind (jittered per spawn for
        // natural variation, not identical for every flake); magnitude
        // combines the base gentle drift with a wind-scaled extra.
        const double jitter_rad =
            (unit_double(rng_) * 2.0 - 1.0) * ParticleSystem::kSnowDriftJitterDeg * kDeg2Rad;
        const double drift_angle = wind_dir_rad + jitter_rad;
        const float  drift_speed =
            ParticleSystem::kSnowDriftSpeedMPerS + current_wind_.strength * ParticleSystem::kSnowWindDriftMPerS;
        p.vx = static_cast<float>(std::sin(drift_angle)) * drift_speed;
        p.vy = -ParticleSystem::kSnowFallSpeedMPerS;
        p.vz = static_cast<float>(-std::cos(drift_angle)) * drift_speed;
        p.lifetime_s = ParticleSystem::kSnowLifetimeS;
    } else {
        p.vx = p.vy = p.vz = 0.0f;
        p.lifetime_s = 0.0f;
    }
}

void ParticleSystem::advance(float dt, WeatherState weather, ParticleQuality quality, WindState wind,
                              double cam_x, double cam_y, double cam_z) {
    // S904 -- updated before any respawn() calls below, so newly-spawned
    // particles (both brand-new pool growth and mid-flight recycles later
    // in this same call) always use the CURRENT wind, never last frame's.
    current_wind_ = wind;

    // S802/S803 -- only Rain/Snow ever have active particles.
    const bool is_precipitating = (weather == WeatherState::Rain || weather == WeatherState::Snow);
    const int  target_count     = is_precipitating ? particle_quality_count(quality) : 0;

    // A genuine weather-kind change (not just a quality change) invalidates
    // every existing particle's velocity/lifetime profile -- clear and
    // respawn fresh rather than let mismatched-looking particles limp along
    // until they individually recycle.
    if (weather != active_weather_) {
        active_weather_ = weather;
        particles_.clear();
    }

    const auto old_size = particles_.size();
    particles_.resize(static_cast<std::size_t>(target_count));
    for (std::size_t i = old_size; i < particles_.size(); ++i) respawn(particles_[i]);

    // Floating-origin re-homing: shift every particle by the negative of
    // how far the camera moved since the last call, so they stay anchored
    // in world space as the player walks (see this method's own doc
    // comment in ParticleSystem.hpp).
    float dcx = 0.0f, dcy = 0.0f, dcz = 0.0f;
    if (have_prev_cam_) {
        dcx = static_cast<float>(cam_x - prev_cam_x_);
        dcy = static_cast<float>(cam_y - prev_cam_y_);
        dcz = static_cast<float>(cam_z - prev_cam_z_);
    }
    prev_cam_x_ = cam_x;
    prev_cam_y_ = cam_y;
    prev_cam_z_ = cam_z;
    have_prev_cam_ = true;

    // Re-homing is a coordinate-frame correction for camera movement, not a
    // simulated time step -- it must NOT be skipped when dt<=0 (a caller
    // can legitimately report camera movement with no elapsed simulation
    // time, e.g. two calls within the same frame), unlike the
    // velocity/lifetime integration below.
    if (dcx != 0.0f || dcy != 0.0f || dcz != 0.0f) {
        for (Particle& p : particles_) {
            p.x -= dcx;
            p.y -= dcy;
            p.z -= dcz;
        }
    }

    if (dt <= 0.0f) return;

    for (Particle& p : particles_) {
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.z += p.vz * dt;
        p.lifetime_s -= dt;

        if (p.lifetime_s <= 0.0f || p.y < min_height_m_) respawn(p);
    }
}

} // namespace MeshWorld
