// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "WorldRenderer.hpp"
#include <algorithm>
#include <cmath>

#ifdef MESH_WORLD_HAS_RENDERER
#include "Mc3Renderer.hpp"
#include "MaterialRegistry.hpp"
#include "ObjectDefinitionLibrary.hpp"
#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Material.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>
#include <MeshCraft/Mc3/Mc3Texture.hpp>
#include <MeshCraft/Renderer/SceneRenderer.hpp>
#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#endif

namespace MeshWorld {

// M175/M176/M177 -- pure logic, no MeshCraft/SDL types, always compiled and
// tested regardless of MESH_WORLD_HAS_RENDERER (see WorldRenderer.hpp's own
// doc comment on why this split exists).

namespace {
// Shared by compute_sun_render_state()/compute_moon_render_state()/
// render_stars() -- converts a SkyAngle into a camera-relative XYZ position
// at `distance_m`, per the axis convention documented on SunRenderState
// (WorldRenderer.hpp): azimuth 0/North = -Z, 90/East = +X, elevation = +Y.
// Factored out once render_stars() (S501-S504) became the 3rd
// near-identical use of this exact conversion.
void sky_angle_to_xyz(SkyAngle angle, float distance_m, float& out_x, float& out_y, float& out_z) {
    constexpr double kDeg2Rad = 3.14159265358979323846 / 180.0;
    const double elevation_rad = angle.elevation_deg * kDeg2Rad;
    const double azimuth_rad   = angle.azimuth_deg * kDeg2Rad;
    const double horizontal    = std::cos(elevation_rad);
    out_x = static_cast<float>(distance_m * horizontal * std::sin(azimuth_rad));
    out_y = static_cast<float>(distance_m * std::sin(elevation_rad));
    out_z = static_cast<float>(-distance_m * horizontal * std::cos(azimuth_rad));
}
} // namespace

float placement_lod_visible_distance_m(int lod_min, float max_render_distance_m) {
    float d = max_render_distance_m;
    for (int i = 0; i < lod_min; ++i) d *= 0.5f;
    return std::max(d, 10.0f);
}

std::vector<PlacementInstance> compute_visible_placement_instances(
    double cam_x, double cam_y, double cam_z,
    const std::vector<ModelPlacement>& placements,
    float max_render_distance_m)
{
    std::vector<PlacementInstance> out;
    out.reserve(placements.size());

    for (const ModelPlacement& p : placements) {
        // M176 -- floating-origin offset computed in double (the operands
        // can be planet-scale, up to ~22,585 km from the origin) so the
        // subtraction itself never loses precision; only the already-small
        // result is narrowed to float.
        const double dx = p.pos_x - cam_x;
        const double dy = p.pos_y - cam_y;
        const double dz = p.pos_z - cam_z;

        // M177 -- LOD gate: cull before doing anything else with this
        // placement once it's farther than its own tier's visible distance.
        const double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist > placement_lod_visible_distance_m(p.lod_min, max_render_distance_m)) continue;

        PlacementInstance inst;
        inst.definition_id = p.definition_id;
        inst.x             = static_cast<float>(dx);
        inst.y             = static_cast<float>(dy);
        inst.z             = static_cast<float>(dz);
        inst.rot_y         = p.rot_y;
        inst.scale         = p.scale;
        out.push_back(std::move(inst));
    }

    return out;
}

bool chunk_may_be_visible(float camera_x, float camera_z, float camera_yaw,
                          float fov_y, float aspect, float far_z,
                          ChunkCoord chunk, float chunk_size_m) {
    if (chunk_size_m <= 0.0f || fov_y <= 0.0f || aspect <= 0.0f || far_z <= 0.0f)
        return true;  // Invalid inputs must not hide the world.

    const float radius = chunk_size_m * 0.70710678f;  // half diagonal
    const float center_x = (static_cast<float>(chunk.x) + 0.5f) * chunk_size_m;
    const float center_z = (static_cast<float>(chunk.y) + 0.5f) * chunk_size_m;
    const float dx = center_x - camera_x;
    const float dz = center_z - camera_z;
    const float distance = std::sqrt(dx * dx + dz * dz);
    if (distance - radius > far_z) return false;
    if (distance <= radius) return true;  // Camera is inside/touching chunk.

    const float forward_x = std::sin(camera_yaw);
    const float forward_z = -std::cos(camera_yaw);
    const float forward = dx * forward_x + dz * forward_z;
    if (forward + radius < 0.0f) return false;

    const float horizontal_half_fov = std::atan(std::tan(fov_y * 0.5f) * aspect);
    const float sideways = std::abs(dx * -forward_z + dz * forward_x);
    return sideways <= forward * std::tan(horizontal_half_fov) + radius;
}

SunRenderState compute_sun_render_state(SkyAngle sun, float distance_m, float fade_band_deg) {
    SunRenderState state;
    sky_angle_to_xyz(sun, distance_m, state.x, state.y, state.z);

    if (fade_band_deg <= 0.0f) {
        state.brightness = sun.elevation_deg > 0.0 ? 1.0f : 0.0f;
    } else if (sun.elevation_deg >= fade_band_deg) {
        state.brightness = 1.0f;
    } else if (sun.elevation_deg <= -fade_band_deg) {
        state.brightness = 0.0f;
    } else {
        state.brightness = static_cast<float>((sun.elevation_deg + fade_band_deg) / (2.0 * fade_band_deg));
    }
    return state;
}

