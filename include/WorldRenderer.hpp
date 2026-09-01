// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include "CelestialPosition.hpp"  // SkyAngle
#include "Mc3Collision.hpp"
#include "ModelPlacement.hpp"
#include "ParticleSystem.hpp"  // Particle, ParticleSystem
#include "Weather.hpp"  // WeatherState
#include "WorldConfig.hpp"
#include "WorldMap.hpp"
#include "WorldStreamer.hpp"
#include "generators/map/PlanetGenerator.hpp"  // for Map::PlanetParams (complete type)
#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#ifdef MESH_WORLD_HAS_RENDERER
#include "Mc3Renderer.hpp"
#endif

namespace MeshWorld {

// M175/M176/M177 (MAP11) -- one placement ready to hand to a renderer:
// camera-relative float position with the floating-origin offset already
// applied, definition id, rotation, scale. Deliberately holds no
// MeshCraft/SDL types, so this (and the pure function that produces it
// below) compiles and is testable in the root build -- no GPU, no separate
// apps/mesh-world-app project needed. Only the final "hand this to
// Mc3Renderer" step needs the actual renderer, and stays behind
// MESH_WORLD_HAS_RENDERER exactly as chunk rendering already does.
struct PlacementInstance {
    std::string definition_id;
    float       x{0.0f}, y{0.0f}, z{0.0f};  // camera-relative (floating origin applied)
    float       rot_y{0.0f};
    float       scale{1.0f};
};

// M177 -- how far (meters) a placement at the given lod_min tier stays
// visible. Tier 0 (the only tier any generator sets today -- see
// ModelPlacement.hpp's own doc comment) is visible out to
// max_render_distance_m; each higher tier requires being progressively
// closer (halved per tier, floored at 10 m so nothing vanishes at
// point-blank range). Deliberately simple: no existing placement data uses
// lod_min > 0 yet, so this establishes the convention rather than matching
// a pre-existing one.
float placement_lod_visible_distance_m(int lod_min, float max_render_distance_m);

// M175/M176/M177 -- which of `placements` are close enough to
// (cam_x, cam_y, cam_z) -- true, double-precision, planet-scale world
// coordinates, NOT the renderer's float-precision FPCamera -- to render,
// with the floating-origin offset already applied (the cam-to-placement
// delta is computed in double, then narrowed to float only once it's
// already small, so this stays precise arbitrarily far from the planet's
// own origin). LOD-gated per placement_lod_visible_distance_m() above.
std::vector<PlacementInstance> compute_visible_placement_instances(
    double cam_x, double cam_y, double cam_z,
    const std::vector<ModelPlacement>& placements,
    float max_render_distance_m = 300.0f);

// R140 -- conservative horizontal frustum test for an entire square chunk.
// It rejects only chunks whose bounding circle lies wholly behind, outside
// the camera's horizontal field of view, or beyond the far plane. It does not
// attempt vertical culling, so looking up/down can never make nearby city
// geometry disappear incorrectly. Kept renderer-free for direct unit tests.
bool chunk_may_be_visible(float camera_x, float camera_z, float camera_yaw,
                          float fov_y, float aspect, float far_z,
                          ChunkCoord chunk, float chunk_size_m);

// S301/S302 -- where to draw the sun this frame (camera-relative, so it
// always reads as "at infinity" regardless of player position, same
// technique render_placements() already uses for its own floating-origin
// offset) and how bright/visible it should be. Pure logic, no MeshCraft/SDL
// types, same "testable without a renderer" split
// compute_visible_placement_instances() already established.
//
// Axis convention (this project has no other established "world north" to
// match, so this is a deliberate, documented choice): azimuth 0/North =
// -Z, 90/East = +X, 180/South = +Z, 270/West = -X; elevation is +Y (up).
struct SunRenderState {
    float x{0.0f}, y{0.0f}, z{0.0f};  // camera-relative position
    float brightness{0.0f};           // 0 = fully faded out, 1 = fully visible
};

// `sun` is CelestialPosition.hpp's own SkyAngle. `distance_m` places the sun
// sphere far enough away that it reads as "at infinity" regardless of the
// chunk/placement render distances used elsewhere. `fade_band_deg` is how
// many degrees above/below the horizon the brightness ramps linearly
// (S302's "no hard pop-in/pop-out") -- full brightness above
// +fade_band_deg, fully faded at/below -fade_band_deg.
SunRenderState compute_sun_render_state(SkyAngle sun, float distance_m = 5000.0f,
                                          float fade_band_deg = 10.0f);

// S401/S402/S403 -- where to draw the moon and its "eclipsing shadow
// sphere" (the no-texture phase trick: a dark sphere overlapping the moon
// disc, sized/positioned so only the illuminated crescent/gibbous shows
// through) this frame, and how bright/visible it should be. Pure logic,
// same "testable without a renderer" split as SunRenderState above.
struct MoonRenderState {
    float x{0.0f}, y{0.0f}, z{0.0f};                 // camera-relative moon disc position
    float shadow_x{0.0f}, shadow_y{0.0f}, shadow_z{0.0f};  // camera-relative shadow-sphere position
    float brightness{0.0f};              // 0 = fully invisible, 1 = fully visible
    float illuminated_fraction{0.0f};    // 0 = new moon (fully dark), 1 = full moon
};

// `moon`/`sun` are both CelestialPosition.hpp's SkyAngle (same axis
// convention as compute_sun_render_state() above). `phase_fraction` is
// CelestialPosition.hpp's own moon_phase_fraction() output, [0,1).
//
// `brightness` combines two independent fades, multiplied together:
//   - the moon's OWN horizon fade, identical shape to
//     compute_sun_render_state()'s (linear across `fade_band_deg` straddling
//     the moon's own horizon);
//   - a "washed out by daylight" dimming based on the SUN's elevation (S401's
//     "visible when sun is below horizon, faintly during day") -- fades
//     linearly from 1.0 (sun below horizon) down to a low but nonzero floor
//     once the sun climbs above `full_daylight_sun_elev_deg`, so the moon
//     stays faintly visible during the day rather than fully disappearing.
//
// The shadow sphere is offset from the moon disc perpendicular to the
// camera-to-moon view direction (a "right" vector via cross product with
// world-up), scaled by `illuminated_fraction * radius_m` -- 0 offset (fully
// overlapping, fully dark) at new moon, growing to fully clear of the disc
// (fully bright) at full moon. This is a manual version of the
// camera-facing billboard trick a 2D sprite would get for free, needed here
// because the moon is real 3D geometry (an Mc3Object icosphere), not a
// billboard.
MoonRenderState compute_moon_render_state(SkyAngle moon, SkyAngle sun, double phase_fraction,
                                            float distance_m = 5000.0f, float radius_m = 150.0f,
                                            float fade_band_deg = 10.0f,
                                            float full_daylight_sun_elev_deg = 30.0f,
                                            float daylight_brightness_floor = 0.15f);

// S502/S503 -- how many of a CelestialPosition.hpp generate_star_field()
// result should actually be drawn this frame, given the sun's current
// position. Returns a COUNT, not a filtered copy or per-star alpha: the
// caller draws the first N entries of the (deterministically-ordered) star
// field, so the visible set only ever grows/shrinks from one end as the sky
// darkens/brightens -- no flicker from re-sampling which specific stars are
// visible frame to frame. Visibility is the exact inverse of
// compute_sun_render_state()'s own S302 horizon fade (0 stars once the sun
// is at/above `fade_band_deg`, all `total_star_count` stars once the sun is
// at/below `-fade_band_deg`, linear in between) -- stars fade in exactly as
// the sun fades out.
int visible_star_count(int total_star_count, SkyAngle sun, float fade_band_deg = 10.0f);

// S701/S702/S703 -- one cloud "puff" cluster's camera-relative position,
// ready to render as a small group of overlapping spheres (S701's own
// "same primitive-composition technique ObjectDefinitionLibrary.cpp's
// existing objects already use"). Just a position -- the puff's internal
// multi-sphere shape and its light/dark material (by WeatherState) are
// rendering-layer detail (render_clouds()), not part of this pure struct.
struct CloudPuff {
    float x{0.0f}, y{0.0f}, z{0.0f};  // camera-relative position
};

// S702 -- how many puffs a given WeatherState should show: 0 for `Clear`,
// `sparse_count` for `PartlyCloudy`, `dense_count` for `Overcast`/`Rain`/
// `Snow` (matches the task's own "0 for clear, few for partly_cloudy,
// many/dense for overcast/rain/snow" wording).
int cloud_count_for_weather(WeatherState state, int sparse_count = 8, int dense_count = 24);

// S701/S702/S703 -- `cloud_count_for_weather(state)` puffs scattered around
// the camera at a fixed altitude (`altitude_m` above it) and within
// `scatter_radius_m` -- same "at infinity"/camera-relative, recomputed-
// every-frame technique compute_sun_render_state()/compute_moon_render_state()
// already use (see SunRenderState's own doc comment for the axis
// convention this reuses), so clouds always read as present regardless of
// how far the player has walked, never "running out" the way a finite
// persisted field would.
//
// Each puff sits at `angle_deg = index * kGoldenAngleDeg + drift_deg` /
// `radius_frac = sqrt((index+0.5)/count)` -- Vogel's disk-sampling method
// (the sunflower-seed spiral), a well-known technique for scattering N
// points evenly across a disk without clustering, using neither an RNG nor
// a stored seed (fully deterministic from index+count alone, unlike
// generate_star_field()'s own seeded approach -- clouds don't need
// cross-session reproducibility the way stars do).
//
// S703/S904 -- "clouds drift slowly across the sky over time"/"wind also
// drives cloud drift speed": `drift_deg = deg_per_hour_at_full_wind *
// wind.strength * total_game_hours`. Only `wind.strength` affects this --
// `wind.direction_deg` is deliberately NOT used here: the drift itself is a
// whole-field ROTATION (a pinwheel, not puffs translating independently),
// which has no natural "which way does a compass direction rotate it"
// mapping without an arbitrary, unmotivated convention -- simpler and more
// honest to only vary the rate, not invent a fake directional effect.
std::vector<CloudPuff> compute_cloud_puffs(WeatherState state, double total_game_hours, WindState wind,
                                             float altitude_m = 180.0f, float scatter_radius_m = 350.0f,
                                             float deg_per_hour_at_full_wind = 20.0f,
                                             int sparse_count = 8, int dense_count = 24);

// S902 -- one frame's tree-sway rotation override (Euler XYZ degrees, ready
// to hand to MeshCraft::Renderer::AnimOverride::rotation), driven by a sine
// wave whose amplitude scales with `wind.strength` -- zero wind means zero
// sway (S905's own explicit requirement). The sway axis is decomposed into
// Euler X/Z components from `wind.direction_deg` (same azimuth convention
// as SkyAngle/sky_angle_to_xyz: 0/North=-Z, 90/East=+X) so trees roughly
// lean along the wind's own direction rather than an arbitrary fixed axis
// -- AnimOverride's rotation is Euler, not axis-angle, so this is a
// decomposition, not a true single-axis tilt, a stylized approximation
// like every other S-series celestial/weather effect in this codebase.
std::array<float, 3> compute_tree_sway_rotation_deg(WindState wind, double total_game_hours,
                                                       float max_sway_deg = 8.0f,
                                                       float sway_cycles_per_hour = 40.0f);

// S1001 -- true if a chunk-embedded object's own name marks it as an
// outdoor-facing surface eligible for snow accumulation (S1002). Currently
// recognizes only the `"roof_"` prefix `SmallHouseBlockGenerator.cpp`
// already uses (confirmed via research before writing this, not assumed:
// no other chunk generator marks anything with a reusable outdoor-facing
// naming convention, and no interior-room generator exists ANYWHERE in
// this codebase to exclude in the first place -- S1004's own "explicitly
// excludes interiors" is satisfied vacuously today, not by new exclusion
// logic, since nothing interior is ever generated to accidentally cover).
// Tree/prop `ModelPlacement` instances (M175-177) are unconditionally
// outdoor by construction (documented since MAP11 as "trees/props/etc.",
// never interior furniture) -- this function is only needed for chunk-
// embedded box geometry, which mixes walls/houses/cliffs/roofs together.
//
// KNOWN V1 LIMITATION: other generators' rooftops (e.g.
// `ApartmentBlockGenerator`) aren't tagged with any recognizable prefix
// today, so they don't get snow in this pass -- extending this to more
// generators is later, straightforward work (tag more object names), not
// attempted here to keep this task's own scope honest and bounded.
bool is_snow_eligible_chunk_object_name(const std::string& name);

// Combines WorldStreamer with optional MeshCraft rendering.
// update() drives background chunk loading/unloading.
// render() (only available when MESH_WORLD_HAS_RENDERER is defined) draws
// all currently-loaded chunks via Mc3Renderer using the given camera.
class WorldRenderer {
public:
    // chunk_cache_dir: directory where chunk .mc3.xml files are read from
    // (populated externally by MeshWorldExport, or written by the app).
    // map_world_dir/map_params: forwarded straight to WorldStreamer's own
    // constructor (see its doc comment) — empty map_world_dir (default)
    // means no map layer, identical to this class's behavior before.
    WorldRenderer(const WorldConfig& cfg,
                  const WorldMap& map,
                  std::string chunk_cache_dir,
                  int load_radius = 3,
                  std::string map_world_dir = "",
                  Map::PlanetParams map_params = {});
    WorldRenderer(const WorldConfig& cfg,
                  const WorldMap& map,
                  std::string chunk_cache_dir,
                  int load_radius,
                  std::string map_world_dir,
                  Map::PlanetParams map_params,
                  std::size_t cache_max_entries);
    ~WorldRenderer();

