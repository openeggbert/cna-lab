// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "ObjectDefinitionLibrary.hpp"

#include <MeshCraft/Mc3/Mc3AssetMetadata.hpp>
#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>
#include <MeshCraft/Mc3/Mc3Primitive.hpp>
#include <MeshCraft/Mc3/Mc3Transform.hpp>

#include <cmath>
#include <vector>

using namespace MeshCraft::Mc3;

namespace MeshWorld {

// ---------------------------------------------------------------------------
// Shape helpers — all geometry in definition-local space, base at y=0.
// Conventions mirror Mc3DocumentBuilder:
//   cylinder: position = {cx, base_y + h/2, cz}
//   box:      position = {cx, base_y + sy/2, cz}
// ---------------------------------------------------------------------------

namespace {

std::shared_ptr<Mc3Object> cyl(const std::string& name,
                                float r, float h, const std::string& mat,
                                float base_y = 0.f, float cx = 0.f, float cz = 0.f) {
    auto o = Mc3Object::makeCylinder(name, r, h, 12, mat);
    o->transform.position = {cx, base_y + h / 2.f, cz};
    return o;
}

std::shared_ptr<Mc3Object> box(const std::string& name,
                                float sx, float sy, float sz, const std::string& mat,
                                float base_y = 0.f, float cx = 0.f, float cz = 0.f,
                                float ry_deg = 0.f) {
    auto o = Mc3Object::makeBox(name, {sx, sy, sz}, mat);
    o->transform.position = {cx, base_y + sy / 2.f, cz};
    if (ry_deg != 0.f) o->transform.rotation = {0.f, ry_deg, 0.f};
    return o;
}

// IcoSphere centered at (cx, center_y, cz).
std::shared_ptr<Mc3Object> icosphere(const std::string& name,
                                      float r, const std::string& mat,
                                      float center_y = 0.f,
                                      float cx = 0.f, float cz = 0.f,
                                      float sx = 1.f, float sy = 1.f, float sz = 1.f) {
    auto o = Mc3Object::makeIcoSphere(name, r, 2, mat);
    o->transform.position = {cx, center_y, cz};
    if (sx != 1.f || sy != 1.f || sz != 1.f)
        o->transform.scale = {sx, sy, sz};
    return o;
}

// Cone with base at base_y, centered laterally.
std::shared_ptr<Mc3Object> cone(const std::string& name,
                                 float r, float h, const std::string& mat,
                                 float base_y = 0.f,
                                 float cx = 0.f, float cz = 0.f) {
    auto o = Mc3Object::makeCone(name, r, h, 12, mat);
    o->transform.position = {cx, base_y + h / 2.f, cz};
    return o;
}

// Deciduous tree: cylinder trunk + two IcoSphere canopy levels.
std::shared_ptr<Mc3Object> deciduous_tree(
    const std::string& trunk_mat, const std::string& foliage_mat,
    float tr, float th, float cr) {
    float cy = th * 0.70f;  // height where main canopy sphere is centered
    return Mc3Object::makeGroup("tree", {
        cyl("trunk",       tr,       th,  trunk_mat),
        icosphere("canopy",     cr,        foliage_mat, cy + cr),
        icosphere("canopy_top", cr * 0.6f, foliage_mat, cy + cr * 1.9f),
    });
}

// Birch/pear: slender trunk + tall narrow icosphere canopy (scale deform).
std::shared_ptr<Mc3Object> birch_tree(
    const std::string& trunk_mat, const std::string& foliage_mat,
    float tr, float th, float cr) {
    float cy = th * 0.65f;
    return Mc3Object::makeGroup("tree", {
        cyl("trunk",  tr, th, trunk_mat),
        icosphere("canopy", cr, foliage_mat, cy + cr * 1.3f,
                  0.f, 0.f, 0.60f, 1.55f, 0.60f),
    });
}

// Willow: thin tall trunk + wide drooping flat canopy.
std::shared_ptr<Mc3Object> willow_tree() {
    return Mc3Object::makeGroup("willow", {
        cyl("trunk",      0.14f, 5.0f, "wood_bark_light"),
        box("canopy",     5.0f, 2.2f, 5.0f, "foliage_willow", 2.5f),
        box("drape_n",    0.5f, 2.5f, 0.5f, "foliage_willow", 0.5f, 0.f, -2.2f),
        box("drape_s",    0.5f, 2.5f, 0.5f, "foliage_willow", 0.5f, 0.f,  2.2f),
        box("drape_e",    0.5f, 2.5f, 0.5f, "foliage_willow", 0.5f,  2.2f, 0.f),
        box("drape_w",    0.5f, 2.5f, 0.5f, "foliage_willow", 0.5f, -2.2f, 0.f),
    });
}

// Palm: tall thin trunk + flat fan crown of crossed boxes.
std::shared_ptr<Mc3Object> palm_tree() {
    return Mc3Object::makeGroup("palm", {
        cyl("trunk",      0.12f, 6.0f, "wood_palm"),
        box("crown_ns",   0.5f,  0.4f, 5.0f, "foliage_palm", 6.0f),
        box("crown_ew",   5.0f,  0.4f, 0.5f, "foliage_palm", 6.0f),
        box("crown_nw",   3.5f,  0.35f, 0.5f, "foliage_palm", 5.9f, 0.f, 0.f, 45.f),
        box("crown_ne",   3.5f,  0.35f, 0.5f, "foliage_palm", 5.9f, 0.f, 0.f, -45.f),
    });
}

// Pine/mountain tree: cylinder trunk + 4 stacked Cones (true conical silhouette).
std::shared_ptr<Mc3Object> pine_tree() {
    return Mc3Object::makeGroup("pine", {
        cyl( "trunk", 0.12f, 2.5f, "wood_bark_dark"),
        cone("tier1", 1.8f, 1.8f, "foliage_pine", 2.0f),
        cone("tier2", 1.3f, 1.6f, "foliage_pine", 3.3f),
        cone("tier3", 0.8f, 1.4f, "foliage_pine", 4.3f),
        cone("tip",   0.35f,1.0f, "foliage_pine", 5.3f),
    });
}

// Dead gnarled tree: thick trunk + bare branch stubs.
std::shared_ptr<Mc3Object> dead_tree() {
    return Mc3Object::makeGroup("dead_tree", {
        cyl("trunk",      0.18f, 4.5f, "wood_bark_dead"),
        box("branch_l",   1.8f,  0.12f, 0.12f, "wood_bark_dead", 3.5f, -0.9f, 0.f, 20.f),
        box("branch_r",   1.6f,  0.10f, 0.10f, "wood_bark_dead", 3.0f,  0.8f, 0.f, -15.f),
        box("branch_b",   0.12f, 0.10f, 1.5f,  "wood_bark_dead", 2.5f,  0.f,  0.7f, 0.f),
    });
}

// Bare winter tree: slender trunk + thin branches, no foliage.
std::shared_ptr<Mc3Object> bare_tree() {
    return Mc3Object::makeGroup("bare_tree", {
        cyl("trunk",      0.10f, 4.0f, "wood_bark_light"),
        box("branch_l",   1.4f,  0.08f, 0.08f, "wood_bark_light", 3.0f, -0.7f, 0.f, 15.f),
        box("branch_r",   1.2f,  0.07f, 0.07f, "wood_bark_light", 2.8f,  0.6f, 0.f, -10.f),
        box("branch_f",   0.07f, 0.07f, 1.2f,  "wood_bark_light", 2.6f,  0.f,  0.5f, 5.f),
    });
}

// Park bench: two leg supports + seat plank + backrest.
std::shared_ptr<Mc3Object> make_bench(const std::string& seat_mat,
                                       const std::string& leg_mat) {
    float w  = 1.6f;
    float sh = 0.44f;
    return Mc3Object::makeGroup("bench", {
        box("leg_l",     0.08f, sh,   0.50f, leg_mat,  0.f, -(w/2.f-0.1f), 0.f),
        box("leg_r",     0.08f, sh,   0.50f, leg_mat,  0.f,  (w/2.f-0.1f), 0.f),
        box("seat",      w-0.05f, 0.04f, 0.35f, seat_mat, sh, 0.f,  0.08f),
        box("backrest",  w-0.05f, 0.35f, 0.04f, seat_mat, sh+0.16f, 0.f, -0.19f),
    });
}

// Shrub: three overlapping IcoSpheres for a rounded natural silhouette.
std::shared_ptr<Mc3Object> make_shrub() {
    return Mc3Object::makeGroup("shrub", {
        icosphere("core",   0.50f, "shrub_foliage", 0.50f),
        icosphere("side_a", 0.35f, "shrub_foliage", 0.40f,  0.35f,  0.20f),
        icosphere("side_b", 0.32f, "shrub_foliage", 0.35f, -0.28f, -0.18f),
    });
}

// Mushroom: short stem cylinder + rounded dome cap (icosphere scaled flat).
std::shared_ptr<Mc3Object> make_mushroom() {
    return Mc3Object::makeGroup("mushroom", {
        cyl("stem", 0.06f, 0.30f, "mushroom_stem"),
        icosphere("cap", 0.22f, "mushroom_cap_brown", 0.32f,
                  0.f, 0.f, 1.6f, 0.7f, 1.6f),
    });
}

// Tall grass tuft: two thin crossed planes.
std::shared_ptr<Mc3Object> make_grass_tuft() {
    return Mc3Object::makeGroup("grass_tuft", {
        box("blade_a", 0.05f, 0.65f, 0.45f, "grass_tall"),
        box("blade_b", 0.45f, 0.65f, 0.05f, "grass_tall"),
    });
}

// Generic rock: a slightly irregular box.
std::shared_ptr<Mc3Object> make_rock(const std::string& mat,
                                      float sx, float sy, float sz) {
    return Mc3Object::makeGroup("rock", {
        box("body", sx, sy, sz, mat, 0.f, 0.f, 0.f, 15.f),
    });
}

// Rock pile: three stacked/offset boxes.
std::shared_ptr<Mc3Object> make_rock_pile() {
    return Mc3Object::makeGroup("rock_pile", {
        box("r0", 0.6f, 0.35f, 0.5f, "rock_grey", 0.f,  0.f,   0.f,  10.f),
        box("r1", 0.4f, 0.30f, 0.4f, "rock_grey", 0.28f, 0.15f, 0.f, -8.f),
        box("r2", 0.3f, 0.22f, 0.3f, "rock_grey", 0.50f, -0.12f, 0.1f, 20.f),
    });
}

// Cave crystal: narrow tall box, slightly tilted.
std::shared_ptr<Mc3Object> make_crystal() {
    return Mc3Object::makeGroup("crystal", {
        box("shard_a", 0.12f, 1.2f, 0.12f, "crystal_blue", 0.f,  0.f,   0.f, 12.f),
        box("shard_b", 0.09f, 0.90f, 0.09f, "crystal_blue", 0.f,  0.15f, 0.1f, -8.f),
    });
}

// Cactus: main cylinder body + two arm cylinders.
std::shared_ptr<Mc3Object> make_cactus() {
    return Mc3Object::makeGroup("cactus", {
        cyl("body",    0.14f, 2.2f, "cactus_green"),
        box("arm_l",   0.12f, 0.8f, 0.12f, "cactus_green", 1.0f, -0.35f, 0.f),
        box("arm_r",   0.12f, 0.8f, 0.12f, "cactus_green", 1.0f,  0.35f, 0.f),
    });
}

// Simple low plant: single flat box.
std::shared_ptr<Mc3Object> make_simple_plant(const std::string& mat,
                                              float sx, float sy, float sz) {
    return Mc3Object::makeGroup("plant", {
        box("body", sx, sy, sz, mat, 0.f),
    });
}

// Flat ground-covering plant (lichen, etc.).
std::shared_ptr<Mc3Object> make_flat_plant(const std::string& mat,
                                            float sx, float sy, float sz) {
    return Mc3Object::makeGroup("flat_plant", {
        box("body", sx, sy, sz, mat, 0.f, 0.f, 0.f, 20.f),
    });
}

// Seashell: small irregular box pair.
std::shared_ptr<Mc3Object> make_seashell() {
    return Mc3Object::makeGroup("shell", {
        box("hull", 0.18f, 0.07f, 0.14f, "shell_white", 0.f, 0.f, 0.f, 10.f),
    });
}

// Branching coral: a small cluster of stubby cylinders + a rounded tip,
// low to the sea floor (MAP20, M326).
std::shared_ptr<Mc3Object> make_coral() {
    return Mc3Object::makeGroup("coral", {
        cyl("branch_a", 0.05f, 0.35f, "coral_pink", 0.f,  0.f,   0.f),
        cyl("branch_b", 0.04f, 0.26f, "coral_pink", 0.f,  0.10f, 0.06f),
        cyl("branch_c", 0.04f, 0.22f, "coral_pink", 0.f, -0.08f, 0.07f),
        icosphere("tip", 0.10f, "coral_pink", 0.40f),
    });
}

// Stalactite: a plain cylinder (MAP21, M333) -- matches CaveGenerator's own
// pre-M333 raw w.cylinder() shape exactly, deliberately not tapered (a
// tapered/inverted-cone look would need pitch rotation or negative scale,
// neither of which any shape helper in this file uses today -- see M327's
// own note that this codebase's object library only ever uses yaw
// rotation). "Hangs from the ceiling" is purely a placement-time concern
// (this definition's own local origin is its BOTTOM, like every other
// object here -- CaveGenerator.cpp positions it so that bottom+height
// lands exactly at the cave's ceiling height, see its own comment).
std::shared_ptr<Mc3Object> make_stalactite() {
    return Mc3Object::makeGroup("stalactite", {
        cyl("body", 0.25f, 1.6f, "rock_stalactite"),
    });
}

// Stalagmite: a cone rising point-up from the cave floor (MAP21, M333) --
// cone()'s own "apex points up, like cylinder" convention (see
// Mc3DocumentBuilder.hpp) matches a stalagmite's real shape exactly, no
// rotation trickery needed (unlike the stalactite above).
std::shared_ptr<Mc3Object> make_stalagmite() {
    return Mc3Object::makeGroup("stalagmite", {
        cone("body", 0.22f, 1.0f, "rock_stalagmite"),
    });
}

// Rubble: a small scattered cluster of rock debris on the cave floor
// (MAP21, M335) -- three small boxes at slightly different sizes/
// rotations/offsets, mirroring make_rock_pile()'s own "stacked/offset
// boxes" technique but flatter and more spread out (a debris scatter, not
// a stacked pile).
std::shared_ptr<Mc3Object> make_rubble() {
    return Mc3Object::makeGroup("rubble", {
        box("r0", 0.35f, 0.20f, 0.30f, "rock_rubble", 0.f,  0.f,   0.f,   15.f),
        box("r1", 0.25f, 0.15f, 0.22f, "rock_rubble", 0.f,  0.22f, 0.10f, -20.f),
        box("r2", 0.18f, 0.12f, 0.16f, "rock_rubble", 0.f, -0.15f, 0.14f,  30.f),
    });
}

// Kelp strand: two tall thin fronds reaching from the sea floor toward the
// surface, topped with a small buoyant bulb (MAP20, M326).
std::shared_ptr<Mc3Object> make_kelp() {
    return Mc3Object::makeGroup("kelp", {
        cyl("frond_a",     0.05f, 3.5f, "kelp_green", 0.f, 0.f,   0.f),
        cyl("frond_b",     0.04f, 2.8f, "kelp_green", 0.f, 0.15f, 0.10f),
        icosphere("bulb",  0.08f, "kelp_green", 3.6f),
    });
}

// Lamp post: base disc + pole + lamp head. Mirrors generators/lua/object/lamp.lua.
std::shared_ptr<Mc3Object> make_lamp_post(const std::string& mat, float h = 4.5f) {
    return Mc3Object::makeGroup("lamp_post", {
        cyl("base",  0.12f,  0.06f, mat),
        cyl("pole",  0.055f, h,     mat),
        box("head",  0.35f,  0.18f, 0.35f, mat, h),
    });
}

// Simple flower: tiny stem cylinder + small flat disc box.
std::shared_ptr<Mc3Object> make_flower(const std::string& petal_mat) {
    return Mc3Object::makeGroup("flower", {
        cyl("stem",   0.015f, 0.25f, "grass_tall"),
        box("petals", 0.18f,  0.04f, 0.18f, petal_mat, 0.24f),
    });
}

// Tropical fern: no trunk -- thin blade-like fronds radiating from a low
// base point, 60 degrees apart.
std::shared_ptr<Mc3Object> tropical_fern() {
    return Mc3Object::makeGroup("fern", {
        box("frond_a", 0.15f, 0.06f, 2.2f, "foliage_tropical", 0.5f, 0.f, 0.f,   0.f),
        box("frond_b", 0.15f, 0.06f, 2.0f, "foliage_tropical", 0.5f, 0.f, 0.f,  60.f),
        box("frond_c", 0.15f, 0.06f, 2.0f, "foliage_tropical", 0.5f, 0.f, 0.f, 120.f),
        box("frond_d", 0.15f, 0.06f, 2.2f, "foliage_tropical", 0.5f, 0.f, 0.f, 180.f),
        box("frond_e", 0.15f, 0.06f, 2.0f, "foliage_tropical", 0.5f, 0.f, 0.f, 240.f),
        box("frond_f", 0.15f, 0.06f, 2.0f, "foliage_tropical", 0.5f, 0.f, 0.f, 300.f),
    });
}

// Bamboo cluster: several thin canes of varying height/radius/offset,
// clustered close together.
std::shared_ptr<Mc3Object> bamboo_cluster() {
    return Mc3Object::makeGroup("bamboo", {
        cyl("cane_a", 0.060f, 4.5f, "bamboo_cane", 0.f,  0.15f,  0.00f),
        cyl("cane_b", 0.050f, 3.8f, "bamboo_cane", 0.f, -0.15f,  0.10f),
        cyl("cane_c", 0.055f, 4.2f, "bamboo_cane", 0.f,  0.05f, -0.20f),
        cyl("cane_d", 0.050f, 3.5f, "bamboo_cane", 0.f, -0.20f, -0.05f),
    });
}

// R113 v1 (docs/world-composer-design.md §7/§11) -- a detached gable-
// roofed house, a MECHANICAL PORT of generators/lua/building/
// simple_house.lua's own default gable geometry (w=10, d=8, wh=3.2,
// wt=0.30, rh=2.5) into a native C++ Mc3Object definition, per the design
// doc's own recommendation: de-risk the first real AssetRegistry+
// Mc3AssetMetadata usage with proven geometry rather than authoring new
// content. Uses the SAME numbers simple_house.lua uses (after this same
// session's own base-vs-center fix to that file, see its own comments) --
// this is the corrected geometry, not a copy of the pre-fix bug.
std::shared_ptr<Mc3Object> make_house_gable() {
    const float w = 10.0f, d = 8.0f, wh = 3.2f, wt = 0.30f, rh = 2.5f;
    const float half_w = w / 2.0f, half_d = d / 2.0f;
    const float slope_w = std::sqrt((w / 2.0f) * (w / 2.0f) + rh * rh) + 0.4f;
    const float angle_deg = std::atan(rh / (w / 2.0f)) * (180.0f / 3.14159265358979323846f);

    auto roof_l = box("roof_l", slope_w, 0.25f, d + wt * 2.0f + 0.2f, "roof_tile_red",
                       wh + rh / 2.0f - 0.125f, -w / 4.0f, 0.0f);
    roof_l->transform.rotation = {0.0f, 0.0f, -angle_deg};
    auto roof_r = box("roof_r", slope_w, 0.25f, d + wt * 2.0f + 0.2f, "roof_tile_red",
                       wh + rh / 2.0f - 0.125f, w / 4.0f, 0.0f);
    roof_r->transform.rotation = {0.0f, 0.0f, angle_deg};

    auto house = Mc3Object::makeGroup("house_gable_default", {
        box("floor", w + wt * 2.0f, 0.20f, d + wt * 2.0f, "concrete", -0.10f),
        box("wall_front", w + wt * 2.0f, wh, wt, "plaster_white", 0.0f, 0.0f,  half_d + wt / 2.0f),
        box("wall_back",  w + wt * 2.0f, wh, wt, "plaster_white", 0.0f, 0.0f, -(half_d + wt / 2.0f)),
        box("wall_left",  wt, wh, d, "plaster_white", 0.0f, -(half_w + wt / 2.0f), 0.0f),
        box("wall_right", wt, wh, d, "plaster_white", 0.0f,  (half_w + wt / 2.0f), 0.0f),
        box("door", 1.0f, 2.1f, wt + 0.002f, "wood_door_panel", 0.0f, 0.0f, half_d + wt / 2.0f + 0.001f),
        box("win_front_l", 1.2f, 1.0f, wt + 0.002f, "glass_clear", 1.60f, -w / 4.0f, half_d + wt / 2.0f + 0.001f),
        box("win_front_r", 1.2f, 1.0f, wt + 0.002f, "glass_clear", 1.60f,  w / 4.0f, half_d + wt / 2.0f + 0.001f),
        roof_l,
        roof_r,
        box("ridge", 0.30f, 0.20f, d + wt * 2.0f + 0.4f, "roof_tile_red", wh + rh - 0.10f),
        box("gable_front", w + wt * 2.0f, rh, wt, "plaster_white", wh, 0.0f,  half_d + wt / 2.0f),
        box("gable_back",  w + wt * 2.0f, rh, wt, "plaster_white", wh, 0.0f, -(half_d + wt / 2.0f)),
    });

    Mc3AssetMetadata meta;
    meta.category      = "house";
    meta.subcategory    = "detached";
    meta.semanticTags   = {"residential", "detached", "gable_roof"};
    meta.styleTags       = {"central_europe", "gable_roof"};
    meta.regionTags      = {"central_europe"};
    meta.nominalSize     = {w + wt * 2.0f, wh + rh, d + wt * 2.0f};
    meta.boundsMin        = {-(half_w + wt), 0.0f, -(half_d + wt)};
    meta.boundsMax        = { (half_w + wt), wh + rh, (half_d + wt)};
    meta.facing           = "+Z";
    meta.materialSlots    = {"wall", "roof"};
    meta.collisionProxy   = "box";
    meta.instancingEligible = true;
    meta.shadowPolicy     = "cast_receive";
    meta.selectionWeight  = 1.0f;
    meta.license          = "MIT";
    meta.provenance       = "MeshWorld procedural (mechanical port of "
                             "generators/lua/building/simple_house.lua's gable geometry)";
    meta.sourceGeneratorOrHash = "lua.building.simple_house.standard";
    meta.semanticVersion       = "0.1.0";
    house->assetMetadata = std::move(meta);

    return house;
}

// R126 (docs/world-composer-design.md v2) -- a detached, 3-story,
// flat-roofed apartment building, following make_house_gable()'s own
// "mechanical port of proven, simple box geometry, not new art" v1
// authoring convention: wider/deeper than the single-family house
// (w=14, d=10 vs the house's w=10, d=8) with 3 stacked floors of front
// windows instead of the house's single row, and a real flat roof slab
// instead of a gable -- apartment blocks in this project's own style
// catalog (data/taxonomy/taxonomy.json's "object.roof" family) are flat-
// or low-slope, not gabled, so this asset deliberately carries only the
// generic "central_europe" style tag, NOT "gable_roof" (see
// BuildingComposer.cpp's own comment on why the apartment query omits
// StyleProfile::roofFamily).
std::shared_ptr<Mc3Object> make_apartment_block() {
    const float w = 14.0f, d = 10.0f, floor_h = 3.0f, wt = 0.30f;
    constexpr int floors = 3;
    const float wall_h = floor_h * static_cast<float>(floors);
    const float half_w = w / 2.0f, half_d = d / 2.0f;

    std::vector<std::shared_ptr<Mc3Object>> parts = {
        box("floor", w + wt * 2.0f, 0.20f, d + wt * 2.0f, "concrete", -0.10f),
        box("wall_front", w + wt * 2.0f, wall_h, wt, "plaster_beige", 0.0f, 0.0f,  half_d + wt / 2.0f),
        box("wall_back",  w + wt * 2.0f, wall_h, wt, "plaster_beige", 0.0f, 0.0f, -(half_d + wt / 2.0f)),
        box("wall_left",  wt, wall_h, d, "plaster_beige", 0.0f, -(half_w + wt / 2.0f), 0.0f),
        box("wall_right", wt, wall_h, d, "plaster_beige", 0.0f,  (half_w + wt / 2.0f), 0.0f),
        // Entrance door offset toward one side (like a real apartment
        // block's single street entrance), not centered like the house's.
        box("door", 1.2f, 2.2f, wt + 0.002f, "wood_door_panel",
            0.0f, -half_w / 2.0f, half_d + wt / 2.0f + 0.001f),
        box("roof_flat", w + wt * 2.0f + 0.4f, 0.30f, d + wt * 2.0f + 0.4f, "concrete", wall_h),
    };

    // 3 floors x 3 window columns on the front facade -- the visual cue
    // that distinguishes this from the single-story house (which has just
    // 2 front windows), reusing the exact same per-window box() call
    // convention (name/size/material/base_y/cx/cz).
    const std::array<float, 3> col_x = {-w / 3.0f, 0.0f, w / 3.0f};
    for (int floor = 0; floor < floors; ++floor) {
        const float win_y = floor_h * static_cast<float>(floor) + 1.6f;
        for (std::size_t col = 0; col < col_x.size(); ++col) {
            parts.push_back(box(
                "win_f" + std::to_string(floor) + "_c" + std::to_string(col),
                1.1f, 1.3f, wt + 0.002f, "glass_clear",
                win_y, col_x[col], half_d + wt / 2.0f + 0.001f));
        }
    }

    auto apartment = Mc3Object::makeGroup("apartment.block.wide_01", parts);

    Mc3AssetMetadata meta;
    meta.category       = "apartment";
    meta.subcategory     = "multi_unit";
    meta.semanticTags    = {"residential", "multi_unit", "flat_roof"};
    meta.styleTags       = {"central_europe"};
    meta.regionTags      = {"central_europe"};
    meta.nominalSize     = {w + wt * 2.0f, wall_h + 0.30f, d + wt * 2.0f};
    meta.boundsMin       = {-(half_w + wt), 0.0f, -(half_d + wt)};
    meta.boundsMax       = { (half_w + wt), wall_h + 0.30f, (half_d + wt)};
    meta.facing          = "+Z";
    meta.materialSlots   = {"wall", "roof"};
    meta.collisionProxy  = "box";
    meta.instancingEligible = true;
    meta.shadowPolicy    = "cast_receive";
    meta.selectionWeight = 1.0f;
    meta.license         = "MIT";
    meta.provenance      = "MeshWorld procedural (R126, mirrors make_house_gable()'s "
                            "own box-composition authoring convention)";
    meta.sourceGeneratorOrHash = "cpp.object.composer.apartment_block.wide_01";
    meta.semanticVersion       = "0.1.0";
    apartment->assetMetadata = std::move(meta);

    return apartment;
}

// R127 (docs/world-composer-design.md v2) -- a narrow, 2-story, flat-
// roofed mixed-use building: a tall ground-floor shop (glazed shopfront +
// awning, cross-checked against ShopStreetGenerator.cpp's own 3.5-5.5m
// shop height / 9m depth / "awning_stripe"-material awning-plane
// convention so the composer path isn't a visual regression versus the
// legacy generator it will eventually replace) with one plain
// residential/office floor above -- the common Central-European
// "polyfunkcni dum" pattern, narrower street frontage than the house
// (w=8 vs the house's w=10) matching a real shop-street lot's own tight
// frontage. The ground-floor glazing's width/height deliberately echoes
// the real R112 mc3lib content's window.shopfront.large.urban_01 asset
// (2.4x2.0m nominalSize, styleTags ["modern","urban"]) -- literally
// attaching THAT socket-based facade asset here would need R104 (MeshCraft
// <script> execution, explicitly out of scope per NEXT.md's own "do not
// do yet" list), so this box-composed glazing is a documented, honest
// stand-in with matching real-world proportions, not a silent
// downgrade. Carries BOTH "central_europe" (so BuildingComposer's shared
// facadeFamily-tag query still matches the shipped central_europe_default
// StyleProfile, same as house/apartment) and "modern" (the real, honest
// style of the shopfront glazing it echoes).
std::shared_ptr<Mc3Object> make_shop_building() {
    const float w = 8.0f, d = 9.0f, wt = 0.30f;
    const float ground_floor_h = 4.5f, upper_floor_h = 3.0f;
    const float wall_h = ground_floor_h + upper_floor_h;
    const float half_w = w / 2.0f, half_d = d / 2.0f;
    const float front_z = half_d + wt / 2.0f;

    auto shop = Mc3Object::makeGroup("shop.building.storefront_01", {
        box("floor", w + wt * 2.0f, 0.20f, d + wt * 2.0f, "concrete", -0.10f),
        box("wall_front", w + wt * 2.0f, wall_h, wt, "plaster_blue", 0.0f, 0.0f,  front_z),
        box("wall_back",  w + wt * 2.0f, wall_h, wt, "plaster_blue", 0.0f, 0.0f, -front_z),
        box("wall_left",  wt, wall_h, d, "plaster_blue", 0.0f, -(half_w + wt / 2.0f), 0.0f),
        box("wall_right", wt, wall_h, d, "plaster_blue", 0.0f,  (half_w + wt / 2.0f), 0.0f),
        // Ground-floor entrance door, offset to one side, mirroring the
        // real shopfront window taking up most of the remaining frontage.
        box("door", 1.0f, 2.2f, wt + 0.002f, "wood_door_panel",
            0.0f, -2.75f, front_z + 0.001f),
        // Large ground-floor shopfront glazing -- see the function's own
        // doc comment for why this echoes window.shopfront.large.urban_01's
        // real proportions instead of instancing it directly.
        box("shopfront_window", 5.4f, 2.0f, wt + 0.002f, "glass_clear",
            0.3f, 0.65f, front_z + 0.001f),
        // Awning over the shopfront, same material/height precedent as
        // ShopStreetGenerator.cpp's own awning planes.
        box("awning", 7.0f, 0.10f, 1.2f, "awning_stripe",
            2.6f, 0.0f, front_z + 0.6f),
        box("roof_flat", w + wt * 2.0f + 0.4f, 0.30f, d + wt * 2.0f + 0.4f, "concrete", wall_h),
        // Upper-floor windows -- one plain residential/office row above
        // the shop, same per-window box() convention as the apartment's.
        box("win_u0", 1.0f, 1.2f, wt + 0.002f, "glass_clear",
            ground_floor_h + 1.4f, -2.4f, front_z + 0.001f),
        box("win_u1", 1.0f, 1.2f, wt + 0.002f, "glass_clear",
            ground_floor_h + 1.4f,  0.0f, front_z + 0.001f),
        box("win_u2", 1.0f, 1.2f, wt + 0.002f, "glass_clear",
            ground_floor_h + 1.4f,  2.4f, front_z + 0.001f),
    });

    Mc3AssetMetadata meta;
    meta.category       = "shop";
    meta.subcategory     = "ground_floor_storefront";
    meta.semanticTags    = {"commercial", "retail", "mixed_use", "flat_roof"};
    meta.styleTags       = {"central_europe", "modern"};
    meta.regionTags      = {"central_europe"};
    meta.nominalSize     = {w + wt * 2.0f, wall_h + 0.30f, d + wt * 2.0f};
    meta.boundsMin       = {-(half_w + wt), 0.0f, -(half_d + wt)};
    meta.boundsMax       = { (half_w + wt), wall_h + 0.30f, (half_d + wt)};
    meta.facing          = "+Z";
    meta.materialSlots   = {"wall", "roof"};
    meta.collisionProxy  = "box";
    meta.instancingEligible = true;
    meta.shadowPolicy    = "cast_receive";
    meta.selectionWeight = 1.0f;
    meta.license         = "MIT";
    meta.provenance      = "MeshWorld procedural (R127, mirrors make_apartment_block()'s "
                            "own box-composition authoring convention)";
    meta.sourceGeneratorOrHash = "cpp.object.composer.shop_street.storefront_01";
    meta.semanticVersion       = "0.1.0";
    shop->assetMetadata = std::move(meta);

    return shop;
}

// R128 (city showcase completion) -- the one genuinely new, one-off
// landmark/monument definition NEXT.md's own "R114 v2" task calls for,
// mirroring how house.gable.wide_01 was added (a real new definition, not
// a re-tagged existing one) while staying with THIS file's own simpler
// box/cylinder/cone-composition convention (make_house_gable() et al.)
// rather that convention's `<script>`-driven socket variant --
// house.gable.wide_01 needed sockets because it composes R112 window/
// door/roof library content; a landmark has no such requirement, so the
// simpler, more robust convention is the right one here. A small clock
// tower: a granite plinth, a plastered shaft, a stone cornice, one clock
// face per side, and a tiled pyramidal spire -- placed via
// BuildingComposer's own `ctx.landmark` field (WorldConfig::landmarks),
// NOT through the generic Parcel/AssetRegistry query mechanism every
// other composer asset goes through.
std::shared_ptr<Mc3Object> make_landmark_clocktower() {
    const float base_w = 5.0f, base_h = 1.2f;
    const float shaft_w = 3.2f, shaft_h = 11.0f;
    const float cornice_w = 3.8f, cornice_h = 0.4f;
    const float clock_size = 1.4f, clock_h = 1.4f;
    const float spire_r = 2.0f, spire_h = 3.5f;
    const float half_shaft = shaft_w / 2.0f;

    const float cornice_base_y = base_h + shaft_h;
    const float clock_base_y   = cornice_base_y + cornice_h;
    const float spire_base_y   = clock_base_y + clock_h;
    const float total_h        = spire_base_y + spire_h;

    auto tower = Mc3Object::makeGroup("landmark.clocktower_01", {
        box("base",    base_w,    base_h,    base_w,    "stone_granite", 0.0f),
        box("shaft",   shaft_w,   shaft_h,   shaft_w,   "plaster_cream", base_h),
        box("cornice", cornice_w, cornice_h, cornice_w, "stone_light",   cornice_base_y),
        box("clock_n", clock_size, clock_h, 0.10f, "metal_chrome",
            clock_base_y, 0.0f, -(half_shaft + 0.05f)),
        box("clock_s", clock_size, clock_h, 0.10f, "metal_chrome",
            clock_base_y, 0.0f,  (half_shaft + 0.05f)),
        box("clock_e", 0.10f, clock_h, clock_size, "metal_chrome",
            clock_base_y,  (half_shaft + 0.05f), 0.0f),
        box("clock_w", 0.10f, clock_h, clock_size, "metal_chrome",
            clock_base_y, -(half_shaft + 0.05f), 0.0f),
        cone("spire", spire_r, spire_h, "roof_tile_red", spire_base_y),
    });

    Mc3AssetMetadata meta;
    meta.category       = "landmark";
    meta.subcategory     = "clocktower";
    meta.semanticTags    = {"landmark", "monument", "civic"};
    meta.styleTags       = {"central_europe"};
    meta.regionTags      = {"central_europe"};
    meta.nominalSize     = {base_w, total_h, base_w};
    meta.boundsMin       = {-(base_w / 2.0f), 0.0f, -(base_w / 2.0f)};
    meta.boundsMax       = { (base_w / 2.0f), total_h, (base_w / 2.0f)};
    meta.facing          = "+Z";
    meta.materialSlots   = {"stone", "metal", "roof"};
    meta.collisionProxy  = "box";
    meta.instancingEligible = true;
    meta.shadowPolicy    = "cast_receive";
    meta.selectionWeight = 1.0f;
    meta.license         = "MIT";
    meta.provenance      = "MeshWorld procedural (R128, mirrors make_house_gable()'s "
                            "own box/cone-composition authoring convention)";
    meta.sourceGeneratorOrHash = "cpp.object.composer.landmark.clocktower_01";
    meta.semanticVersion       = "0.1.0";
    tower->assetMetadata = std::move(meta);

    return tower;
}

} // namespace

// ---------------------------------------------------------------------------

ObjectDefinitionLibrary& ObjectDefinitionLibrary::instance() {
    static ObjectDefinitionLibrary lib;
    return lib;
}

void ObjectDefinitionLibrary::register_definition(
    std::string id, std::shared_ptr<Mc3Object> obj) {
    defs_[std::move(id)] = std::move(obj);
}

bool ObjectDefinitionLibrary::has(const std::string& id) const {
    return defs_.count(id) > 0;
}

std::shared_ptr<Mc3Object> ObjectDefinitionLibrary::get(const std::string& id) const {
    auto it = defs_.find(id);
    return it == defs_.end() ? nullptr : it->second;
}

void ObjectDefinitionLibrary::load_all() {
    // ── Deciduous trees ───────────────────────────────────────────────────────
    register_definition("tree_oak",
        deciduous_tree("wood_bark_dark",  "foliage_oak",      0.15f, 4.0f, 2.8f));
    register_definition("tree_lime",
        deciduous_tree("wood_bark_light", "foliage_linden",   0.13f, 3.5f, 2.5f));
    register_definition("tree_birch",
        birch_tree("wood_birch",      "foliage_birch",    0.10f, 5.0f, 2.0f));
    register_definition("tree_chestnut",
        deciduous_tree("wood_bark_dark",  "foliage_chestnut", 0.18f, 4.5f, 3.2f));
    // Real bug (found 2026-07-11, T-series backlog triage): ForestGenerator.cpp
    // has always instanced "tree_pine"/"tree_beech" (src/generators/ForestGenerator.cpp:22,87),
    // but neither was ever registered here -- only "tree_pine_mountain" (below)
    // and no beech at all -- so WorldRenderer::inject_definitions() silently
    // no-ops for both and these ForestGenerator trees have been invisible in
    // every rendered forest chunk. "tree_pine" reuses pine_tree()'s exact shape
    // (a second, independent registration -- NOT an alias for
    // "tree_pine_mountain", which MountainGenerator.cpp's own instance() calls
    // still need unchanged); "tree_beech" is a new, genuine broadleaf
    // definition, parameterized between tree_oak/tree_chestnut.
    register_definition("tree_pine", pine_tree());
    register_definition("tree_beech",
        deciduous_tree("wood_bark_dark", "foliage_beech", 0.16f, 4.2f, 3.0f));
    register_definition("tree_apple",
        deciduous_tree("wood_bark_light", "foliage_fruit",    0.10f, 3.5f, 2.2f));
    register_definition("tree_cherry",
        deciduous_tree("wood_bark_dark",  "foliage_fruit",    0.09f, 3.8f, 2.0f));
    register_definition("tree_pear",
        birch_tree("wood_bark_light", "foliage_fruit",    0.10f, 4.0f, 2.0f));

    // ── Specialty trees ───────────────────────────────────────────────────────
    register_definition("tree_willow",        willow_tree());
    register_definition("tree_palm",          palm_tree());
    register_definition("tree_pine_mountain", pine_tree());
    register_definition("tree_dead_gnarled",  dead_tree());
    register_definition("tree_bare_winter",   bare_tree());

    // ── Jungle (same triage pass, 2026-07-11): JungleGenerator.cpp has always
    // instanced "tree_banyan"/"tree_tropical_fern"/"tree_bamboo", none of
    // which were ever registered -- silently invisible in every rendered
    // jungle chunk. "tree_palm" above already covers the 4th species it uses.
    register_definition("tree_banyan",
        deciduous_tree("wood_bark_dark", "foliage_tropical", 0.25f, 3.0f, 4.5f));
    register_definition("tree_tropical_fern", tropical_fern());
    register_definition("tree_bamboo",        bamboo_cluster());

    // ── Lamp posts ────────────────────────────────────────────────────────────
    register_definition("lamp_post_ornate", make_lamp_post("metal_lamp_ornate"));
    register_definition("lamp_post_simple", make_lamp_post("metal_lamp"));

    // ── Benches ───────────────────────────────────────────────────────────────
    register_definition("bench_park",   make_bench("wood_bench",  "metal_lamp"));
    register_definition("bench_stone",  make_bench("stone_light", "stone_light"));
    register_definition("bench_street", make_bench("wood_bench",  "metal_dark"));

    // ── Ground cover / shrubs ─────────────────────────────────────────────────
    register_definition("shrub_round",       make_shrub());
    register_definition("grass_tuft_tall",   make_grass_tuft());
    register_definition("plant_desert_scrub",make_simple_plant("plant_scrub",   0.60f, 0.40f, 0.55f));
    register_definition("plant_sea_weed",    make_simple_plant("plant_tropical",0.25f, 0.55f, 0.20f));
    register_definition("plant_sea_grass",   make_simple_plant("plant_tropical",0.35f, 0.45f, 0.30f));
    register_definition("plant_marsh_grass", make_simple_plant("plant_marsh",   0.30f, 0.70f, 0.25f));
    register_definition("plant_lichen",      make_flat_plant("lichen_grey",     0.55f, 0.08f, 0.45f));

    // ── Flowers ───────────────────────────────────────────────────────────────
    register_definition("flower_red",    make_flower("flower_red"));
    register_definition("flower_yellow", make_flower("flower_yellow"));
    register_definition("flower_daisy",  make_flower("flower_daisy"));
    // Same triage pass, 2026-07-11: MeadowGenerator.cpp has always instanced
    // these 3 too, alongside flower_daisy above (which WAS already
    // registered) -- none of these 3 were, silently invisible in every
    // rendered meadow chunk.
    register_definition("flower_poppy",     make_flower("flower_poppy"));
    register_definition("flower_bluebell",  make_flower("flower_bluebell"));
    register_definition("flower_buttercup", make_flower("flower_buttercup"));

    // ── Fungi / nature objects ────────────────────────────────────────────────
    register_definition("mushroom_brown", make_mushroom());

    // ── Rocks / minerals ─────────────────────────────────────────────────────
    register_definition("rock_grey_small",  make_rock("rock_grey",  0.80f, 0.38f, 0.65f));
    register_definition("rock_mossy",       make_rock("rock_mossy", 1.00f, 0.48f, 0.85f));
    register_definition("rock_pile_small",  make_rock_pile());
    register_definition("crystal_blue",     make_crystal());

    // ── Desert / exotic ──────────────────────────────────────────────────────
    register_definition("cactus_saguaro", make_cactus());

    // ── Beach ─────────────────────────────────────────────────────────────────
    register_definition("shell_seashell", make_seashell());

    // ── Aquatic (MAP20, M326) ────────────────────────────────────────────────
    register_definition("coral_branching", make_coral());
    register_definition("kelp_strand",     make_kelp());

    // ── Cave (MAP21, M333/M335) ──────────────────────────────────────────────
    register_definition("stalactite_hanging", make_stalactite());
    register_definition("stalagmite_rising",  make_stalagmite());
    register_definition("rock_rubble",        make_rubble());

    // ── Buildings (R113 v1, world composer) ─────────────────────────────────
    register_definition("house_gable_default", make_house_gable());
    // R126 (BuildingComposer v2, apartment_block) -- see make_apartment_block()'s
    // own doc comment.
    register_definition("apartment.block.wide_01", make_apartment_block());
    // R127 (BuildingComposer v2, shop_street) -- see make_shop_building()'s
    // own doc comment.
    register_definition("shop.building.storefront_01", make_shop_building());
    // R128 (city showcase completion) -- see make_landmark_clocktower()'s
    // own doc comment.
    register_definition("landmark.clocktower_01", make_landmark_clocktower());
}

namespace {
void collect_instance_ids(const MeshCraft::Mc3::Mc3Object& obj, std::vector<std::string>& out) {
    using namespace MeshCraft::Mc3;
    if (obj.type == ObjectType::Instance && !obj.definition.empty())
        out.push_back(obj.definition);
    for (const auto& child : obj.children)
        if (child) collect_instance_ids(*child, out);
}
} // namespace

void resolve_instance_definitions(MeshCraft::Mc3::Mc3Document& doc) {
    auto& lib = ObjectDefinitionLibrary::instance();
    // Fixed-point loop: injecting a definition can itself introduce new,
    // previously-unseen instance refs of its own (e.g. a compiled modular
    // building like house.gable.modular_01 carries its own window/door/
    // roof children, embedded as instances at registration time) -- keep
    // rescanning doc.objects AND every already-injected definition until
    // a full pass finds nothing new to resolve.
    bool progress = true;
    while (progress) {
        progress = false;
        std::vector<std::string> ids;
        for (const auto& obj : doc.objects)
            if (obj) collect_instance_ids(*obj, ids);
        for (const auto& [def_id, def] : doc.definitions)
            if (def) collect_instance_ids(*def, ids);

        for (const auto& id : ids) {
            if (doc.definitions.count(id)) continue;
            auto def = lib.get(id);
            if (!def) {
                // A compiled modular building's own children reference
                // imports by their qualified "<alias>:<definitionId>"
                // form -- the literal string passed to
                // Mc3ScriptRunner's def:place()/place_at() calls at
                // compose time (e.g. "windows:window.residential.double.
                // classic_01"). ObjectDefinitionLibrary only ever
                // registers the BARE id (register_mc3lib_batch strips
                // the alias before registering, since a definition's own
                // internal cross-references, e.g. Mc3AssetMetadata.lods,
                // use the bare id too) -- retry with the alias stripped.
                const auto sep = id.find(':');
                if (sep != std::string::npos) def = lib.get(id.substr(sep + 1));
            }
            if (def) {
                doc.defineObject(id, def);
                progress = true;
            }
        }
    }
}

} // namespace MeshWorld