MoonRenderState compute_moon_render_state(SkyAngle moon, SkyAngle sun, double phase_fraction,
                                            float distance_m, float radius_m, float fade_band_deg,
                                            float full_daylight_sun_elev_deg,
                                            float daylight_brightness_floor) {
    constexpr double kPi = 3.14159265358979323846;

    MoonRenderState state;
    sky_angle_to_xyz(moon, distance_m, state.x, state.y, state.z);

    // The moon's own horizon fade -- identical shape to
    // compute_sun_render_state()'s.
    float own_horizon_fade;
    if (fade_band_deg <= 0.0f) {
        own_horizon_fade = moon.elevation_deg > 0.0 ? 1.0f : 0.0f;
    } else if (moon.elevation_deg >= fade_band_deg) {
        own_horizon_fade = 1.0f;
    } else if (moon.elevation_deg <= -fade_band_deg) {
        own_horizon_fade = 0.0f;
    } else {
        own_horizon_fade = static_cast<float>((moon.elevation_deg + fade_band_deg) / (2.0 * fade_band_deg));
    }

    // S401 -- "faintly during day": the sun washes the moon out as it
    // climbs higher, fading down to a low but nonzero floor rather than
    // fully invisible.
    float daylight_dimming;
    if (full_daylight_sun_elev_deg <= 0.0f) {
        daylight_dimming = sun.elevation_deg > 0.0 ? daylight_brightness_floor : 1.0f;
    } else if (sun.elevation_deg <= 0.0) {
        daylight_dimming = 1.0f;
    } else if (sun.elevation_deg >= full_daylight_sun_elev_deg) {
        daylight_dimming = daylight_brightness_floor;
    } else {
        const double t = sun.elevation_deg / full_daylight_sun_elev_deg;
        daylight_dimming = static_cast<float>(1.0 - t * (1.0 - daylight_brightness_floor));
    }

    state.brightness = own_horizon_fade * daylight_dimming;

    // S402/S403 -- illuminated_fraction follows the standard
    // 0(new)->0.5(full)->1(new) cosine curve.
    state.illuminated_fraction =
        static_cast<float>((1.0 - std::cos(2.0 * kPi * phase_fraction)) / 2.0);

    if (state.brightness <= 0.0f) {
        state.shadow_x = state.x;
        state.shadow_y = state.y;
        state.shadow_z = state.z;
        return state;
    }

    // Offset the shadow sphere perpendicular to the camera-to-moon view
    // direction (a "right" vector via cross product with world-up), so it
    // reads as a crescent/gibbous shape regardless of where the moon
    // currently sits in the sky -- a manual version of the camera-facing
    // billboard trick a 2D sprite would get for free, needed here because
    // the moon is real 3D geometry (an Mc3Object icosphere), not a
    // billboard.
    float right_x = 1.0f, right_y = 0.0f, right_z = 0.0f;  // fallback if the view vector is degenerate
    const float view_len =
        std::sqrt(state.x * state.x + state.y * state.y + state.z * state.z);
    if (view_len > 1e-4f) {
        const float vx = state.x / view_len;
        const float vy = state.y / view_len;
        const float vz = state.z / view_len;
        // A horizontal vector perpendicular to the view direction (the
        // negation of cross(view, world_up=(0,1,0)); which exact sign/
        // handedness is "right" doesn't matter -- this project has no
        // established left/right lunar convention to match, see
        // side_sign's own comment below). Degenerates to zero only when
        // the view is exactly vertical (moon directly overhead/underfoot),
        // an edge case not worth a special branch for a stylized effect.
        (void)vy;
        const float rx = vz;
        const float rz = -vx;
        const float rlen = std::sqrt(rx * rx + rz * rz);
        if (rlen > 1e-4f) {
            right_x = rx / rlen;
            right_y = 0.0f;
            right_z = rz / rlen;
        }
    }

    // 0 at new moon (shadow centered on the disc, fully covering it) ->
    // grows as the moon brightens, until it's well clear of the disc at
    // full moon (nothing left to shade). side_sign just varies the sweep
    // direction between the waxing/waning halves of the cycle -- this
    // project has no established left/right lunar convention to match
    // (CelestialPosition.hpp is deliberately hemisphere-agnostic), so
    // either direction is an equally valid, consistent choice.
    const float side_sign = (phase_fraction < 0.5) ? -1.0f : 1.0f;
    const float offset     = state.illuminated_fraction * radius_m * 2.2f * side_sign;
    state.shadow_x = state.x + right_x * offset;
    state.shadow_y = state.y + right_y * offset;
    state.shadow_z = state.z + right_z * offset;

    return state;
}