    // Drive streaming: loads/unloads chunks around world position (wx, wz).
    // Non-blocking — actual loading happens on background threads.
    void update(float wx, float wz);

    // Number of chunks currently in LOADED state.
    int loaded_chunk_count() const;

    // Chunk coordinates that are currently loaded.
    std::vector<ChunkCoord> loaded_coords() const;

#ifdef MESH_WORLD_HAS_RENDERER
    // Render all loaded chunks. Loads Mc3Document from chunk_cache_dir on
    // first access per chunk; parsed documents are bounded to the currently
    // resident streaming set rather than growing through a long traversal.
    void render(Mc3Renderer& renderer, const FPCamera& cam);

    // M175/M176/M177 -- render `placements` (typically
    // Model3DStreamer::loaded_placements()) as instances: builds one
    // synthetic, never-persisted Mc3Document per call from
    // compute_visible_placement_instances()'s already-filtered,
    // already-camera-relative result, resolving each definition_id via
    // ObjectDefinitionLibrary exactly as chunk-embedded instances already
    // do (inject_definitions()). `cam_x/cam_y/cam_z` is the player's true
    // double-precision world position (not `cam`'s own float fields, which
    // aren't planet-scale-precise) -- `cam` itself is only used for the
    // projection/view matrix, same as render() above.
    void render_placements(Mc3Renderer& renderer, const FPCamera& cam,
                            double cam_x, double cam_y, double cam_z,
                            const std::vector<ModelPlacement>& placements);

