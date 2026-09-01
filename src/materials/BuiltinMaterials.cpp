// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// Pre-registers every material ID used by C++ chunk generators and Lua generators.
// License for all procedural (no-texture) materials is MIT / procedurally generated.

#include "BuiltinMaterials.hpp"
#include "MaterialRegistry.hpp"

#include <filesystem>

namespace MeshWorld {

namespace {

void reg(const char* id, float r, float g, float b,
         float roughness = 0.8f, float metallic = 0.0f) {
    MaterialEntry e;
    e.id        = id;
    e.r         = r;
    e.g         = g;
    e.b         = b;
    e.roughness = roughness;
    e.metallic  = metallic;
    e.license.spdx_license = "MIT";
    e.license.author       = "MeshWorld procedural";
    MaterialRegistry::instance().register_material(std::move(e));
}

void reg_tex(const char* id, float r, float g, float b,
             float roughness, float metallic, const char* uri) {
    MaterialEntry e;
    e.id          = id;
    e.r           = r;
    e.g           = g;
    e.b           = b;
    e.roughness   = roughness;
    e.metallic    = metallic;
    // Real bug (found+fixed 2026-07-11, user-reported "everything is a flat
    // color, no textures"): MeshCraft's SceneRenderer resolves a texture's
    // relative URI against the LOADED DOCUMENT's own sourcePath (the
    // directory its .mc3.xml was read from -- for chunk documents, that's
    // this app's saves/<world>/chunks/, not this repo's real assets/ root),
    // so a plain relative "assets/textures/grass.png" silently resolved to
    // a nonexistent path and the renderer fell back to the material's flat
    // color with no error. Resolving to an ABSOLUTE path here sidesteps
    // that mismatch entirely: std::filesystem::path::operator/ replaces
    // (not appends to) its left operand when the right operand is already
    // absolute, so this resolves correctly regardless of whatever
    // sourcePath a given document ends up with -- chunk-loaded or
    // synthetic, present or future. Relies on the SAME "process is run
    // with its working directory at the repo root" assumption every other
    // binary in this project already makes for its own data files (see
    // every `ContentPackLoader{}.load_auto(".", ...)` call site) --
    // register_builtin_materials() always runs at process startup, before
    // anything could plausibly have changed the working directory.
    e.texture_uri = std::filesystem::absolute(uri).string();
    e.license.spdx_license = "MIT";
    e.license.author       = "MeshWorld procedural";
    MaterialRegistry::instance().register_material(std::move(e));
}

} // namespace

void register_builtin_materials() {
    // ── Ground / terrain ──────────────────────────────────────────────────────
    reg_tex("grass",           0.30f, 0.55f, 0.18f, 0.9f, 0.0f, "assets/textures/grass.png");
    reg_tex("grass_park",      0.35f, 0.60f, 0.22f, 0.9f, 0.0f, "assets/textures/grass.png");
    reg_tex("grass_meadow",    0.38f, 0.62f, 0.20f, 0.9f, 0.0f, "assets/textures/grass.png");
    reg_tex("grass_garden",    0.32f, 0.58f, 0.21f, 0.9f, 0.0f, "assets/textures/grass.png");
    reg_tex("grass_courtyard", 0.28f, 0.50f, 0.18f, 0.9f, 0.0f, "assets/textures/grass.png");
    reg_tex("grass_bank",      0.34f, 0.57f, 0.20f, 0.9f, 0.0f, "assets/textures/grass.png");
    reg_tex("grass_strip",     0.30f, 0.55f, 0.19f, 0.9f, 0.0f, "assets/textures/grass.png");
    reg("dirt",               0.50f, 0.35f, 0.20f);
    reg("soil",               0.42f, 0.30f, 0.18f);
    reg_tex("sand",       0.85f, 0.78f, 0.55f, 0.9f, 0.0f, "assets/textures/sand.png");
    reg_tex("sand_beach", 0.90f, 0.83f, 0.60f, 0.9f, 0.0f, "assets/textures/sand.png");
    reg_tex("sand_wet",   0.70f, 0.65f, 0.48f, 0.9f, 0.0f, "assets/textures/sand.png");
    reg_tex("sand_dune",  0.82f, 0.74f, 0.52f, 0.9f, 0.0f, "assets/textures/sand.png");
    reg("leaf_litter",        0.55f, 0.40f, 0.18f);
    reg("forest_floor",       0.28f, 0.22f, 0.10f);
    reg("jungle_floor",       0.20f, 0.30f, 0.12f);
    reg("jungle_undergrowth", 0.22f, 0.40f, 0.14f);
    reg("swamp_mud",          0.28f, 0.24f, 0.15f);
    reg("tundra_snow",        0.90f, 0.92f, 0.95f);
    reg("snow",               0.92f, 0.94f, 0.97f);
    reg("gravel_riverbed",    0.55f, 0.50f, 0.44f);
    reg("rock",               0.50f, 0.47f, 0.43f);

    // ── Paving / roads ────────────────────────────────────────────────────────
    reg_tex("asphalt",           0.22f, 0.22f, 0.22f, 0.9f, 0.0f, "assets/textures/asphalt.png");
    reg_tex("concrete",          0.72f, 0.72f, 0.72f, 0.9f, 0.0f, "assets/textures/concrete.png");
    reg_tex("concrete_pavement", 0.68f, 0.68f, 0.68f, 0.9f, 0.0f, "assets/textures/concrete.png");
    reg_tex("concrete_slab",     0.70f, 0.70f, 0.70f, 0.9f, 0.0f, "assets/textures/concrete.png");
    reg_tex("concrete_panel",    0.65f, 0.65f, 0.65f, 0.9f, 0.0f, "assets/textures/concrete.png");
    reg("pavement",           0.65f, 0.63f, 0.60f);
    reg("pavement_slab",      0.67f, 0.65f, 0.62f);
    // T238 (2026-07-12) -- found immediately upon first real use of the new
    // MeshWorldExport --validate flag: road.lua's/crossroad.lua's own
    // scene:addGround("sidewalk") referenced this and it was never
    // registered, same class of gap R106 found for flowers_*.
    reg("sidewalk",           0.66f, 0.64f, 0.61f);
    reg("cobblestone",        0.45f, 0.42f, 0.38f);
    reg("cobblestone_light",  0.60f, 0.57f, 0.52f);
    reg("cobblestone_path",   0.48f, 0.45f, 0.40f);
    reg("cobblestone_square", 0.50f, 0.47f, 0.43f);
    reg("stone_pavement",     0.58f, 0.55f, 0.50f);
    reg("stone_path",         0.55f, 0.52f, 0.47f);
    reg("path_gravel",        0.62f, 0.57f, 0.48f);
    reg("path_stone",         0.55f, 0.52f, 0.47f);
    reg("road_line_white",    0.92f, 0.92f, 0.88f);
    reg("road_marking_white", 0.92f, 0.92f, 0.88f);
    reg("paint_white",        0.93f, 0.93f, 0.90f);

    // ── Stone ─────────────────────────────────────────────────────────────────
    reg("stone_granite",      0.50f, 0.48f, 0.45f);
    reg("stone_light",        0.78f, 0.75f, 0.70f);
    reg("stone_arch",         0.55f, 0.52f, 0.48f);
    reg("stone_bridge",       0.48f, 0.45f, 0.42f);
    reg("stone_curb",         0.60f, 0.58f, 0.55f);
    reg("stone_embank",       0.45f, 0.43f, 0.40f);
    reg("stone_post",         0.52f, 0.50f, 0.47f);
    reg("stone_railing",      0.55f, 0.53f, 0.50f);
    reg("stone_countertop",   0.65f, 0.62f, 0.58f);
    reg("stone_wall",         0.55f, 0.52f, 0.48f);

    // ── Rock / cave ───────────────────────────────────────────────────────────
    reg("rock_grey",          0.50f, 0.50f, 0.50f);
    reg("rock_cliff",         0.42f, 0.40f, 0.36f);
    reg("rock_cave_floor",    0.30f, 0.28f, 0.25f);
    reg("rock_cave_wall",     0.32f, 0.30f, 0.27f);
    reg("rock_cave_ceiling",  0.28f, 0.26f, 0.23f);
    reg("rock_sea",           0.40f, 0.42f, 0.44f);
    reg("rock_sandstone",     0.72f, 0.58f, 0.38f);
    reg("rock_mossy",         0.35f, 0.45f, 0.28f);
    reg("rock_snow_covered",  0.72f, 0.74f, 0.75f);
    // MAP21, M333/M335 -- CaveGenerator's generate() already referenced
    // rock_stalactite/rock_stalagmite (its own raw w.cylinder() calls) but
    // neither was ever actually registered here, a pre-existing gap fixed
    // as part of adding real ObjectDefinitionLibrary definitions for them.
    reg("rock_stalactite",    0.42f, 0.40f, 0.38f);
    reg("rock_stalagmite",    0.40f, 0.38f, 0.35f);
    reg("rock_rubble",        0.36f, 0.33f, 0.30f);

    // ── Water ─────────────────────────────────────────────────────────────────
    reg_tex("water",         0.18f, 0.42f, 0.65f, 0.05f, 0.0f, "assets/textures/water.png");
    reg_tex("water_deep",    0.10f, 0.28f, 0.55f, 0.05f, 0.0f, "assets/textures/water.png");
    reg_tex("water_still",   0.20f, 0.45f, 0.68f, 0.03f, 0.0f, "assets/textures/water.png");
    reg_tex("water_stream",  0.22f, 0.48f, 0.70f, 0.05f, 0.0f, "assets/textures/water.png");
    reg_tex("water_ocean",   0.08f, 0.22f, 0.50f, 0.05f, 0.0f, "assets/textures/water.png");
    reg("water_foam",        0.88f, 0.94f, 0.96f, 0.02f, 0.0f);
    reg_tex("water_swamp",   0.20f, 0.32f, 0.25f, 0.10f, 0.0f, "assets/textures/water.png");
    reg_tex("water_fountain",0.22f, 0.50f, 0.72f, 0.04f, 0.0f, "assets/textures/water.png");
    reg_tex("water_bowl",    0.20f, 0.48f, 0.70f, 0.04f, 0.0f, "assets/textures/water.png");

    // ── Vegetation ────────────────────────────────────────────────────────────
    reg("flower_red",         0.80f, 0.12f, 0.10f);
    reg("flower_yellow",      0.90f, 0.80f, 0.08f);
    reg("flower_daisy",       0.95f, 0.92f, 0.88f);
    reg("flower_poppy",       0.85f, 0.10f, 0.08f);
    reg("flower_bluebell",    0.35f, 0.40f, 0.80f);
    reg("flower_buttercup",   0.92f, 0.82f, 0.10f);
    // R106 audit (2026-07-12) -- generators/lua/zone/park.lua's own
    // FLOWER_COLORS table uses this separate "flowers_*" (plural) naming,
    // distinct from the "flower_*" (singular) ids above; never registered,
    // a real (if narrow) pre-existing gap -- found via full-suite
    // MaterialRegistry "not registered" warning output, not assumed.
    reg("flowers_red",        0.82f, 0.14f, 0.12f);
    reg("flowers_yellow",     0.90f, 0.80f, 0.10f);
    reg("flowers_white",      0.95f, 0.94f, 0.90f);
    reg("flowers_purple",     0.55f, 0.30f, 0.70f);
    reg("vine_green",         0.16f, 0.42f, 0.12f);
    reg("plant_lily_pad",     0.20f, 0.48f, 0.16f);
    reg("vfx_mist",           0.72f, 0.78f, 0.76f, 0.35f, 0.0f);

    // ── Wood ──────────────────────────────────────────────────────────────────
    reg_tex("wood_natural",      0.60f, 0.40f, 0.22f, 0.85f, 0.0f, "assets/textures/wood.png");
    reg_tex("wood_fence",        0.55f, 0.38f, 0.20f, 0.85f, 0.0f, "assets/textures/wood.png");
    reg_tex("wood_bench",        0.55f, 0.38f, 0.20f, 0.85f, 0.0f, "assets/textures/wood.png");
    reg_tex("wood_counter",      0.58f, 0.40f, 0.24f, 0.85f, 0.0f, "assets/textures/wood.png");
    reg_tex("wood_door_frame",   0.52f, 0.36f, 0.18f, 0.85f, 0.0f, "assets/textures/wood.png");
    reg_tex("wood_door_panel",   0.50f, 0.34f, 0.17f, 0.85f, 0.0f, "assets/textures/wood.png");
    reg_tex("wood_window_frame", 0.54f, 0.38f, 0.19f, 0.85f, 0.0f, "assets/textures/wood.png");
    reg_tex("bark_driftwood",    0.42f, 0.30f, 0.17f, 0.85f, 0.0f, "assets/textures/wood.png");
    reg_tex("bark_log",          0.36f, 0.24f, 0.12f, 0.85f, 0.0f, "assets/textures/wood.png");

    // ── Metal ─────────────────────────────────────────────────────────────────
    reg("metal_lamp",         0.60f, 0.58f, 0.54f, 0.4f, 0.5f);
    reg("metal_lamp_ornate",  0.55f, 0.50f, 0.40f, 0.3f, 0.6f);
    reg("metal_railing",      0.55f, 0.55f, 0.55f, 0.4f, 0.5f);
    reg("metal_chrome",       0.80f, 0.80f, 0.82f, 0.1f, 0.9f);
    reg("metal_dark",         0.25f, 0.25f, 0.25f, 0.5f, 0.6f);
    reg("metal_grate",        0.40f, 0.40f, 0.40f, 0.6f, 0.5f);

    // R121 zone/chunk audit follow-up (2026-07-12) -- traffic-light lens
    // colors, ported from generators/lua/zone/crossroad.lua to
    // CrossroadGenerator.cpp (the C++ fallback). Never registered before
    // (crossroad.lua referenced them without them existing here either --
    // a real, if minor, pre-existing material gap, same class R106 already
    // found/fixed for flowers_*).
    reg("light_red",   0.85f, 0.10f, 0.08f, 0.2f);
    reg("light_amber", 0.90f, 0.55f, 0.05f, 0.2f);
    reg("light_green", 0.10f, 0.75f, 0.15f, 0.2f);
    // R138 -- dim lenses make a traffic signal read as a real state instead
    // of the old impossible "red + amber + green on at once" stack. They
    // remain deliberately bright enough to be legible in daylight; true
    // emissive bloom is a separate renderer capability.
    reg("light_red_dim",   0.18f, 0.02f, 0.02f, 0.7f);
    reg("light_amber_dim", 0.18f, 0.08f, 0.01f, 0.7f);
    reg("light_green_dim", 0.02f, 0.16f, 0.03f, 0.7f);

    // ── Building / surfaces ───────────────────────────────────────────────────
    reg_tex("plaster_white",  0.92f, 0.90f, 0.86f, 0.9f, 0.0f, "assets/textures/plaster.png");
    reg_tex("plaster_cream",  0.90f, 0.86f, 0.76f, 0.9f, 0.0f, "assets/textures/plaster.png");
    // The composer-owned apartment.block.wide_01 definition uses this
    // warmer facade shade.  It must be registered too so live rendering
    // and MeshWorldGLB both preserve the intended material instead of
    // falling back to an unmaterialed surface.
    reg_tex("plaster_beige",  0.84f, 0.76f, 0.62f, 0.9f, 0.0f, "assets/textures/plaster.png");
    reg_tex("plaster_yellow", 0.90f, 0.84f, 0.55f, 0.9f, 0.0f, "assets/textures/plaster.png");
    reg_tex("plaster_green",  0.70f, 0.82f, 0.68f, 0.9f, 0.0f, "assets/textures/plaster.png");
    reg_tex("plaster_blue",   0.60f, 0.70f, 0.85f, 0.9f, 0.0f, "assets/textures/plaster.png");
    reg_tex("plaster_grey",   0.72f, 0.72f, 0.70f, 0.9f, 0.0f, "assets/textures/plaster.png");
    reg_tex("brick_red",  0.65f, 0.28f, 0.18f, 0.85f, 0.0f, "assets/textures/brick.png");
    reg_tex("brick_dark", 0.42f, 0.20f, 0.14f, 0.85f, 0.0f, "assets/textures/brick.png");
    reg("glass_clear",        0.75f, 0.85f, 0.90f, 0.05f);
    reg_tex("roof_tile_red",  0.62f, 0.25f, 0.15f, 0.85f, 0.0f, "assets/textures/roof_tile.png");
    reg_tex("roof_tile_grey", 0.50f, 0.50f, 0.50f, 0.85f, 0.0f, "assets/textures/roof_tile.png");
    // T238 (2026-07-12) -- another real gap found by the new
    // MeshWorldExport --validate flag, right after the "sidewalk" one:
    // ShopStreetGenerator.cpp's own awning planes referenced this and it
    // was never registered.
    reg("awning_stripe",      0.80f, 0.18f, 0.15f, 0.7f);

    // ── Interior ──────────────────────────────────────────────────────────────
    reg("tile_kitchen",       0.88f, 0.88f, 0.85f);
    reg("appliance_white",    0.92f, 0.92f, 0.92f, 0.3f);
    reg("plastic_black",      0.10f, 0.10f, 0.10f, 0.6f);
    reg("tv_screen_off",      0.05f, 0.05f, 0.08f, 0.1f, 0.2f);

    // ── Ice / frozen ─────────────────────────────────────────────────────────
    reg("ice_stream",         0.70f, 0.82f, 0.90f, 0.05f);
    reg("ice_sheet",          0.72f, 0.84f, 0.92f, 0.04f);

    // ── Foliage (tree canopies) ───────────────────────────────────────────────
    reg("foliage_oak",        0.22f, 0.40f, 0.10f);
    reg("foliage_linden",     0.30f, 0.50f, 0.14f);
    reg("foliage_birch",      0.35f, 0.55f, 0.18f);
    reg("foliage_chestnut",   0.18f, 0.34f, 0.08f);
    reg("foliage_beech",      0.26f, 0.44f, 0.12f);
    reg("foliage_fruit",      0.28f, 0.48f, 0.14f);
    reg("foliage_willow",     0.28f, 0.48f, 0.20f);
    reg("foliage_palm",       0.22f, 0.50f, 0.10f);
    reg("foliage_pine",       0.10f, 0.28f, 0.14f);
    reg("foliage_tropical",   0.18f, 0.42f, 0.16f);
    reg("bamboo_cane",        0.55f, 0.65f, 0.25f);

    // ── Tree bark / trunk ────────────────────────────────────────────────────
    reg("wood_bark_dark",     0.30f, 0.22f, 0.12f);
    reg("wood_bark_light",    0.52f, 0.40f, 0.24f);
    reg("wood_birch",         0.88f, 0.84f, 0.78f);
    reg("wood_palm",          0.55f, 0.42f, 0.28f);
    reg("wood_bark_dead",     0.40f, 0.35f, 0.26f);

    // ── Nature / plants ──────────────────────────────────────────────────────
    reg("shrub_foliage",      0.20f, 0.42f, 0.12f);
    reg("mushroom_cap_brown", 0.52f, 0.30f, 0.12f);
    reg("mushroom_stem",      0.85f, 0.80f, 0.70f);
    reg("grass_tall",         0.30f, 0.52f, 0.20f);
    reg("cactus_green",       0.12f, 0.42f, 0.14f);
    reg("plant_scrub",        0.40f, 0.42f, 0.22f);
    reg("plant_tropical",     0.14f, 0.42f, 0.18f);
    reg("plant_marsh",        0.20f, 0.38f, 0.14f);
    // T236 (2026-07-13) -- OceanGenerator.cpp's own "weed_0".."weed_4"
    // floating seaweed planes always reference this material (unconditional,
    // not behind any random roll); found via --validate against a fresh
    // demo world whose planet-generated biome happened to place ocean at a
    // grid-edge chunk -- same class of gap R106/T238 found for flowers_*/
    // sidewalk/awning_stripe.
    reg("plant_sea_weed_float", 0.10f, 0.32f, 0.20f);
    reg("lichen_grey",        0.58f, 0.60f, 0.52f);
    reg("shell_white",        0.92f, 0.88f, 0.80f);

    // ── Minerals / cave ──────────────────────────────────────────────────────
    reg("crystal_blue",       0.25f, 0.52f, 0.88f, 0.2f, 0.3f);

    // ── Aquatic (MAP20, M326 -- coral_reef/kelp_forest sub-areas) ───────────
    reg("coral_pink",         0.85f, 0.35f, 0.45f, 0.6f);
    reg("kelp_green",         0.15f, 0.35f, 0.16f, 0.7f);

    // ── Sky / celestial (S301, sky/day-night/weather S-series) ──────────────
    // No emissive/unlit material support exists yet (MaterialEntry has no
    // such field -- confirmed before picking this approach) -- a real
    // "glows regardless of scene lighting" sun would need shader work in
    // mesh-craft, the same class of cross-repo change S203 already flags as
    // ask-first. This is the best available v1 approximation: a very
    // bright, low-roughness (glossy, reflects strongly) warm near-white so
    // the sun sphere still reads as bright under this app's own fixed
    // scene lighting (SceneRenderer::EnableDefaultLighting()).
    reg("sun_glow", 1.0f, 0.95f, 0.75f, /*roughness=*/0.05f, /*metallic=*/0.0f);

    // S401/S403 -- moon disc (pale grey-white, dimmer/cooler than the
    // sun's own warm glow) + its eclipsing "shadow" sphere (near-black --
    // dark albedo reads as dark even under this app's own fixed lighting,
    // same reasoning sun_glow's own comment already explains for why
    // there's no real emissive/unlit material to reach for instead).
    reg("moon_glow",   0.85f, 0.85f, 0.90f, /*roughness=*/0.3f, /*metallic=*/0.0f);
    reg("moon_shadow", 0.02f, 0.02f, 0.03f, /*roughness=*/0.9f, /*metallic=*/0.0f);

    // S502 -- stars: small, bright, slightly cool-white points, same "no
    // emissive material yet" approximation sun_glow/moon_glow already use.
    reg("star_glow", 0.95f, 0.95f, 1.0f, /*roughness=*/0.05f, /*metallic=*/0.0f);

    // S701 -- clouds: light-grey normally, darker grey for overcast/rain/
    // snow (matte, unlike the glossy sun/moon/star materials above).
    reg("cloud_light", 0.90f, 0.90f, 0.92f, /*roughness=*/0.9f, /*metallic=*/0.0f);
    reg("cloud_dark",  0.45f, 0.45f, 0.48f, /*roughness=*/0.9f, /*metallic=*/0.0f);

    // S802/S803 -- precipitation: light blue-grey rain streaks, small white
    // snowflakes (matte, same reasoning cloud_light/cloud_dark already use).
    reg("rain_streak", 0.55f, 0.65f, 0.75f, /*roughness=*/0.2f, /*metallic=*/0.0f);
    reg("snow_flake",  0.95f, 0.96f, 0.98f, /*roughness=*/0.6f, /*metallic=*/0.0f);

    // ── Furniture / appliances / vehicles (T195-T212 Lua object generators) ──
    reg("fabric_bed",     0.85f, 0.88f, 0.92f, /*roughness=*/0.85f);
    reg("fabric_pillow",  0.92f, 0.92f, 0.90f, /*roughness=*/0.85f);
    reg("fabric_sofa",    0.35f, 0.42f, 0.45f, /*roughness=*/0.80f);
    reg("ceramic_white",  0.95f, 0.95f, 0.95f, /*roughness=*/0.15f);
    reg("car_paint_red",   0.75f, 0.10f, 0.08f, /*roughness=*/0.25f, /*metallic=*/0.3f);
    reg("car_paint_blue",  0.12f, 0.30f, 0.65f, /*roughness=*/0.25f, /*metallic=*/0.3f);
    reg("car_paint_white", 0.92f, 0.92f, 0.90f, /*roughness=*/0.25f, /*metallic=*/0.1f);
    reg("car_paint_black", 0.05f, 0.05f, 0.06f, /*roughness=*/0.20f, /*metallic=*/0.3f);
    reg("tire_rubber",    0.05f, 0.05f, 0.05f, /*roughness=*/0.9f);
    reg("bicycle_frame",  0.15f, 0.35f, 0.70f, /*roughness=*/0.35f, /*metallic=*/0.4f);
    reg("mailbox_body",   0.15f, 0.25f, 0.55f, /*roughness=*/0.5f,  /*metallic=*/0.3f);
    reg("hydrant_red",    0.80f, 0.12f, 0.08f, /*roughness=*/0.4f,  /*metallic=*/0.1f);
    reg("sign_panel",     0.90f, 0.90f, 0.85f, /*roughness=*/0.3f);
}

} // namespace MeshWorld