int visible_star_count(int total_star_count, SkyAngle sun, float fade_band_deg) {
    if (total_star_count <= 0) return 0;

    // Reuses compute_sun_render_state()'s own S302 fade shape rather than
    // duplicating it -- star visibility is deliberately defined as "exactly
    // the inverse of how bright the sun currently is". distance_m is
    // irrelevant here (only .brightness is used), so pass a throwaway value.
    const float sun_brightness = compute_sun_render_state(sun, /*distance_m=*/1.0f, fade_band_deg).brightness;
    const float star_visibility = 1.0f - sun_brightness;

    const int count = static_cast<int>(std::lround(star_visibility * static_cast<double>(total_star_count)));
    return std::clamp(count, 0, total_star_count);
}

int cloud_count_for_weather(WeatherState state, int sparse_count, int dense_count) {
    switch (state) {
        case WeatherState::Clear: return 0;
        case WeatherState::PartlyCloudy: return std::max(sparse_count, 0);
        case WeatherState::Overcast:
        case WeatherState::Rain:
        case WeatherState::Snow: return std::max(dense_count, 0);
    }
    return 0;
}

std::vector<CloudPuff> compute_cloud_puffs(WeatherState state, double total_game_hours, WindState wind,
                                             float altitude_m, float scatter_radius_m,
                                             float deg_per_hour_at_full_wind, int sparse_count,
                                             int dense_count) {
    const int count = cloud_count_for_weather(state, sparse_count, dense_count);

    std::vector<CloudPuff> puffs;
    puffs.reserve(static_cast<std::size_t>(count));
    if (count <= 0) return puffs;

    constexpr double kDeg2Rad     = 3.14159265358979323846 / 180.0;
    constexpr float  kGoldenAngleDeg = 137.50776f;  // Vogel's disk-sampling method -- see this
                                                     // function's own doc comment (WorldRenderer.hpp)
    // S904 -- only wind.strength affects the drift rate; direction is
    // deliberately unused (see this function's own doc comment).
    const double drift_deg = deg_per_hour_at_full_wind * wind.strength * total_game_hours;

    for (int i = 0; i < count; ++i) {
        const float  radius_frac = std::sqrt((static_cast<float>(i) + 0.5f) / static_cast<float>(count));
        const double angle_rad   = (static_cast<double>(i) * kGoldenAngleDeg + drift_deg) * kDeg2Rad;

        CloudPuff puff;
        puff.x = scatter_radius_m * radius_frac * static_cast<float>(std::sin(angle_rad));
        puff.y = altitude_m;
        puff.z = -scatter_radius_m * radius_frac * static_cast<float>(std::cos(angle_rad));
        puffs.push_back(puff);
    }
    return puffs;
}

std::array<float, 3> compute_tree_sway_rotation_deg(WindState wind, double total_game_hours,
                                                       float max_sway_deg, float sway_cycles_per_hour) {
    constexpr double kPi      = 3.14159265358979323846;
    constexpr double kDeg2Rad = kPi / 180.0;

    // S905 -- zero wind strength means zero sway, exactly (not just
    // "small") -- multiplying by wind.strength (which can legitimately be
    // 0.0f) already guarantees this, no separate branch needed.
    const double phase    = 2.0 * kPi * sway_cycles_per_hour * total_game_hours;
    const float  sway_deg = max_sway_deg * wind.strength * static_cast<float>(std::sin(phase));

    // Decompose into Euler X/Z components from the wind direction (same
    // azimuth convention sky_angle_to_xyz() uses) -- see this function's
    // own doc comment (WorldRenderer.hpp) on why this is an approximation,
    // not a true single-axis tilt.
    const double dir_rad = wind.direction_deg * kDeg2Rad;
    const float  rx      = sway_deg * static_cast<float>(std::cos(dir_rad));
    const float  rz      = sway_deg * static_cast<float>(std::sin(dir_rad));
    return {rx, 0.0f, rz};
}

bool is_snow_eligible_chunk_object_name(const std::string& name) {
    return name.rfind("roof_", 0) == 0;  // starts-with
}

WorldRenderer::WorldRenderer(const WorldConfig& cfg,
                             const WorldMap& map,
                             std::string chunk_cache_dir,
                             int load_radius,
                             std::string map_world_dir,
                             Map::PlanetParams map_params)
    : WorldRenderer(cfg, map, std::move(chunk_cache_dir), load_radius,
                    std::move(map_world_dir), std::move(map_params), 0) {}

WorldRenderer::WorldRenderer(const WorldConfig& cfg,
                             const WorldMap& map,
                             std::string chunk_cache_dir,
                             int load_radius,
                             std::string map_world_dir,
                             Map::PlanetParams map_params,
                             std::size_t cache_max_entries)
    : cfg_(cfg)
    , streamer_(std::make_unique<WorldStreamer>(cfg, map, load_radius, 2, chunk_cache_dir,
                                                std::move(map_world_dir), std::move(map_params),
                                                cache_max_entries))
    , chunk_cache_dir_(std::move(chunk_cache_dir))
{
    streamer_->set_chunk_loaded_callback(
        [this](const LoadedChunk& lc) { on_chunk_loaded(lc); });
}

WorldRenderer::~WorldRenderer() {
    streamer_->shutdown();
}

void WorldRenderer::update(float wx, float wz) {
    streamer_->update(wx, wz);
}

int WorldRenderer::loaded_chunk_count() const {
    return streamer_->loaded_count();
}