    // S301/S302 -- renders the sun as a single bright sphere at
    // compute_sun_render_state()'s own camera-relative position, fading out
    // near/below the horizon. A no-op when brightness is fully zero (sun
    // fully faded out), so callers can call this unconditionally every
    // frame without their own visibility check. Same "build a small,
    // never-persisted synthetic Mc3Document, render from the origin"
    // technique render_placements() already uses.
    void render_sun(Mc3Renderer& renderer, const FPCamera& cam, SkyAngle sun);

    // S401/S402/S403 -- renders the moon as two overlapping spheres (the
    // disc + its eclipsing shadow) at compute_moon_render_state()'s own
    // camera-relative positions. A no-op when brightness is fully zero, same
    // "callers don't need their own visibility check" contract render_sun()
    // has. `phase_fraction` is CelestialPosition.hpp's own
    // moon_phase_fraction() output.
    void render_moon(Mc3Renderer& renderer, const FPCamera& cam, SkyAngle moon, SkyAngle sun,
                      double phase_fraction);

    // S501/S502/S503 -- renders `stars` (typically a world-load-time
    // generate_star_field() result, held by the caller so it's only ever
    // generated once) as small spheres, camera-relative like render_sun()/
    // render_moon(). Only draws visible_star_count()'s own first N entries
    // -- a no-op (nothing drawn) once that count is zero -- so callers can
    // call this unconditionally every frame without their own visibility
    // check, same contract render_sun()/render_moon() already have.
    void render_stars(Mc3Renderer& renderer, const FPCamera& cam,
                       const std::vector<SkyAngle>& stars, SkyAngle sun);

