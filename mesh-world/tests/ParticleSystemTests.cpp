// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// Sky/day-night/weather tests. S801-S805: precipitation particle system
// (pool cap, quality tiers, recycling, weather gating, floating-origin
// re-homing).

#include <gtest/gtest.h>

#include "ParticleSystem.hpp"

#include <cmath>

using namespace MeshWorld;

TEST(ParticleSystemTest, QualityCountsMatchDocumentedTiers) {
    EXPECT_EQ(particle_quality_count(ParticleQuality::Low), 200);
    EXPECT_EQ(particle_quality_count(ParticleQuality::Medium), 600);
    EXPECT_EQ(particle_quality_count(ParticleQuality::High), ParticleSystem::kMaxParticles);
}

TEST(ParticleSystemTest, HighQualityIsExactlyTheCap) {
    EXPECT_EQ(particle_quality_count(ParticleQuality::High), 1500);
    EXPECT_EQ(ParticleSystem::kMaxParticles, 1500);
}

TEST(ParticleSystemTest, NoParticlesWhenNotPrecipitating) {
    for (WeatherState s : {WeatherState::Clear, WeatherState::PartlyCloudy, WeatherState::Overcast}) {
        ParticleSystem ps(/*seed=*/1);
        ps.advance(1.0f / 60.0f, s, ParticleQuality::High, WindState{}, 0.0, 0.0, 0.0);
        EXPECT_TRUE(ps.particles().empty()) << "weather=" << static_cast<int>(s);
    }
}

TEST(ParticleSystemTest, RainActivatesExactlyTheQualityCount) {
    ParticleSystem ps(/*seed=*/1);
    ps.advance(1.0f / 60.0f, WeatherState::Rain, ParticleQuality::Medium, WindState{}, 0.0, 0.0, 0.0);
    EXPECT_EQ(ps.particles().size(), static_cast<std::size_t>(particle_quality_count(ParticleQuality::Medium)));
}

TEST(ParticleSystemTest, SnowActivatesExactlyTheQualityCount) {
    ParticleSystem ps(/*seed=*/1);
    ps.advance(1.0f / 60.0f, WeatherState::Snow, ParticleQuality::Low, WindState{}, 0.0, 0.0, 0.0);
    EXPECT_EQ(ps.particles().size(), static_cast<std::size_t>(particle_quality_count(ParticleQuality::Low)));
}

TEST(ParticleSystemTest, PoolNeverExceedsTheConfiguredCapAcrossQualityChanges) {
    // S805 -- drive the pool through every quality tier, including High
    // (the cap itself), and confirm it never grows past kMaxParticles.
    ParticleSystem ps(/*seed=*/7);
    for (int i = 0; i < 300; ++i) {
        const ParticleQuality q = static_cast<ParticleQuality>(i % 3);
        ps.advance(1.0f / 60.0f, WeatherState::Rain, q, WindState{}, 0.0, 0.0, 0.0);
        EXPECT_LE(ps.particles().size(), static_cast<std::size_t>(ParticleSystem::kMaxParticles))
            << "iteration " << i;
    }
}

TEST(ParticleSystemTest, ParticlesStayWithinTheSpawnRadiusAndHeightBounds) {
    constexpr float kRadius = 60.0f;
    constexpr float kHeight = 40.0f;
    constexpr float kMinH   = -5.0f;
    ParticleSystem ps(/*seed=*/3, kRadius, kHeight, kMinH);
    for (int i = 0; i < 600; ++i) {
        ps.advance(0.05f, WeatherState::Rain, ParticleQuality::Medium, WindState{}, 0.0, 0.0, 0.0);
    }
    for (const Particle& p : ps.particles()) {
        const float horiz = std::sqrt(p.x * p.x + p.z * p.z);
        EXPECT_LE(horiz, kRadius + 1e-2f);
        EXPECT_GE(p.y, kMinH) << "a particle fell below min_height_m without being recycled";
        EXPECT_LE(p.y, kHeight + 4.5f) << "a particle spawned above the documented jitter range";
    }
}