std::vector<ChunkCoord> WorldRenderer::loaded_coords() const {
    std::lock_guard<std::mutex> lk(xml_mutex_);
    std::vector<ChunkCoord> out;
    out.reserve(chunk_xml_.size());
    for (const auto& [coord, _] : chunk_xml_)
        out.push_back(coord);
    return out;
}

void WorldRenderer::on_chunk_loaded(const LoadedChunk& lc) {
    if (lc.state == ChunkState::LOADED) {
        std::lock_guard<std::mutex> lk(xml_mutex_);
        chunk_xml_[lc.coord] = lc.xml;
    } else if (lc.state == ChunkState::UNLOADED) {
        std::lock_guard<std::mutex> lk(xml_mutex_);
        chunk_xml_.erase(lc.coord);
    }
}

#ifdef MESH_WORLD_HAS_RENDERER

namespace {
std::mutex             g_doc_cache_mutex;
std::unordered_map<ChunkCoord, MeshCraft::Mc3::Mc3Document> g_doc_cache;

// Inject MaterialRegistry colors into an Mc3Document so SceneRenderer
// uses our palette instead of the default grey.
void inject_materials(MeshCraft::Mc3::Mc3Document& doc) {
    auto& reg = MaterialRegistry::instance();
    for (const auto& entry : reg.all()) {
        if (doc.materials.count(entry.id)) continue;
        MeshCraft::Mc3::Mc3Material mat(
            entry.id,
            {entry.r, entry.g, entry.b, 1.0f},
            entry.roughness,
            entry.metallic);
        if (!entry.texture_uri.empty()) {
            mat.baseColorTexture = entry.texture_uri;
            if (!doc.textures.count(entry.texture_uri))
                doc.textures[entry.texture_uri] =
                    MeshCraft::Mc3::Mc3Texture(entry.texture_uri, entry.texture_uri);
        }
        doc.materials[entry.id] = std::move(mat);
    }
}

} // namespace

void WorldRenderer::render(Mc3Renderer& renderer, const FPCamera& cam) {
    std::vector<ChunkCoord> coords = loaded_coords();

    // R140 -- bound the parsed-MC3 cache to the streamer's resident set.
    // Chunks are intentionally kept around briefly for movement smoothness,
    // but a long walk must not retain every historical city's CPU-side mesh
    // document indefinitely. The app owns one active WorldRenderer at a time.
    {
        const std::unordered_set<ChunkCoord> resident(coords.begin(), coords.end());
        std::lock_guard<std::mutex> lk(g_doc_cache_mutex);
        for (auto it = g_doc_cache.begin(); it != g_doc_cache.end();) {
            if (!resident.contains(it->first))
                it = g_doc_cache.erase(it);
            else
                ++it;
        }
    }

    for (const auto& coord : coords) {
        // R140 -- streaming deliberately keeps nearby chunks resident for
        // smooth movement, but they do not all belong in every draw call.
        // Cull before filesystem/cache work so a chunk behind the player
        // cannot trigger a parse or a SceneRenderer traversal this frame.
        if (!chunk_may_be_visible(cam.x, cam.z, cam.yaw, cam.fov_y, cam.aspect,
                                  cam.far_z, coord, cfg_.chunk_size_m))
            continue;
        std::string filename = coord.to_string() + ".mc3.xml";
        std::filesystem::path xml_path =
            std::filesystem::path(chunk_cache_dir_) / filename;

        if (!std::filesystem::exists(xml_path)) continue;

        MeshCraft::Mc3::Mc3Document* doc_ptr = nullptr;
        {
            std::lock_guard<std::mutex> lk(g_doc_cache_mutex);
            auto it = g_doc_cache.find(coord);
            if (it == g_doc_cache.end()) {
                auto doc = MeshCraft::Mc3::Mc3Document::loadFromFile(xml_path);
                inject_materials(doc);
                resolve_instance_definitions(doc);
                g_doc_cache.emplace(coord, std::move(doc));
                it = g_doc_cache.find(coord);
            }
            doc_ptr = &it->second;
        }

        if (doc_ptr) {
            FPCamera local_cam = cam;
            local_cam.x -= static_cast<float>(coord.x * cfg_.chunk_size_m);
            local_cam.z -= static_cast<float>(coord.y * cfg_.chunk_size_m);
            renderer.render(*doc_ptr, local_cam);
        }
    }
}