    // S701/S702/S703/S904 -- renders compute_cloud_puffs()'s own puffs as
    // small groups of overlapping spheres (S701's "handful of overlapping
    // spheres/icospheres" wording), light-grey normally or dark-grey for
    // `Overcast`/`Rain`/`Snow`. A no-op when `state == WeatherState::Clear`
    // (0 puffs), so callers can call this unconditionally every frame
    // without their own visibility check, same contract render_sun()/
    // render_moon()/render_stars() already have. `wind` drives drift speed
    // (S904) -- see compute_cloud_puffs()'s own doc comment.
    void render_clouds(Mc3Renderer& renderer, const FPCamera& cam, WeatherState state,
                        double total_game_hours, WindState wind);

    // S801/S802/S803 -- renders `particles` (typically a
    // ParticleSystem::particles() result, already camera-relative -- see
    // its own doc comment) as small shapes: thin vertical streaks
    // ("rain_streak" material) for rain, small spheres ("snow_flake"
    // material) for snow. A no-op when `particles` is empty (e.g.
    // WeatherState::Clear), so callers can call this unconditionally every
    // frame without their own visibility check, same contract render_sun()/
    // render_clouds()/etc. already have. `state` picks which of the two
    // shapes/materials to use -- ParticleSystem itself only tracks
    // position/velocity/lifetime, not which effect they belong to.
    void render_particles(Mc3Renderer& renderer, const FPCamera& cam,
                           const std::vector<Particle>& particles, WeatherState state);

