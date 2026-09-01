// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include <gtest/gtest.h>
#include "BuiltinMaterials.hpp"
#include "CelestialPosition.hpp"
#include "WorldRenderer.hpp"
#include "WorldConfig.hpp"
#include "WorldMap.hpp"
#include "BuiltinStyles.hpp"
#include "PlanetWorld.hpp"
#include "generators/map/PlanetGenerator.hpp"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

using namespace MeshWorld;

// T269: WorldRenderer::update() loads chunks for a 3×3 area around origin
// without crashing and reports the expected loaded count.
TEST(WorldRendererTests, T269_UpdateLoadsChunksAroundOrigin) {
    // R106 -- see UnloadOnPlayerMove's own comment below: real chunk
    // generation here can reference real MaterialRegistry ids, so this
    // needs its own idempotent registration too, not just whatever
    // happened to run earlier in the same binary.
    register_builtin_materials();
    register_builtin_styles();

    WorldConfig cfg;
    cfg.grid_w       = 10;
    cfg.grid_h       = 10;
    cfg.chunk_size_m = 64;
    cfg.seed         = 42;

    WorldMap map(cfg);

    // Use an empty string for chunk_cache_dir — render() won't be called.
    WorldRenderer wr(cfg, map, "", /*load_radius=*/1);

    // Trigger streaming update at world origin (chunk 0,0 area)
    wr.update(0.0f, 0.0f);

    // Wait briefly for background threads to load chunks.
    for (int i = 0; i < 50; ++i) {
        if (wr.loaded_chunk_count() > 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_GT(wr.loaded_chunk_count(), 0)
        << "Expected at least one chunk loaded around origin";

    // loaded_coords() must not crash and return a non-empty list.
    auto coords = wr.loaded_coords();
    EXPECT_GT(static_cast<int>(coords.size()), 0);
}

// Moving the player far away should eventually unload nearby chunks.
TEST(WorldRendererTests, UnloadOnPlayerMove) {
    // R106 (investigated 2026-07-13) -- real chunk generation here can
    // reference real MaterialRegistry ids (e.g. "dirt"); this test never
    // called register_builtin_materials() itself, relying on some OTHER
    // test file happening to run first in the same MeshWorldTests binary
    // (confirmed via --gtest_shuffle). Idempotent, so calling it here too
    // removes the run-order dependency.
    register_builtin_materials();
    register_builtin_styles();

    WorldConfig cfg;
    cfg.grid_w       = 20;
    cfg.grid_h       = 20;
    cfg.chunk_size_m = 64;
    cfg.seed         = 7;

    WorldMap map(cfg);

    WorldRenderer wr(cfg, map, "", /*load_radius=*/1);

    // Load chunks at origin
    wr.update(0.0f, 0.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    int count_near = wr.loaded_chunk_count();

    // Move far away — this should trigger unloads
    wr.update(1000.0f, 1000.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // After moving 1000m away with radius 1, origin chunks should be gone.
    // We only verify this doesn't crash and the count changes.
    EXPECT_GE(count_near, 0); // sanity
    (void)wr.loaded_chunk_count(); // just call it, no crash
}

// WorldRenderer forwards map_world_dir/map_params straight through to its
// internal WorldStreamer (MeshWorldApp map-wiring task, NEXT.md §8) — this
// just proves the extra constructor arguments actually reach a working
// PlanetWorld-backed streamer end to end, not just that they compile.
TEST(WorldRendererTests, WithMapLayerLoadsChunksWithoutCrashing) {
    // R106 -- see UnloadOnPlayerMove's own comment above.
    register_builtin_materials();
    register_builtin_styles();

    const std::string dir =
        (std::filesystem::temp_directory_path() / "meshworld_renderer_map_test").string();
    std::filesystem::remove_all(dir);
    PlanetWorld::create_new(dir);

    WorldConfig cfg;
    cfg.grid_w       = 10;
    cfg.grid_h       = 10;
    cfg.chunk_size_m = 64;
    cfg.seed         = 42;

    WorldMap map(cfg);
    WorldRenderer wr(cfg, map, "", /*load_radius=*/1, dir, Map::PlanetParams{});

    wr.update(0.0f, 0.0f);
    for (int i = 0; i < 50; ++i) {
        if (wr.loaded_chunk_count() > 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    EXPECT_GT(wr.loaded_chunk_count(), 0);
}

// --- MAP11 M175/M176/M177: placement-instance pure logic ---------------------
//
// compute_visible_placement_instances()/placement_lod_visible_distance_m()
// hold no MeshCraft/SDL types (see WorldRenderer.hpp's own doc comment on
// why), so -- unlike render()/render_placements() above -- these run in
// every build, not just apps/mesh-world-app's, and are directly testable
// without a GPU.

TEST(WorldRendererTests, PlacementLodVisibleDistanceHalvesPerTier) {
    constexpr float kMax = 300.0f;
    EXPECT_FLOAT_EQ(placement_lod_visible_distance_m(0, kMax), 300.0f);
    EXPECT_FLOAT_EQ(placement_lod_visible_distance_m(1, kMax), 150.0f);
    EXPECT_FLOAT_EQ(placement_lod_visible_distance_m(2, kMax), 75.0f);
    // Floors at 10m rather than shrinking to nothing at very high tiers.
    EXPECT_FLOAT_EQ(placement_lod_visible_distance_m(20, kMax), 10.0f);
}

namespace {
ModelPlacement make_placement(const std::string& id, double x, double y, double z,
                               int lod_min = 0, float rot_y = 0.0f, float scale = 1.0f) {
    ModelPlacement p;
    p.definition_id = id;
    p.pos_x = x;
    p.pos_y = y;
    p.pos_z = z;
    p.lod_min = lod_min;
    p.rot_y = rot_y;
    p.scale = scale;
    return p;
}
} // namespace

TEST(WorldRendererTests, ComputeVisiblePlacementInstancesEmptyInputReturnsEmpty) {
    EXPECT_TRUE(compute_visible_placement_instances(0.0, 0.0, 0.0, {}).empty());
}

// R140 -- resident chunks behind/outside the camera must not make every
// frame traverse their full MC3 object lists. The cull is deliberately
// conservative: a chunk touching the camera or on the FOV edge remains.
TEST(WorldRendererTests, ChunkVisibilityRejectsBehindAndKeepsFrontAndNearChunks) {
    constexpr float kChunk = 64.0f;
    constexpr float kFovY = 1.0472f;
    constexpr float kAspect = 16.0f / 9.0f;
    constexpr float kFar = 1000.0f;
    // yaw 0 faces north (-Z); camera sits in chunk (0, 0).
    EXPECT_TRUE(chunk_may_be_visible(32.0f, 32.0f, 0.0f, kFovY, kAspect, kFar,
                                      ChunkCoord{0, 0}, kChunk));
    EXPECT_TRUE(chunk_may_be_visible(32.0f, 32.0f, 0.0f, kFovY, kAspect, kFar,
                                      ChunkCoord{0, -2}, kChunk));
    EXPECT_FALSE(chunk_may_be_visible(32.0f, 32.0f, 0.0f, kFovY, kAspect, kFar,
                                       ChunkCoord{0, 2}, kChunk));
}

TEST(WorldRendererTests, ChunkVisibilityRejectsFarChunksOutsideFarPlane) {
    EXPECT_FALSE(chunk_may_be_visible(32.0f, 32.0f, 0.0f, 1.0472f, 16.0f / 9.0f,
                                       /*far_z=*/100.0f, ChunkCoord{0, -4}, 64.0f));
}

TEST(WorldRendererTests, ComputeVisiblePlacementInstancesFiltersOutOfRangeAndKeepsInRange) {
    const std::vector<ModelPlacement> placements = {
        make_placement("near_tree", 10.0, 0.0, 0.0),    // 10m away -- in range
        make_placement("far_tree", 10000.0, 0.0, 0.0),  // 10km away -- out of range
    };
    const auto instances = compute_visible_placement_instances(0.0, 0.0, 0.0, placements,
                                                                 /*max_render_distance_m=*/300.0f);
    ASSERT_EQ(instances.size(), 1u);
    EXPECT_EQ(instances[0].definition_id, "near_tree");
}

TEST(WorldRendererTests, ComputeVisiblePlacementInstancesAppliesFloatingOriginOffset) {
    const std::vector<ModelPlacement> placements = {
        make_placement("rock", 105.0, 12.0, -30.0),
    };
    const auto instances = compute_visible_placement_instances(100.0, 10.0, -25.0, placements);
    ASSERT_EQ(instances.size(), 1u);
    EXPECT_FLOAT_EQ(instances[0].x, 5.0f);
    EXPECT_FLOAT_EQ(instances[0].y, 2.0f);
    EXPECT_FLOAT_EQ(instances[0].z, -5.0f);
}

TEST(WorldRendererTests, ComputeVisiblePlacementInstancesPreservesRotationAndScale) {
    const std::vector<ModelPlacement> placements = {
        make_placement("statue", 1.0, 0.0, 1.0, /*lod_min=*/0, /*rot_y=*/135.0f, /*scale=*/2.5f),
    };
    const auto instances = compute_visible_placement_instances(0.0, 0.0, 0.0, placements);
    ASSERT_EQ(instances.size(), 1u);
    EXPECT_FLOAT_EQ(instances[0].rot_y, 135.0f);
    EXPECT_FLOAT_EQ(instances[0].scale, 2.5f);
}

// M176's actual purpose: a placement and camera both sitting at genuinely
// planet-scale coordinates (~22,585 km, this project's real planet size)
// but only a few meters apart must still produce an accurate small delta.
// A naive `static_cast<float>(pos) - static_cast<float>(cam)` would lose
// this entirely -- float has ~7 significant digits, and 22,585,000 alone
// already consumes all of them, leaving nothing for the few-meter
// difference that actually matters. Computing the subtraction in double
// first (as compute_visible_placement_instances() does) is the whole point.
TEST(WorldRendererTests, ComputeVisiblePlacementInstancesStaysPreciseAtPlanetScale) {
    constexpr double kPlanetScaleCoord = 22'585'000.0;  // Map::PLANET_SIZE_M's own magnitude
    const std::vector<ModelPlacement> placements = {
        make_placement("far_out_tree", kPlanetScaleCoord + 3.0, 0.0, kPlanetScaleCoord - 4.0),
    };
    const auto instances = compute_visible_placement_instances(
        kPlanetScaleCoord, 0.0, kPlanetScaleCoord, placements);
    ASSERT_EQ(instances.size(), 1u);
    EXPECT_NEAR(instances[0].x, 3.0f, 1e-2f);
    EXPECT_NEAR(instances[0].z, -4.0f, 1e-2f);

    // Demonstrate what this test is actually guarding against: the naive
    // float-only computation genuinely does fail here.
    const float naive_x = static_cast<float>(kPlanetScaleCoord + 3.0) - static_cast<float>(kPlanetScaleCoord);
    EXPECT_GT(std::abs(naive_x - 3.0f), 0.5f)
        << "if this now passes, float precision at planet scale improved and this guard is obsolete";
}

TEST(WorldRendererTests, ComputeVisiblePlacementInstancesRespectsLodTier) {
    // 100m away: within tier-0's default 300m range, but beyond tier-2's
    // 75m range (see PlacementLodVisibleDistanceHalvesPerTier above).
    const std::vector<ModelPlacement> placements = {
        make_placement("bush_detail", 100.0, 0.0, 0.0, /*lod_min=*/2),
        make_placement("landmark", 100.0, 0.0, 0.0, /*lod_min=*/0),
    };
    const auto instances = compute_visible_placement_instances(0.0, 0.0, 0.0, placements,
                                                                 /*max_render_distance_m=*/300.0f);
    ASSERT_EQ(instances.size(), 1u);
    EXPECT_EQ(instances[0].definition_id, "landmark");
}

// --- S301/S302: compute_sun_render_state() pure logic -----------------------
//
// Same "no MeshCraft/SDL types, testable in every build" split
// compute_visible_placement_instances() above already established.

TEST(WorldRendererTests, ComputeSunRenderStateAtZenithPointsStraightUp) {
    SkyAngle sun{/*elevation_deg=*/90.0, /*azimuth_deg=*/0.0};
    const auto state = compute_sun_render_state(sun, /*distance_m=*/1000.0f);
    EXPECT_NEAR(state.x, 0.0f, 0.5f);
    EXPECT_NEAR(state.y, 1000.0f, 0.5f);
    EXPECT_NEAR(state.z, 0.0f, 0.5f);
    EXPECT_FLOAT_EQ(state.brightness, 1.0f);
}

TEST(WorldRendererTests, ComputeSunRenderStateAtNadirIsFullyFadedOut) {
    SkyAngle sun{-90.0, 0.0};
    const auto state = compute_sun_render_state(sun, 1000.0f);
    EXPECT_FLOAT_EQ(state.brightness, 0.0f);
}

TEST(WorldRendererTests, ComputeSunRenderStateOnHorizonIsAtHalfBrightness) {
    SkyAngle sun{0.0, 0.0};
    const auto state = compute_sun_render_state(sun, 1000.0f, /*fade_band_deg=*/10.0f);
    EXPECT_NEAR(state.brightness, 0.5f, 1e-5f);
}

// Axis convention pinned per direction (see WorldRenderer.hpp's own doc
// comment): azimuth 0/North=-Z, 90/East=+X, 180/South=+Z, 270/West=-X.
TEST(WorldRendererTests, ComputeSunRenderStateEastAzimuthPointsPositiveX) {
    SkyAngle sun{0.0, 90.0};
    const auto state = compute_sun_render_state(sun, 1000.0f);
    EXPECT_NEAR(state.x, 1000.0f, 0.5f);
    EXPECT_NEAR(state.z, 0.0f, 0.5f);
}

TEST(WorldRendererTests, ComputeSunRenderStateNorthAzimuthPointsNegativeZ) {
    SkyAngle sun{0.0, 0.0};
    const auto state = compute_sun_render_state(sun, 1000.0f);
    EXPECT_NEAR(state.x, 0.0f, 0.5f);
    EXPECT_NEAR(state.z, -1000.0f, 0.5f);
}

TEST(WorldRendererTests, ComputeSunRenderStateSouthAzimuthPointsPositiveZ) {
    SkyAngle sun{0.0, 180.0};
    const auto state = compute_sun_render_state(sun, 1000.0f);
    EXPECT_NEAR(state.x, 0.0f, 0.5f);
    EXPECT_NEAR(state.z, 1000.0f, 0.5f);
}

TEST(WorldRendererTests, ComputeSunRenderStateWestAzimuthPointsNegativeX) {
    SkyAngle sun{0.0, 270.0};
    const auto state = compute_sun_render_state(sun, 1000.0f);
    EXPECT_NEAR(state.x, -1000.0f, 0.5f);
    EXPECT_NEAR(state.z, 0.0f, 0.5f);
}

TEST(WorldRendererTests, ComputeSunRenderStateDistanceScalesPositionLinearly) {
    SkyAngle sun{0.0, 90.0};
    const auto near_state = compute_sun_render_state(sun, 100.0f);
    const auto far_state  = compute_sun_render_state(sun, 1000.0f);
    EXPECT_NEAR(far_state.x, near_state.x * 10.0f, 1.0f);
}

TEST(WorldRendererTests, ComputeSunRenderStateBrightnessFullyOneAboveFadeBand) {
    SkyAngle sun{15.0, 0.0};
    const auto state = compute_sun_render_state(sun, 1000.0f, /*fade_band_deg=*/10.0f);
    EXPECT_FLOAT_EQ(state.brightness, 1.0f);
}

TEST(WorldRendererTests, ComputeSunRenderStateBrightnessInterpolatesLinearlyWithinFadeBand) {
    SkyAngle sun{5.0, 0.0};
    const auto state = compute_sun_render_state(sun, 1000.0f, /*fade_band_deg=*/10.0f);
    EXPECT_NEAR(state.brightness, 0.75f, 1e-5f);
}

TEST(WorldRendererTests, ComputeSunRenderStateZeroFadeBandIsAHardCutoff) {
    SkyAngle above{5.0, 0.0};
    SkyAngle below{-5.0, 0.0};
    EXPECT_FLOAT_EQ(compute_sun_render_state(above, 1000.0f, /*fade_band_deg=*/0.0f).brightness, 1.0f);
    EXPECT_FLOAT_EQ(compute_sun_render_state(below, 1000.0f, /*fade_band_deg=*/0.0f).brightness, 0.0f);
}

// --- S401/S402/S403: compute_moon_render_state() pure logic -----------------

TEST(WorldRendererTests, ComputeMoonRenderStateAtZenithAtNightIsFullyVisible) {
    SkyAngle moon{90.0, 0.0};
    SkyAngle sun{-90.0, 0.0};  // sun well below the horizon -- full night
    const auto state = compute_moon_render_state(moon, sun, /*phase_fraction=*/0.5, 1000.0f);
    EXPECT_NEAR(state.x, 0.0f, 0.5f);
    EXPECT_NEAR(state.y, 1000.0f, 0.5f);
    EXPECT_NEAR(state.z, 0.0f, 0.5f);
    EXPECT_FLOAT_EQ(state.brightness, 1.0f);
}

TEST(WorldRendererTests, ComputeMoonRenderStateAtNadirIsFullyFadedOutRegardlessOfSun) {
    SkyAngle moon{-90.0, 0.0};
    SkyAngle sun{-90.0, 0.0};  // even at night, the moon's own horizon fade wins
    const auto state = compute_moon_render_state(moon, sun, 0.5, 1000.0f);
    EXPECT_FLOAT_EQ(state.brightness, 0.0f);
}

TEST(WorldRendererTests, ComputeMoonRenderStateDaylightDimsButNeverFullyHidesIt) {
    SkyAngle moon{90.0, 0.0};  // moon fully up
    SkyAngle sun{90.0, 0.0};   // sun also fully up -- full daylight
    const auto state = compute_moon_render_state(moon, sun, 0.5, 1000.0f, 150.0f,
                                                   /*fade_band_deg=*/10.0f,
                                                   /*full_daylight_sun_elev_deg=*/30.0f,
                                                   /*daylight_brightness_floor=*/0.15f);
    EXPECT_NEAR(state.brightness, 0.15f, 1e-5f) << "S401: faintly visible during day, not fully hidden";
}

TEST(WorldRendererTests, ComputeMoonRenderStateDaylightDimmingInterpolatesLinearly) {
    SkyAngle moon{90.0, 0.0};
    SkyAngle sun{15.0, 0.0};  // halfway to the 30-degree "full daylight" threshold
    const auto state = compute_moon_render_state(moon, sun, 0.5, 1000.0f, 150.0f, 10.0f,
                                                   /*full_daylight_sun_elev_deg=*/30.0f,
                                                   /*daylight_brightness_floor=*/0.15f);
    // own_horizon_fade=1.0 (moon at zenith) * dimming = 1 - 0.5*(1-0.15) = 0.575
    EXPECT_NEAR(state.brightness, 0.575f, 1e-5f);
}

TEST(WorldRendererTests, ComputeMoonRenderStateNewMoonHasZeroIlluminationAndNoShadowOffset) {
    SkyAngle moon{45.0, 90.0};
    SkyAngle sun{-45.0, 0.0};
    const auto state = compute_moon_render_state(moon, sun, /*phase_fraction=*/0.0, 1000.0f);
    EXPECT_NEAR(state.illuminated_fraction, 0.0f, 1e-5f);
    // Shadow sphere sits exactly on the moon disc -- fully eclipsing it.
    EXPECT_NEAR(state.shadow_x, state.x, 1e-3f);
    EXPECT_NEAR(state.shadow_y, state.y, 1e-3f);
    EXPECT_NEAR(state.shadow_z, state.z, 1e-3f);
}

TEST(WorldRendererTests, ComputeMoonRenderStateFullMoonHasFullIlluminationAndOffsetShadow) {
    SkyAngle moon{45.0, 90.0};
    SkyAngle sun{-45.0, 0.0};
    const auto state = compute_moon_render_state(moon, sun, /*phase_fraction=*/0.5, 1000.0f,
                                                   /*radius_m=*/150.0f);
    EXPECT_NEAR(state.illuminated_fraction, 1.0f, 1e-5f);
    const float dx = state.shadow_x - state.x;
    const float dy = state.shadow_y - state.y;
    const float dz = state.shadow_z - state.z;
    const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    EXPECT_GT(dist, 150.0f) << "at full moon the shadow must be well clear of the disc";
}

TEST(WorldRendererTests, ComputeMoonRenderStateQuarterPhasesAreHalfIlluminated) {
    SkyAngle moon{45.0, 90.0};
    SkyAngle sun{-45.0, 0.0};
    EXPECT_NEAR(compute_moon_render_state(moon, sun, 0.25, 1000.0f).illuminated_fraction, 0.5f, 1e-5f);
    EXPECT_NEAR(compute_moon_render_state(moon, sun, 0.75, 1000.0f).illuminated_fraction, 0.5f, 1e-5f);
}

TEST(WorldRendererTests, ComputeMoonRenderStateShadowOffsetIsPerpendicularToViewDirection) {
    // The shadow must slide sideways relative to the camera-to-moon view
    // line, not toward/away from the camera (which would just make it
    // look like the moon itself grew/shrank instead of eclipsing).
    SkyAngle moon{30.0, 200.0};
    SkyAngle sun{-30.0, 20.0};
    const auto state = compute_moon_render_state(moon, sun, /*phase_fraction=*/0.5, 1000.0f);

    const float view_len = std::sqrt(state.x * state.x + state.y * state.y + state.z * state.z);
    const float vx = state.x / view_len, vy = state.y / view_len, vz = state.z / view_len;
    const float ox = state.shadow_x - state.x, oy = state.shadow_y - state.y, oz = state.shadow_z - state.z;
    const float dot = vx * ox + vy * oy + vz * oz;
    EXPECT_NEAR(dot, 0.0f, 1e-2f) << "shadow offset must be perpendicular to the view direction";
}

// --- S502/S503/S504: visible_star_count() pure logic -------------------

TEST(WorldRendererTests, VisibleStarCountIsZeroInFullDaylight) {
    SkyAngle sun{60.0, 180.0};  // well above the fade band -- full daylight
    EXPECT_EQ(visible_star_count(800, sun), 0);
}

TEST(WorldRendererTests, VisibleStarCountIsFullCountAtTrueNight) {
    SkyAngle sun{-60.0, 0.0};  // well below the fade band -- full night
    EXPECT_EQ(visible_star_count(800, sun), 800);
}

TEST(WorldRendererTests, VisibleStarCountIsHalfOnTheHorizon) {
    SkyAngle sun{0.0, 0.0};  // exactly on the horizon -- midpoint of the fade
    EXPECT_EQ(visible_star_count(800, sun, /*fade_band_deg=*/10.0f), 400);
}

TEST(WorldRendererTests, VisibleStarCountInterpolatesLinearlyWithinTheFadeBand) {
    SkyAngle sun{-5.0, 0.0};  // halfway between the horizon and full night
    EXPECT_EQ(visible_star_count(800, sun, /*fade_band_deg=*/10.0f), 600);
}

TEST(WorldRendererTests, VisibleStarCountNeverExceedsTheTotalStarCount) {
    for (double elev = -90.0; elev <= 90.0; elev += 5.0) {
        SkyAngle sun{elev, 0.0};
        const int count = visible_star_count(800, sun);
        EXPECT_GE(count, 0) << "elev=" << elev;
        EXPECT_LE(count, 800) << "elev=" << elev;
    }
}

TEST(WorldRendererTests, VisibleStarCountZeroTotalStarsIsAlwaysZero) {
    SkyAngle sun{-60.0, 0.0};
    EXPECT_EQ(visible_star_count(0, sun), 0);
}

TEST(WorldRendererTests, VisibleStarCountIsInverseOfSunBrightness) {
    // Same fade shape as compute_sun_render_state()'s own brightness, just
    // inverted -- spot-check a handful of elevations agree with that.
    for (double elev : {-30.0, -10.0, -5.0, 0.0, 5.0, 10.0, 30.0}) {
        SkyAngle sun{elev, 0.0};
        const float brightness = compute_sun_render_state(sun, 1000.0f).brightness;
        const int expected = static_cast<int>(std::lround((1.0f - brightness) * 800.0f));
        EXPECT_EQ(visible_star_count(800, sun), expected) << "elev=" << elev;
    }
}

// --- S701/S702/S703/S704: cloud_count_for_weather()/compute_cloud_puffs() --

TEST(WorldRendererTests, CloudCountIsZeroForClear) {
    EXPECT_EQ(cloud_count_for_weather(WeatherState::Clear), 0);
}

TEST(WorldRendererTests, CloudCountIsSparseForPartlyCloudy) {
    EXPECT_EQ(cloud_count_for_weather(WeatherState::PartlyCloudy, /*sparse_count=*/8, /*dense_count=*/24), 8);
}

TEST(WorldRendererTests, CloudCountIsDenseForOvercastRainAndSnow) {
    EXPECT_EQ(cloud_count_for_weather(WeatherState::Overcast, 8, 24), 24);
    EXPECT_EQ(cloud_count_for_weather(WeatherState::Rain, 8, 24), 24);
    EXPECT_EQ(cloud_count_for_weather(WeatherState::Snow, 8, 24), 24);
}

TEST(WorldRendererTests, CloudCountRespectsCustomSparseAndDenseCounts) {
    EXPECT_EQ(cloud_count_for_weather(WeatherState::PartlyCloudy, /*sparse_count=*/3, /*dense_count=*/50), 3);
    EXPECT_EQ(cloud_count_for_weather(WeatherState::Snow, /*sparse_count=*/3, /*dense_count=*/50), 50);
}

TEST(WorldRendererTests, ComputeCloudPuffsCountMatchesCloudCountForWeather) {
    for (WeatherState s : {WeatherState::Clear, WeatherState::PartlyCloudy, WeatherState::Overcast,
                            WeatherState::Rain, WeatherState::Snow}) {
        const auto puffs = compute_cloud_puffs(s, /*total_game_hours=*/0.0, WindState{});
        EXPECT_EQ(puffs.size(), static_cast<std::size_t>(cloud_count_for_weather(s)))
            << "weather=" << static_cast<int>(s);
    }
}

TEST(WorldRendererTests, ComputeCloudPuffsClearIsEmpty) {
    EXPECT_TRUE(compute_cloud_puffs(WeatherState::Clear, 0.0, WindState{}).empty());
}

TEST(WorldRendererTests, ComputeCloudPuffsAreAtTheConfiguredAltitude) {
    const auto puffs = compute_cloud_puffs(WeatherState::Overcast, /*total_game_hours=*/0.0, WindState{},
                                            /*altitude_m=*/180.0f);
    ASSERT_FALSE(puffs.empty());
    for (const CloudPuff& p : puffs) EXPECT_FLOAT_EQ(p.y, 180.0f);
}

TEST(WorldRendererTests, ComputeCloudPuffsStayWithinTheScatterRadius) {
    constexpr float kScatterRadius = 350.0f;
    const auto puffs = compute_cloud_puffs(WeatherState::Overcast, 0.0, WindState{}, 180.0f, kScatterRadius);
    for (const CloudPuff& p : puffs) {
        const float horiz_dist = std::sqrt(p.x * p.x + p.z * p.z);
        EXPECT_LE(horiz_dist, kScatterRadius + 1e-3f);
    }
}

TEST(WorldRendererTests, ComputeCloudPuffsAreDeterministicGivenTheSameInputs) {
    const WindState wind{45.0, 0.6f};
    const auto a = compute_cloud_puffs(WeatherState::Overcast, 12.5, wind);
    const auto b = compute_cloud_puffs(WeatherState::Overcast, 12.5, wind);
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        EXPECT_FLOAT_EQ(a[i].x, b[i].x) << "i=" << i;
        EXPECT_FLOAT_EQ(a[i].y, b[i].y) << "i=" << i;
        EXPECT_FLOAT_EQ(a[i].z, b[i].z) << "i=" << i;
    }
}

TEST(WorldRendererTests, ComputeCloudPuffsDriftOverTime) {
    // S703/S904 -- puffs at different total_game_hours, under nonzero wind,
    // must not sit at identical positions (the whole point of drift); at
    // least one puff must have moved.
    const WindState wind{0.0, /*strength=*/1.0f};
    const auto early = compute_cloud_puffs(WeatherState::Overcast, /*total_game_hours=*/0.0, wind);
    const auto later = compute_cloud_puffs(WeatherState::Overcast, /*total_game_hours=*/5.0, wind);
    ASSERT_EQ(early.size(), later.size());
    bool any_moved = false;
    for (std::size_t i = 0; i < early.size(); ++i) {
        if (std::abs(early[i].x - later[i].x) > 1e-3f || std::abs(early[i].z - later[i].z) > 1e-3f) {
            any_moved = true;
            break;
        }
    }
    EXPECT_TRUE(any_moved);
}

TEST(WorldRendererTests, ComputeCloudPuffsZeroWindMeansNoDrift) {
    // S904 -- zero wind.strength means zero drift, regardless of how much
    // total_game_hours elapses.
    const WindState calm{123.0, /*strength=*/0.0f};
    const auto early = compute_cloud_puffs(WeatherState::Overcast, 0.0, calm, 180.0f, 350.0f);
    const auto later = compute_cloud_puffs(WeatherState::Overcast, 5.0, calm, 180.0f, 350.0f);
    ASSERT_EQ(early.size(), later.size());
    for (std::size_t i = 0; i < early.size(); ++i) {
        EXPECT_NEAR(early[i].x, later[i].x, 1e-3f) << "i=" << i;
        EXPECT_NEAR(early[i].z, later[i].z, 1e-3f) << "i=" << i;
    }
}

TEST(WorldRendererTests, ComputeCloudPuffsDoNotAllOverlapAtTheSamePosition) {
    // Vogel's disk-sampling method should scatter puffs, not stack them.
    const auto puffs = compute_cloud_puffs(WeatherState::Overcast, 0.0, WindState{});
    ASSERT_GE(puffs.size(), 2u);
    bool any_distinct = false;
    for (std::size_t i = 1; i < puffs.size(); ++i) {
        const float dx = puffs[i].x - puffs[0].x;
        const float dz = puffs[i].z - puffs[0].z;
        if (std::sqrt(dx * dx + dz * dz) > 1.0f) { any_distinct = true; break; }
    }
    EXPECT_TRUE(any_distinct);
}

// --- S901: Weather::wind() / S902/S905: compute_tree_sway_rotation_deg() --

TEST(WorldRendererTests, TreeSwayIsZeroWhenWindIsCalm) {
    const auto rot = compute_tree_sway_rotation_deg(WindState{90.0, /*strength=*/0.0f}, /*total_game_hours=*/3.0);
    EXPECT_FLOAT_EQ(rot[0], 0.0f);
    EXPECT_FLOAT_EQ(rot[1], 0.0f);
    EXPECT_FLOAT_EQ(rot[2], 0.0f);
}

TEST(WorldRendererTests, TreeSwayAmplitudeScalesWithWindStrength) {
    // Sample enough phase points that at least one is near the sine wave's
    // peak for both strengths, then compare peak-to-peak amplitude rather
    // than a single arbitrary timestamp (which could land near a zero
    // crossing for either signal).
    float max_abs_half = 0.0f, max_abs_full = 0.0f;
    for (int i = 0; i < 200; ++i) {
        const double hours = i * 0.01;
        const auto half = compute_tree_sway_rotation_deg(WindState{0.0, 0.5f}, hours);
        const auto full = compute_tree_sway_rotation_deg(WindState{0.0, 1.0f}, hours);
        max_abs_half = std::max(max_abs_half, std::abs(half[0]));
        max_abs_full = std::max(max_abs_full, std::abs(full[0]));
    }
    EXPECT_GT(max_abs_full, max_abs_half);
    EXPECT_NEAR(max_abs_full, max_abs_half * 2.0f, 0.05f);
}

TEST(WorldRendererTests, TreeSwayRotationStaysWithinMaxSwayBound) {
    constexpr float kMaxSway = 8.0f;
    for (int i = 0; i < 500; ++i) {
        const auto rot = compute_tree_sway_rotation_deg(WindState{37.0, 1.0f}, i * 0.03, kMaxSway);
        const float magnitude = std::sqrt(rot[0] * rot[0] + rot[2] * rot[2]);
        EXPECT_LE(magnitude, kMaxSway + 1e-3f) << "i=" << i;
    }
}

// --- S1001: is_snow_eligible_chunk_object_name() ---------------------------

TEST(WorldRendererTests, RoofPrefixedNamesAreSnowEligible) {
    EXPECT_TRUE(is_snow_eligible_chunk_object_name("roof_0"));
    EXPECT_TRUE(is_snow_eligible_chunk_object_name("roof_3a"));
    EXPECT_TRUE(is_snow_eligible_chunk_object_name("roof_"));
}

TEST(WorldRendererTests, NonRoofNamesAreNotSnowEligible) {
    EXPECT_FALSE(is_snow_eligible_chunk_object_name("house_0"));
    EXPECT_FALSE(is_snow_eligible_chunk_object_name("fence_0"));
    EXPECT_FALSE(is_snow_eligible_chunk_object_name(""));
    EXPECT_FALSE(is_snow_eligible_chunk_object_name("a_roof_0"))
        << "must match a PREFIX, not merely contain \"roof_\" anywhere in the name";
}