void WorldRenderer::render_placements(Mc3Renderer& renderer, const FPCamera& cam,
                                       double cam_x, double cam_y, double cam_z,
                                       const std::vector<ModelPlacement>& placements) {
    const std::vector<PlacementInstance> instances =
        compute_visible_placement_instances(cam_x, cam_y, cam_z, placements);
    if (instances.empty()) return;

    // Never persisted -- rebuilt fresh from the already-filtered,
    // already-camera-relative instance list every call. Placement counts
    // near the player are small (M177's own LOD gate + Model3DStreamer's
    // own 3D proximity radius already bound this), so this isn't the kind
    // of per-frame cost render()'s own chunk-document cache exists to avoid.
    MeshCraft::Mc3::Mc3Document doc;
    doc.objects.reserve(instances.size());
    for (const PlacementInstance& inst : instances) {
        auto obj = std::make_shared<MeshCraft::Mc3::Mc3Object>();
        obj->type       = MeshCraft::Mc3::ObjectType::Instance;
        obj->definition = inst.definition_id;
        obj->transform  = MeshCraft::Mc3::Mc3Transform::atRotated(
                              inst.x, inst.y, inst.z, 0.0f, inst.rot_y, 0.0f)
                              .withUniformScale(inst.scale);
        doc.objects.push_back(std::move(obj));
    }
    inject_materials(doc);
    resolve_instance_definitions(doc);

    // Placement positions above are already camera-relative (M176's
    // floating-origin offset, computed in double against the player's true
    // world position) -- so this document must be rendered from the
    // origin, not from `cam`'s own x/y/z (which are neither planet-scale
    // precise nor in this document's coordinate space). Only cam's
    // orientation/projection fields (yaw/pitch/fov/near/far/aspect) still
    // apply, same as chunk rendering's own local_cam trick above.
    FPCamera local_cam = cam;
    local_cam.x = 0.0f;
    local_cam.y = 0.0f;
    local_cam.z = 0.0f;
    renderer.render(doc, local_cam);
}

void WorldRenderer::render_sun(Mc3Renderer& renderer, const FPCamera& cam, SkyAngle sun) {
    // Distance/radius are stylized, not physically accurate (a realistic
    // ~0.5-degree angular sun would be a barely visible speck at any
    // sensible render distance) -- dramatic on purpose, matching
    // CelestialPosition.hpp/SkyColor.hpp's own "simple, not astronomically
    // accurate" choices.
    constexpr float kSunDistanceM = 5000.0f;
    constexpr float kSunRadiusM   = kSunDistanceM * 0.05f;

    const SunRenderState state = compute_sun_render_state(sun, kSunDistanceM);
    if (state.brightness <= 0.0f) return;  // fully faded below the horizon -- nothing to draw

    // No confirmed alpha-blending support in this renderer (Mc3Material's
    // own color has an alpha channel, but SceneRenderer was never observed
    // to set up a blend pass for it -- not something to assume without a
    // GPU to actually verify). Shrinking the sphere toward the horizon
    // fade band instead of trying to fade its transparency is a simple,
    // honestly-scoped stand-in that doesn't depend on that unconfirmed
    // capability -- S3xx polish (a real glow/fade shader) is later,
    // separate work if the shrink doesn't look right in practice.
    const float radius = kSunRadiusM * state.brightness;

    MeshCraft::Mc3::Mc3Document doc;
    auto sphere = MeshCraft::Mc3::Mc3Object::makeIcoSphere("sun", radius, /*subdivisions=*/2, "sun_glow");
    sphere->at(state.x, state.y, state.z);
    doc.objects.push_back(std::move(sphere));
    inject_materials(doc);

    FPCamera local_cam = cam;
    local_cam.x = 0.0f;
    local_cam.y = 0.0f;
    local_cam.z = 0.0f;
    // Real bug found via user report + screenshot (2026-07-11): FPCamera's
    // own far_z defaults to 1000 m, well short of kSunDistanceM (5000 m) --
    // the sun sphere was being clipped by the far plane and never actually
    // rendered at all; what the user saw instead was a pre-existing
    // specular highlight on the water from this app's already-existing
    // fixed lighting, not this function's own output. `cam`'s far_z is
    // tuned for terrain/placement rendering, not for a synthetic
    // "at infinity" sky object -- this document is its own tiny, isolated
    // draw call, so extending far_z here has zero effect on anything else.
    local_cam.far_z = kSunDistanceM * 1.5f;
    renderer.render(doc, local_cam);
}

void WorldRenderer::render_moon(Mc3Renderer& renderer, const FPCamera& cam, SkyAngle moon,
                                 SkyAngle sun, double phase_fraction) {
    // Same stylized distance/radius reasoning as render_sun() -- a bit
    // smaller than the sun's own 250 m for a deliberate visual size
    // difference, not a physically accurate ratio.
    constexpr float kMoonDistanceM = 5000.0f;
    constexpr float kMoonRadiusM   = 150.0f;

    const MoonRenderState state =
        compute_moon_render_state(moon, sun, phase_fraction, kMoonDistanceM, kMoonRadiusM);
    if (state.brightness <= 0.0f) return;  // fully faded -- nothing to draw

    // Same "shrink toward the fade band instead of fading transparency"
    // stand-in render_sun() already uses (no confirmed alpha-blending
    // support in this renderer). Both spheres shrink together so the
    // phase-shadow geometry stays proportionally correct at any brightness.
    const float moon_radius   = kMoonRadiusM * state.brightness;
    const float shadow_radius = moon_radius * 1.05f;  // slightly larger for clean full occlusion at new moon

    MeshCraft::Mc3::Mc3Document doc;
    auto moon_sphere =
        MeshCraft::Mc3::Mc3Object::makeIcoSphere("moon", moon_radius, /*subdivisions=*/2, "moon_glow");
    moon_sphere->at(state.x, state.y, state.z);
    doc.objects.push_back(std::move(moon_sphere));

    // S402/S403 -- the eclipsing shadow sphere: same object, opaque, depth-
    // tested against the moon disc like any other geometry, so whichever is
    // actually nearer the camera at a given pixel wins -- no special
    // blending needed for the crescent/gibbous silhouette to read correctly.
    auto shadow_sphere = MeshCraft::Mc3::Mc3Object::makeIcoSphere(
        "moon_shadow", shadow_radius, /*subdivisions=*/2, "moon_shadow");
    shadow_sphere->at(state.shadow_x, state.shadow_y, state.shadow_z);
    doc.objects.push_back(std::move(shadow_sphere));

    inject_materials(doc);

    FPCamera local_cam = cam;
    local_cam.x = 0.0f;
    local_cam.y = 0.0f;
    local_cam.z = 0.0f;
    // Same far-plane fix render_sun()'s own real bug (§5, 2026-07-11) taught
    // us -- FPCamera's default far_z (1000 m) is well short of
    // kMoonDistanceM (5000 m).
    local_cam.far_z = kMoonDistanceM * 1.5f;
    renderer.render(doc, local_cam);
}