TEST(ParticleSystemTest, RainParticlesEventuallyRecycleRatherThanFallingForever) {
    // Fall speed is fast enough that within a couple of lifetimes every
    // particle must have recycled at least once -- confirmed indirectly:
    // if recycling were broken (particles just kept falling unbounded),
    // Y would drift far below min_height_m, which the bounds test above
    // would already catch, but this test also checks lifetime itself never
    // goes negative by more than one frame's worth of dt (i.e. respawn()
    // always fires promptly, not accumulating a growing deficit).
    ParticleSystem ps(/*seed=*/11, /*spawn_radius_m=*/60.0f, /*spawn_height_m=*/40.0f,
                       /*min_height_m=*/-5.0f);
    constexpr float kDt = 0.1f;
    for (int i = 0; i < 2000; ++i) {
        ps.advance(kDt, WeatherState::Rain, ParticleQuality::Low, WindState{}, 0.0, 0.0, 0.0);
        for (const Particle& p : ps.particles()) {
            EXPECT_GT(p.lifetime_s, -kDt) << "iteration " << i;
        }
    }
}

TEST(ParticleSystemTest, SwitchingWeatherKindResetsToTheNewProfile) {
    // A particle that was mid-fall as rain must not keep a rain-speed
    // velocity once the weather has switched to snow -- confirms
    // respawn()'s own per-kind velocity assignment actually takes effect on
    // a weather change, not just for brand-new pool growth.
    ParticleSystem ps(/*seed=*/5);
    ps.advance(0.1f, WeatherState::Rain, ParticleQuality::Low, WindState{}, 0.0, 0.0, 0.0);
    ASSERT_FALSE(ps.particles().empty());
    for (const Particle& p : ps.particles()) EXPECT_FLOAT_EQ(p.vy, -ParticleSystem::kRainFallSpeedMPerS);

    ps.advance(0.1f, WeatherState::Snow, ParticleQuality::Low, WindState{}, 0.0, 0.0, 0.0);
    ASSERT_FALSE(ps.particles().empty());
    for (const Particle& p : ps.particles()) EXPECT_FLOAT_EQ(p.vy, -ParticleSystem::kSnowFallSpeedMPerS);
}

TEST(ParticleSystemTest, StoppingPrecipitationClearsTheActivePool) {
    ParticleSystem ps(/*seed=*/9);
    ps.advance(0.1f, WeatherState::Rain, ParticleQuality::Medium, WindState{}, 0.0, 0.0, 0.0);
    ASSERT_FALSE(ps.particles().empty());
    ps.advance(0.1f, WeatherState::Clear, ParticleQuality::Medium, WindState{}, 0.0, 0.0, 0.0);
    EXPECT_TRUE(ps.particles().empty());
}

TEST(ParticleSystemTest, CameraMovementReHomesParticlesToStayAnchoredInWorldSpace) {
    // Same floating-origin idea
    // WorldRendererTests.cpp's own ComputeVisiblePlacementInstancesAppliesFloatingOriginOffset
    // checks for placements: a particle's camera-relative position must
    // shift by the exact negative of the camera's own movement, so it
    // reads as fixed in world space.
    ParticleSystem ps(/*seed=*/13);
    ps.advance(0.0f, WeatherState::Rain, ParticleQuality::Low, WindState{}, /*cam_x=*/100.0, 0.0, /*cam_z=*/-50.0);
    ASSERT_FALSE(ps.particles().empty());
    const float x0 = ps.particles()[0].x;
    const float z0 = ps.particles()[0].z;

    // Move the camera +5 in X, -3 in Z, with dt=0 so velocity/lifetime
    // integration doesn't also perturb the position in this same step.
    ps.advance(0.0f, WeatherState::Rain, ParticleQuality::Low, WindState{}, 105.0, 0.0, -53.0);
    EXPECT_NEAR(ps.particles()[0].x, x0 - 5.0f, 1e-4f);
    EXPECT_NEAR(ps.particles()[0].z, z0 - (-3.0f), 1e-4f);
}

TEST(ParticleSystemTest, ZeroDtIsAPositionNoOpButStillAppliesPoolChanges) {
    ParticleSystem ps(/*seed=*/17);
    ps.advance(0.1f, WeatherState::Rain, ParticleQuality::Low, WindState{}, 0.0, 0.0, 0.0);
    ASSERT_FALSE(ps.particles().empty());
    const auto before = ps.particles();

    ps.advance(0.0f, WeatherState::Rain, ParticleQuality::Low, WindState{}, 0.0, 0.0, 0.0);
    ASSERT_EQ(ps.particles().size(), before.size());
    for (std::size_t i = 0; i < before.size(); ++i) {
        EXPECT_FLOAT_EQ(ps.particles()[i].x, before[i].x) << "i=" << i;
        EXPECT_FLOAT_EQ(ps.particles()[i].y, before[i].y) << "i=" << i;
        EXPECT_FLOAT_EQ(ps.particles()[i].z, before[i].z) << "i=" << i;
    }
}