    // S902/S903 -- pushes compute_tree_sway_rotation_deg()'s own rotation
    // onto every "trunk"/"canopy" sub-object via
    // Mc3Renderer::scene_renderer().setAnimOverrides() (see that method's
    // own doc comment: replaces the FULL override map on the next draw()
    // call, so this must be called once per frame, unconditionally, not
    // just when wind is nonzero -- a zero-wind call still needs to push a
    // zero-rotation override to clear out the previous frame's nonzero one).
    //
    // KNOWN V1 LIMITATION (confirmed via SceneRenderer.cpp:608-619 before
    // writing this, not assumed): AnimOverride is keyed by name against ONE
    // shared entry, applied to EVERY object anywhere in the document with
    // that name -- so every "trunk"/"canopy" object in the whole scene
    // (every tree instance, of every species, everywhere) sways in lockstep/
    // in-phase, not independently. A real per-instance phase offset needs
    // extending SceneRenderer's own instance-drawing path to key overrides
    // by instance identity, not just name -- a separate, larger, cross-repo
    // (mesh-craft) task, not attempted here unless asked (matches S203/S303's
    // own "ask before crossing into mesh-craft" precedent).
    void apply_tree_sway(Mc3Renderer& renderer, WindState wind, double total_game_hours);

    // S1002 -- renders `snow_depth`-scaled snow-cap overlays (S1001's own
    // "additive extra geometry, no new shader work"): thin white boxes on
    // top of every currently-loaded chunk's `is_snow_eligible_chunk_object_name()`
    // objects (reads the SAME `g_doc_cache` `nearby_collision_boxes()`
    // already does, same locking pattern), plus a small white sphere near
    // the top of every visible `placements` instance (tree canopies/
    // ground-level props -- see this method's own "top height" doc comment
    // below for the stylized approximation used since a placement's true
    // per-definition geometry height isn't available at this layer).
    // `cam_x/cam_y/cam_z` is the player's TRUE, double-precision world
    // position (same reasoning render_placements()/nearby_collision_boxes()
    // already use). A no-op when `snow_depth <= 0`, so callers can call
    // this unconditionally every frame without their own visibility check,
    // same contract render_sun()/render_clouds()/etc. already have.
    //
    // Top height for a placement's own snow cap is approximated as
    // `kApproxOutdoorObjectHeightM * placement.scale` above its base
    // position -- a stylized simplification (PlacementInstance carries no
    // real geometry height), same bar every other S-series celestial/
    // weather effect in this codebase already sets.
    static constexpr float kApproxOutdoorObjectHeightM = 4.0f;
    void render_snow_accumulation(Mc3Renderer& renderer, const FPCamera& cam, double cam_x, double cam_y,
                                   double cam_z, float snow_depth,
                                   const std::vector<ModelPlacement>& placements,
                                   float radius_m = 150.0f);

    // Evict the Mc3Document cache (call when chunk_cache_dir contents change).
    void clear_doc_cache();

    // R133 -- inline collision="box" geometry plus resolved instances whose
    // assetMetadata explicitly declares collisionProxy="box", from chunks
    // currently loaded within `radius_m` of (wx, wz), in world-space meters.
    // Skips objects shorter than `min_height_m`, so thin curbs/road markings
    // remain passable. Reads the SAME Mc3Document cache render() populates;
    // a chunk never rendered (not yet loaded) contributes nothing.
    std::vector<CollisionBox> nearby_collision_boxes(double wx, double wz, float radius_m,
                                                       float min_height_m = 0.3f) const;
#endif

private:
    const WorldConfig& cfg_;
    std::unique_ptr<WorldStreamer> streamer_;
    std::string chunk_cache_dir_;

    // XML strings received from the WorldStreamer callback (coord → xml).
    mutable std::mutex xml_mutex_;
    std::unordered_map<ChunkCoord, std::string> chunk_xml_;

    void on_chunk_loaded(const LoadedChunk& lc);
};

} // namespace MeshWorld