void WorldRenderer::render_stars(Mc3Renderer& renderer, const FPCamera& cam,
                                  const std::vector<SkyAngle>& stars, SkyAngle sun) {
    const int visible = visible_star_count(static_cast<int>(stars.size()), sun);
    if (visible <= 0) return;  // full daylight -- nothing to draw

    // Same "at infinity" distance as the sun/moon; small and un-subdivided
    // (subdivisions=0 -- the base 12-vertex/20-triangle icosahedron) since a
    // star only needs to read as a small point of light, not a detailed
    // sphere -- keeps the per-frame rebuild cheap even at a few hundred
    // stars (see generate_star_field()'s own doc comment on the count cap).
    constexpr float kStarDistanceM = 5000.0f;
    constexpr float kStarRadiusM   = 8.0f;

    MeshCraft::Mc3::Mc3Document doc;
    doc.objects.reserve(static_cast<std::size_t>(visible));
    for (int i = 0; i < visible; ++i) {
        float x = 0.0f, y = 0.0f, z = 0.0f;
        sky_angle_to_xyz(stars[static_cast<std::size_t>(i)], kStarDistanceM, x, y, z);

        auto star = MeshCraft::Mc3::Mc3Object::makeIcoSphere(
            "star" + std::to_string(i), kStarRadiusM, /*subdivisions=*/0, "star_glow");
        star->at(x, y, z);
        doc.objects.push_back(std::move(star));
    }
    inject_materials(doc);

    FPCamera local_cam = cam;
    local_cam.x = 0.0f;
    local_cam.y = 0.0f;
    local_cam.z = 0.0f;
    // Same far-plane fix render_sun()/render_moon() already need -- see
    // render_sun()'s own doc comment on the real bug this taught us.
    local_cam.far_z = kStarDistanceM * 1.5f;
    renderer.render(doc, local_cam);
}

void WorldRenderer::render_clouds(Mc3Renderer& renderer, const FPCamera& cam, WeatherState state,
                                   double total_game_hours, WindState wind) {
    const auto puffs = compute_cloud_puffs(state, total_game_hours, wind);
    if (puffs.empty()) return;  // WeatherState::Clear -- nothing to draw

    // S701 -- "darker grey for overcast/rain/snow".
    const char* material = (state == WeatherState::Overcast || state == WeatherState::Rain ||
                             state == WeatherState::Snow)
                                ? "cloud_dark"
                                : "cloud_light";

    // S701 -- each puff is a small cluster of 3 overlapping icospheres (the
    // same primitive-composition technique ObjectDefinitionLibrary.cpp's
    // deciduous_tree()/birch_tree() already use for their own canopies),
    // not one single sphere -- reads as an irregular cloud blob instead of
    // an obviously round ball. Offsets/radii are fixed per puff (no
    // per-puff size variety, kept simple for this first pass).
    constexpr float kPuffRadiusM = 14.0f;

    MeshCraft::Mc3::Mc3Document doc;
    doc.objects.reserve(puffs.size());
    for (std::size_t i = 0; i < puffs.size(); ++i) {
        const CloudPuff& p = puffs[i];
        const std::string base_name = "cloud" + std::to_string(i);

        auto sphere_a = MeshCraft::Mc3::Mc3Object::makeIcoSphere(base_name + "_a", kPuffRadiusM, 1, material);
        auto sphere_b = MeshCraft::Mc3::Mc3Object::makeIcoSphere(base_name + "_b", kPuffRadiusM * 0.75f, 1, material);
        sphere_b->transform.position = {kPuffRadiusM * 0.9f, kPuffRadiusM * 0.15f, 0.0f};
        auto sphere_c = MeshCraft::Mc3::Mc3Object::makeIcoSphere(base_name + "_c", kPuffRadiusM * 0.65f, 1, material);
        sphere_c->transform.position = {-kPuffRadiusM * 0.8f, kPuffRadiusM * 0.1f, kPuffRadiusM * 0.3f};

        auto cluster = MeshCraft::Mc3::Mc3Object::makeGroup(
            base_name, {std::move(sphere_a), std::move(sphere_b), std::move(sphere_c)});
        cluster->at(p.x, p.y, p.z);
        doc.objects.push_back(std::move(cluster));
    }
    inject_materials(doc);

    FPCamera local_cam = cam;
    local_cam.x = 0.0f;
    local_cam.y = 0.0f;
    local_cam.z = 0.0f;
    // Unlike render_sun()/render_moon()/render_stars(), no far_z extension
    // needed here: compute_cloud_puffs()'s own default altitude/scatter
    // radius (180m/350m) stay well within FPCamera's default far_z (1000m).
    renderer.render(doc, local_cam);
}