TEST(ParticleSystemTest, SameSeedProducesTheSameInitialSpawnPositions) {
    ParticleSystem a(/*seed=*/2024);
    ParticleSystem b(/*seed=*/2024);
    a.advance(0.1f, WeatherState::Snow, ParticleQuality::Low, WindState{}, 0.0, 0.0, 0.0);
    b.advance(0.1f, WeatherState::Snow, ParticleQuality::Low, WindState{}, 0.0, 0.0, 0.0);
    ASSERT_EQ(a.particles().size(), b.particles().size());
    for (std::size_t i = 0; i < a.particles().size(); ++i) {
        EXPECT_FLOAT_EQ(a.particles()[i].x, b.particles()[i].x) << "i=" << i;
        EXPECT_FLOAT_EQ(a.particles()[i].y, b.particles()[i].y) << "i=" << i;
        EXPECT_FLOAT_EQ(a.particles()[i].z, b.particles()[i].z) << "i=" << i;
    }
}

// --- S904: wind-driven horizontal drift -----------------------------------

TEST(ParticleSystemTest, ZeroWindRainFallsStraightDown) {
    ParticleSystem ps(/*seed=*/1);
    ps.advance(0.1f, WeatherState::Rain, ParticleQuality::Low, WindState{0.0, 0.0f}, 0.0, 0.0, 0.0);
    ASSERT_FALSE(ps.particles().empty());
    for (const Particle& p : ps.particles()) {
        EXPECT_FLOAT_EQ(p.vx, 0.0f);
        EXPECT_FLOAT_EQ(p.vz, 0.0f);
    }
}

TEST(ParticleSystemTest, StrongWindGivesRainHorizontalVelocity) {
    ParticleSystem ps(/*seed=*/1);
    ps.advance(0.1f, WeatherState::Rain, ParticleQuality::Low, WindState{90.0, 1.0f}, 0.0, 0.0, 0.0);
    ASSERT_FALSE(ps.particles().empty());
    for (const Particle& p : ps.particles()) {
        const float horiz_speed = std::sqrt(p.vx * p.vx + p.vz * p.vz);
        EXPECT_NEAR(horiz_speed, ParticleSystem::kRainWindDriftMPerS, 1e-3f);
        // Wind toward due East (90 deg): x component should be positive.
        EXPECT_GT(p.vx, 0.0f);
    }
}

TEST(ParticleSystemTest, RainHorizontalSpeedScalesWithWindStrength) {
    ParticleSystem half(/*seed=*/1), full(/*seed=*/1);
    half.advance(0.1f, WeatherState::Rain, ParticleQuality::Low, WindState{0.0, 0.5f}, 0.0, 0.0, 0.0);
    full.advance(0.1f, WeatherState::Rain, ParticleQuality::Low, WindState{0.0, 1.0f}, 0.0, 0.0, 0.0);
    ASSERT_FALSE(half.particles().empty());
    ASSERT_FALSE(full.particles().empty());
    const float half_speed = std::sqrt(half.particles()[0].vx * half.particles()[0].vx +
                                        half.particles()[0].vz * half.particles()[0].vz);
    const float full_speed = std::sqrt(full.particles()[0].vx * full.particles()[0].vx +
                                        full.particles()[0].vz * full.particles()[0].vz);
    EXPECT_NEAR(full_speed, half_speed * 2.0f, 1e-3f);
}

TEST(ParticleSystemTest, SnowDriftIsBiasedTowardWindDirection) {
    // Over many spawns, the AVERAGE drift direction should be close to the
    // wind's own direction (jitter is symmetric, so it averages out) --
    // spot-checked via the mean vx/vz sign rather than every individual
    // particle (each one is individually jittered, see kSnowDriftJitterDeg).
    ParticleSystem ps(/*seed=*/42);
    ps.advance(0.1f, WeatherState::Snow, ParticleQuality::High, WindState{90.0, 1.0f}, 0.0, 0.0, 0.0);
    ASSERT_FALSE(ps.particles().empty());
    double sum_vx = 0.0, sum_vz = 0.0;
    for (const Particle& p : ps.particles()) {
        sum_vx += p.vx;
        sum_vz += p.vz;
    }
    const double mean_vx = sum_vx / static_cast<double>(ps.particles().size());
    const double mean_vz = sum_vz / static_cast<double>(ps.particles().size());
    // Wind toward due East (90 deg): mean drift should be predominantly +X,
    // near-zero Z (jitter symmetric around the wind axis).
    EXPECT_GT(mean_vx, 0.0);
    EXPECT_NEAR(mean_vz, 0.0, 0.3);
}
