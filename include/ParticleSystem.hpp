// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include "Weather.hpp"  // WeatherState
#include <cstdint>
#include <random>
#include <vector>

namespace MeshWorld {

// S801 -- one falling precipitation particle: camera-relative position (same
// convention render_sun()/render_placements()/etc. already use), a fall/
// drift velocity (m/s), and remaining lifetime (seconds) before it's
// recycled to a fresh spawn point.
struct Particle {
    float x{0.0f}, y{0.0f}, z{0.0f};
    float vx{0.0f}, vy{0.0f}, vz{0.0f};
    float lifetime_s{0.0f};
};

// S804 -- how many of ParticleSystem::kMaxParticles are actually simulated/
// rendered at once.
enum class ParticleQuality { Low, Medium, High };

// S804 -- particle counts per quality tier; `high` is also
// ParticleSystem::kMaxParticles (S805's own "pool never exceeds its
// configured cap" -- High IS the cap, by construction, not a separate
// number that could drift out of sync with it).
int particle_quality_count(ParticleQuality quality);

// S801-S805 -- a fixed-cap pool of rain/snow particles, recycled within a
// bounding disk centered on the camera. The first real particle
// infrastructure in either this repo or mesh-craft (confirmed nothing
// existing to build on before writing this -- see plan.md's own S-series
// intro). Pure state, no rendering dependencies -- same "pure logic in the
// root build, thin renderer glue in the app" split every earlier S-task
// established.
//
// Only active for WeatherState::Rain or WeatherState::Snow (S802/S803's own
// "spawned only when..." wording) -- any other state means zero active
// particles. Rain and snow are mutually exclusive (Weather::pick_next_state()
// never selects both at once), so this tracks at most one "kind" of effect
// live at a time, not a persistent mix.
class ParticleSystem {
public:
    // S805 -- the hard cap: particles() never holds more than this many
    // entries, regardless of quality tier (== particle_quality_count(High)).
    static constexpr int kMaxParticles = 1500;

    // Rain: fast, straight-down, short-lived (recycles quickly, matching a
    // real downpour's density). Snow: slow, gently drifting sideways,
    // longer-lived (matches a real snowfall's lazy drift). Stylized, not
    // physically accurate -- same bar CelestialPosition.hpp/SkyColor.hpp
    // already set for this S-series.
    static constexpr float kRainFallSpeedMPerS  = 18.0f;
    static constexpr float kRainLifetimeS       = 4.0f;
    static constexpr float kSnowFallSpeedMPerS  = 1.5f;
    static constexpr float kSnowDriftSpeedMPerS = 0.6f;
    static constexpr float kSnowLifetimeS       = 20.0f;

    // S904 -- "wind also drives precipitation particle horizontal drift".
    // Rain: wind adds a straight horizontal push (max speed at
    // wind.strength=1) on top of the fall above -- heavier wind visibly
    // slants the rain. Snow: wind biases the existing per-particle drift
    // direction toward wind.direction_deg (within +-kSnowDriftJitterDeg,
    // still rerolled per spawn for natural variation) and adds to its own
    // base drift speed, rather than snow drifting in a fully independent
    // random direction the way it did before wind existed.
    static constexpr float kRainWindDriftMPerS = 6.0f;
    static constexpr float kSnowWindDriftMPerS = 2.0f;
    static constexpr float kSnowDriftJitterDeg = 40.0f;

    // The bounding volume particles spawn/recycle within: a disk of
    // `spawn_radius_m` centered on the camera's horizontal position, particles
    // spawning near `spawn_height_m` above the camera and falling until they
    // pass `min_height_m` below it (or their own lifetime expires,
    // whichever happens first), at which point they respawn back at the
    // top with a fresh random horizontal position.
    explicit ParticleSystem(std::uint64_t seed, float spawn_radius_m = 60.0f,
                             float spawn_height_m = 40.0f, float min_height_m = -5.0f);

    // Advances the simulation by `dt` seconds (wall-clock time, e.g. a
    // frame's delta time). `weather` selects which effect (if any) is
    // active; `quality` scales how many particles are simulated (S804).
    // `wind` (S904) biases horizontal drift -- see kRainWindDriftMPerS/
    // kSnowWindDriftMPerS/kSnowDriftJitterDeg's own doc comments.
    // `cam_x/cam_y/cam_z` is the player's TRUE, double-precision world
    // position (not a renderer's float-precision camera) -- particles are
    // re-homed by the camera's own frame-to-frame delta so they stay
    // visually anchored in world space as the player walks (the same
    // floating-origin idea compute_visible_placement_instances() uses,
    // applied incrementally here since particles carry persistent identity
    // instead of being recomputed from scratch every frame).
    void advance(float dt, WeatherState weather, ParticleQuality quality, WindState wind, double cam_x,
                 double cam_y, double cam_z);

    // Currently active particles, camera-relative and ready to hand to a
    // renderer. Always has exactly particle_quality_count(quality) entries
    // (from the most recent advance() call) when the active effect is Rain
    // or Snow, and is empty otherwise.
    const std::vector<Particle>& particles() const { return particles_; }

private:
    void respawn(Particle& p);

    float spawn_radius_m_;
    float spawn_height_m_;
    float min_height_m_;

    std::mt19937_64       rng_;
    std::vector<Particle> particles_;
    WeatherState          active_weather_{WeatherState::Clear};
    WindState             current_wind_{};
    bool                  have_prev_cam_{false};
    double                prev_cam_x_{0.0}, prev_cam_y_{0.0}, prev_cam_z_{0.0};
};

} // namespace MeshWorld