void WorldRenderer::render_particles(Mc3Renderer& renderer, const FPCamera& cam,
                                      const std::vector<Particle>& particles, WeatherState state) {
    if (particles.empty()) return;
    if (state != WeatherState::Rain && state != WeatherState::Snow) return;

    MeshCraft::Mc3::Mc3Document doc;
    doc.objects.reserve(particles.size());

    if (state == WeatherState::Rain) {
        // S802 -- "fast-falling thin streak particles": a thin, short
        // vertical cylinder (a cheap, low-segment-count primitive -- no
        // need for a detailed shape at this size) reads as a streak from
        // any nearby viewing angle without needing to face the camera.
        constexpr float kStreakRadiusM = 0.03f;
        constexpr float kStreakHeightM = 0.6f;
        for (std::size_t i = 0; i < particles.size(); ++i) {
            const Particle& p = particles[i];
            auto streak = MeshCraft::Mc3::Mc3Object::makeCylinder(
                "rain" + std::to_string(i), kStreakRadiusM, kStreakHeightM, /*segments=*/4, "rain_streak");
            streak->at(p.x, p.y, p.z);
            doc.objects.push_back(std::move(streak));
        }
    } else {
        // S803 -- "slow-falling, gently drifting small white particles":
        // same un-subdivided-icosphere technique render_stars() already
        // uses for its own small points.
        constexpr float kFlakeRadiusM = 0.08f;
        for (std::size_t i = 0; i < particles.size(); ++i) {
            const Particle& p = particles[i];
            auto flake = MeshCraft::Mc3::Mc3Object::makeIcoSphere(
                "snow" + std::to_string(i), kFlakeRadiusM, /*subdivisions=*/0, "snow_flake");
            flake->at(p.x, p.y, p.z);
            doc.objects.push_back(std::move(flake));
        }
    }

    inject_materials(doc);

    FPCamera local_cam = cam;
    local_cam.x = 0.0f;
    local_cam.y = 0.0f;
    local_cam.z = 0.0f;
    // No far_z extension needed: ParticleSystem's own default spawn_radius_m
    // (60m) stays well within FPCamera's default far_z (1000m).
    renderer.render(doc, local_cam);
}

void WorldRenderer::apply_tree_sway(Mc3Renderer& renderer, WindState wind, double total_game_hours) {
    using MeshCraft::Renderer::AnimOverride;

    const std::array<float, 3> rot = compute_tree_sway_rotation_deg(wind, total_game_hours);

    AnimOverride ov;
    ov.rotation = rot;

    // Same override applied to BOTH sub-object names -- see S903's own doc
    // comment (WorldRenderer.hpp) on why this sways every tree instance in
    // the scene identically rather than independently.
    std::unordered_map<std::string, AnimOverride> overrides;
    overrides["trunk"]  = ov;
    overrides["canopy"] = ov;
    renderer.scene_renderer().setAnimOverrides(std::move(overrides));
}

void WorldRenderer::render_snow_accumulation(Mc3Renderer& renderer, const FPCamera& cam, double cam_x,
                                              double cam_y, double cam_z, float snow_depth,
                                              const std::vector<ModelPlacement>& placements,
                                              float radius_m) {
    if (snow_depth <= 0.0f) return;  // bare ground -- nothing to draw

    using namespace MeshCraft::Mc3;

    // Thin at any accumulation level -- "additive extra geometry" (S1002's
    // own wording), not a literal physical depth.
    constexpr float kMaxCapHeightM = 0.15f;
    const float     cap_h          = kMaxCapHeightM * snow_depth;

    MeshCraft::Mc3::Mc3Document doc;
    int next_id = 0;

    // S1001/S1002 -- roofs: scan currently-loaded chunks' cached geometry
    // for is_snow_eligible_chunk_object_name() objects. Same chunk-distance
    // cull + doc-cache-lock pattern nearby_collision_boxes() already uses.
    for (const ChunkCoord& coord : loaded_coords()) {
        const float chunk_x0 = static_cast<float>(coord.x) * cfg_.chunk_size_m;
        const float chunk_z0 = static_cast<float>(coord.y) * cfg_.chunk_size_m;

        const float px  = std::clamp(static_cast<float>(cam_x), chunk_x0, chunk_x0 + cfg_.chunk_size_m);
        const float pz  = std::clamp(static_cast<float>(cam_z), chunk_z0, chunk_z0 + cfg_.chunk_size_m);
        const float ddx = px - static_cast<float>(cam_x);
        const float ddz = pz - static_cast<float>(cam_z);
        if (ddx * ddx + ddz * ddz > radius_m * radius_m) continue;

        const MeshCraft::Mc3::Mc3Document* doc_ptr = nullptr;
        {
            std::lock_guard<std::mutex> lk(g_doc_cache_mutex);
            auto it = g_doc_cache.find(coord);
            if (it == g_doc_cache.end()) continue;  // never rendered -- nothing to overlay
            doc_ptr = &it->second;
        }

        for (const auto& obj_ptr : doc_ptr->objects) {
            if (!obj_ptr) continue;
            const Mc3Object& obj = *obj_ptr;
            if (obj.type != ObjectType::Box) continue;
            if (!obj.primitive) continue;
            if (!is_snow_eligible_chunk_object_name(obj.name)) continue;

            const auto& size  = obj.primitive->size;
            const float top_y = obj.transform.position[1] + size[1] * 0.5f;

            // World-space center, then made camera-relative in double
            // before narrowing to float -- same M176 floating-origin
            // precision reasoning render_placements() already establishes.
            const double world_x = static_cast<double>(chunk_x0) + obj.transform.position[0];
            const double world_z = static_cast<double>(chunk_z0) + obj.transform.position[2];
            const float  rel_x   = static_cast<float>(world_x - cam_x);
            const float  rel_y   = static_cast<float>(static_cast<double>(top_y) - cam_y);
            const float  rel_z   = static_cast<float>(world_z - cam_z);

            auto cap = Mc3Object::makeBox("snowcap" + std::to_string(next_id++),
                                          {size[0] * 0.95f, cap_h, size[2] * 0.95f}, "snow");
            cap->transform.position = {rel_x, rel_y + cap_h * 0.5f, rel_z};
            doc.objects.push_back(std::move(cap));
        }
    }

    // S1002 -- tree canopies / ground-level props: every visible placement
    // gets a small snow-cap sphere near its own approximated top height
    // (see kApproxOutdoorObjectHeightM's own doc comment in
    // WorldRenderer.hpp for why this is a stylized approximation).
    const std::vector<PlacementInstance> instances =
        compute_visible_placement_instances(cam_x, cam_y, cam_z, placements, radius_m);
    for (const PlacementInstance& inst : instances) {
        const float top_y = inst.y + kApproxOutdoorObjectHeightM * inst.scale;
        auto cap = Mc3Object::makeIcoSphere("snowcap" + std::to_string(next_id++),
                                             0.4f * inst.scale * snow_depth, /*subdivisions=*/0, "snow");
        cap->transform.position = {inst.x, top_y, inst.z};
        doc.objects.push_back(std::move(cap));
    }

    if (doc.objects.empty()) return;
    inject_materials(doc);

    FPCamera local_cam = cam;
    local_cam.x = 0.0f;
    local_cam.y = 0.0f;
    local_cam.z = 0.0f;
    renderer.render(doc, local_cam);
}

void WorldRenderer::clear_doc_cache() {
    std::lock_guard<std::mutex> lk(g_doc_cache_mutex);
    g_doc_cache.clear();
}

std::vector<CollisionBox> WorldRenderer::nearby_collision_boxes(double wx, double wz, float radius_m,
                                                                  float min_height_m) const {
    std::vector<CollisionBox> out;

    for (const ChunkCoord& coord : loaded_coords()) {
        const float chunk_x0 = static_cast<float>(coord.x) * cfg_.chunk_size_m;
        const float chunk_z0 = static_cast<float>(coord.y) * cfg_.chunk_size_m;

        // Cheap chunk-level distance cull (closest point on this chunk's own
        // square to (wx, wz)) before touching the doc cache lock/lookup.
        const float px  = std::clamp(static_cast<float>(wx), chunk_x0, chunk_x0 + cfg_.chunk_size_m);
        const float pz  = std::clamp(static_cast<float>(wz), chunk_z0, chunk_z0 + cfg_.chunk_size_m);
        const float ddx = px - static_cast<float>(wx);
        const float ddz = pz - static_cast<float>(wz);
        if (ddx * ddx + ddz * ddz > radius_m * radius_m) continue;

        const MeshCraft::Mc3::Mc3Document* doc_ptr = nullptr;
        {
            std::lock_guard<std::mutex> lk(g_doc_cache_mutex);
            auto it = g_doc_cache.find(coord);
            if (it == g_doc_cache.end()) continue;  // never rendered -- nothing to report
            doc_ptr = &it->second;
        }

        // R133 -- extract canonical inline and resolved-instance blockers in
        // local chunk space, then apply this renderer layer's chunk offset.
        // The extractor is deliberately core-only; this is merely the app
        // adapter that knows which generated chunk occupies which world tile.
        CollisionExtractionResult extracted = extract_mc3_collision_boxes(*doc_ptr, min_height_m);
        for (CollisionBox box : extracted.boxes) {
            box.min_x += chunk_x0;
            box.max_x += chunk_x0;
            box.min_z += chunk_z0;
            box.max_z += chunk_z0;
            out.push_back(box);
        }
    }

    return out;
}

#endif // MESH_WORLD_HAS_RENDERER

} // namespace MeshWorld
