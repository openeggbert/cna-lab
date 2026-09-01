// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MeshWorldBuildMc3Lib -- R112 (workstream R4, mesh_world_revival.md
// §4.4/§4.5/§5.1/§11): builds the first real urban mc3lib content batch
// and writes it to data/mc3lib/ as genuine .mc3lib.json library files,
// loadable through the existing R101/R102 Mc3ImportResolver mechanism
// (mesh-craft/mc3) -- NOT baked directly into ObjectDefinitionLibrary the
// way R113 v1's single house_gable_default asset was (that was an
// explicit, documented "no R112 dependency" shortcut for v1; this tool is
// what actually produces real R112 content for the composer to import).
//
// Covers all 7 categories R112's own plan.md text names -- window, door,
// roof, facade module, street furniture (streetlamp/bench), vehicle
// (car/van), prop (mailbox/trash bin) -- 3-5 coherent variants each
// (deeper coverage pass, up from 2 each in the first pass), plus one
// low-LOD proxy per variant. Every "_02"-style id is a genuine same-
// family variant per mesh_world_revival.md §4.5's own worked example
// (e.g. window.residential.double.classic_01/02); every new subcategory
// (single window, shopfront window, apartment door, bench, trash bin,
// van) is a deliberately different kind of thing, not just a recolor.
// R106 (a real material catalogue) is still open -- this batch reuses
// materials already registered in BuiltinMaterials.cpp rather than
// blocking on R106 first. Not every category has a placement consumer
// yet: BuildingComposer places streetlamp/bench (street_furniture),
// mailbox/trash_bin (prop), and car/van (vehicle) -- no socket needed,
// parcel/street-level -- but window/door/roof/facade_module are
// registered+queryable only; consuming them needs R103's socket-aware
// placement, not yet built.
//
// Re-run this tool whenever the content needs regenerating (e.g. a schema
// change) -- it is deterministic and always overwrites the same 7 files.

#include <MeshCraft/Mc3/Mc3AssetMetadata.hpp>
#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>
#include <MeshCraft/Mc3/Mc3Primitive.hpp>

#include <cmath>
#include <cstdio>
#include <filesystem>

using namespace MeshCraft::Mc3;

namespace {

// Shape helpers -- same "base_y is a BASE elevation, not a center"
// convention as ObjectDefinitionLibrary.cpp's own box()/cyl() helpers
// (position.y = base_y + size/2 internally), deliberately kept identical
// across this codebase after the base-vs-center bug found and fixed in
// simple_house.lua/house/detached.lua (R113 v1) -- getting this backwards
// silently floats geometry above where it should sit.

std::shared_ptr<Mc3Object> box(const std::string& name,
                                float sx, float sy, float sz, const std::string& mat,
                                float base_y = 0.f, float cx = 0.f, float cz = 0.f) {
    auto o = Mc3Object::makeBox(name, {sx, sy, sz}, mat);
    o->transform.position = {cx, base_y + sy / 2.f, cz};
    return o;
}

std::shared_ptr<Mc3Object> cyl(const std::string& name,
                                float r, float h, const std::string& mat,
                                float base_y = 0.f, float cx = 0.f, float cz = 0.f) {
    auto o = Mc3Object::makeCylinder(name, r, h, 12, mat);
    o->transform.position = {cx, base_y + h / 2.f, cz};
    return o;
}

std::shared_ptr<Mc3Object> icosphere(const std::string& name,
                                      float r, const std::string& mat,
                                      float center_y = 0.f, float cx = 0.f, float cz = 0.f) {
    auto o = Mc3Object::makeIcoSphere(name, r, 2, mat);
    o->transform.position = {cx, center_y, cz};
    return o;
}

// Vehicle wheel: a cylinder tipped 90 degrees around Z so its rolling
// axis is horizontal (X) instead of the default vertical (Y) -- once
// tipped, "resting on the ground" means the CENTER sits at y = radius,
// not base_y + h/2 like an upright cyl(); a different enough convention
// from the other helpers that it gets its own function rather than an
// extra flag on cyl().
std::shared_ptr<Mc3Object> wheel(const std::string& name,
                                  float r, float w, const std::string& mat,
                                  float cx, float cz) {
    auto o = Mc3Object::makeCylinder(name, r, w, 12, mat);
    o->transform.position = {cx, r, cz};
    o->transform.rotation = {0.f, 0.f, 90.f};
    return o;
}

Mc3AssetMetadata make_meta(const std::string& category, const std::string& subcategory,
                            std::vector<std::string> styleTags,
                            std::array<float, 3> nominalSize,
                            std::array<float, 3> boundsMin, std::array<float, 3> boundsMax,
                            std::string facing, std::string lowLodId,
                            std::string collisionProxy = "none") {
    Mc3AssetMetadata meta;
    meta.category    = category;
    meta.subcategory = subcategory;
    meta.styleTags    = std::move(styleTags);
    meta.nominalSize  = nominalSize;
    meta.boundsMin    = boundsMin;
    meta.boundsMax    = boundsMax;
    meta.facing       = std::move(facing);
    // R133 -- reusable assets are passable unless their author explicitly
    // opts into the currently supported player blocker.  The first R112
    // batch used box for every asset, which would make benches, streetlamps,
    // mailboxes, windows and thin roof modules obstruct a player once MC3
    // instance collision is enabled. Vehicles opt in at their call sites;
    // structural building definitions use their own explicit metadata below.
    meta.collisionProxy = std::move(collisionProxy);
    meta.instancingEligible = true;
    meta.shadowPolicy = "cast_receive";
    meta.selectionWeight = 1.0f;
    meta.license = "MIT";
    meta.provenance = "MeshWorld procedural (R112, build_mc3lib_content.cpp)";
    meta.sourceGeneratorOrHash = "cpp.mc3lib.urban_core.v1";
    meta.semanticVersion = "1.0.0";
    if (!lowLodId.empty()) meta.lods["low"] = lowLodId;
    return meta;
}

void write_library(const std::string& libraryNamespace, const std::string& version,
                    const std::vector<std::pair<std::string, std::shared_ptr<Mc3Object>>>& defs,
                    const std::filesystem::path& outDir) {
    Mc3Document doc;
    doc.model = libraryNamespace;
    doc.library = Mc3LibraryInfo{libraryNamespace, version, ""};
    for (const auto& [id, obj] : defs) doc.defineObject(id, obj);
    doc.library->contentHash = "sha256:" + doc.computeLibraryContentHash();

    const auto path = outDir / (libraryNamespace + "-" + version + ".mc3lib.json");
    doc.saveToLibraryJsonFile(path);
    std::printf("wrote %s (%zu definitions)\n", path.string().c_str(), defs.size());
}

// --- windows -----------------------------------------------------------

void build_windows_library(const std::filesystem::path& outDir) {
    // classic_01: wood frame, 4-pane cross mullion.
    auto classic_frame = box("frame", 1.2f, 1.0f, 0.10f, "wood_window_frame");
    auto classic_glass  = box("glass", 1.04f, 0.84f, 0.03f, "glass_clear", 0.08f, 0.f, 0.01f);
    auto classic_mul_v  = box("mullion_v", 0.04f, 0.84f, 0.05f, "wood_window_frame", 0.08f, 0.f, 0.02f);
    auto classic_mul_h  = box("mullion_h", 1.04f, 0.04f, 0.05f, "wood_window_frame", 0.5f, 0.f, 0.02f);
    auto classic = Mc3Object::makeGroup("window.residential.double.classic_01",
        {classic_frame, classic_glass, classic_mul_v, classic_mul_h});
    classic->assetMetadata = make_meta(
        "window", "residential_double", {"central_europe"},
        {1.2f, 1.0f, 0.10f}, {-0.6f, 0.f, -0.05f}, {0.6f, 1.0f, 0.05f}, "-Z",
        "window.residential.double.classic_01.lod_low");

    auto classic_low = box("proxy", 1.2f, 1.0f, 0.10f, "wood_window_frame");
    classic_low->id = "window.residential.double.classic_01.lod_low";

    // modern_01: slim metal frame, single large pane.
    auto modern_frame = box("frame", 1.4f, 1.2f, 0.08f, "metal_dark");
    auto modern_glass  = box("glass", 1.28f, 1.08f, 0.03f, "glass_clear", 0.06f, 0.f, 0.01f);
    auto modern = Mc3Object::makeGroup("window.residential.double.modern_01",
        {modern_frame, modern_glass});
    modern->assetMetadata = make_meta(
        "window", "residential_double", {"modern"},
        {1.4f, 1.2f, 0.08f}, {-0.7f, 0.f, -0.04f}, {0.7f, 1.2f, 0.04f}, "-Z",
        "window.residential.double.modern_01.lod_low");

    auto modern_low = box("proxy", 1.4f, 1.2f, 0.08f, "metal_dark");
    modern_low->id = "window.residential.double.modern_01.lod_low";

    // classic_02: same family as classic_01 (deeper variant coverage,
    // mesh_world_revival.md §4.5's own "classic_01/classic_02" worked
    // example), with wood shutters either side.
    auto classic2_frame   = box("frame", 1.2f, 1.0f, 0.10f, "wood_window_frame");
    auto classic2_glass   = box("glass", 1.04f, 0.84f, 0.03f, "glass_clear", 0.08f, 0.f, 0.01f);
    auto classic2_mul_v   = box("mullion_v", 0.04f, 0.84f, 0.05f, "wood_window_frame", 0.08f, 0.f, 0.02f);
    auto classic2_mul_h   = box("mullion_h", 1.04f, 0.04f, 0.05f, "wood_window_frame", 0.5f, 0.f, 0.02f);
    auto classic2_shut_l  = box("shutter_l", 0.35f, 1.0f, 0.03f, "wood_bark_dark", 0.08f, -0.78f);
    auto classic2_shut_r  = box("shutter_r", 0.35f, 1.0f, 0.03f, "wood_bark_dark", 0.08f,  0.78f);
    auto classic2 = Mc3Object::makeGroup("window.residential.double.classic_02",
        {classic2_frame, classic2_glass, classic2_mul_v, classic2_mul_h, classic2_shut_l, classic2_shut_r});
    classic2->assetMetadata = make_meta(
        "window", "residential_double", {"central_europe"},
        {1.91f, 1.0f, 0.10f}, {-0.955f, 0.f, -0.05f}, {0.955f, 1.0f, 0.05f}, "-Z",
        "window.residential.double.classic_02.lod_low");

    auto classic2_low = box("proxy", 1.91f, 1.0f, 0.10f, "wood_window_frame");
    classic2_low->id = "window.residential.double.classic_02.lod_low";

    // single.classic_01: smaller single-pane window, no mullion --
    // genuinely different subcategory, not just a recolor.
    auto single_frame = box("frame", 0.65f, 1.0f, 0.10f, "wood_window_frame");
    auto single_glass = box("glass", 0.50f, 0.84f, 0.03f, "glass_clear", 0.08f, 0.f, 0.01f);
    auto single = Mc3Object::makeGroup("window.residential.single.classic_01",
        {single_frame, single_glass});
    single->assetMetadata = make_meta(
        "window", "residential_single", {"central_europe"},
        {0.65f, 1.0f, 0.10f}, {-0.325f, 0.f, -0.05f}, {0.325f, 1.0f, 0.05f}, "-Z",
        "window.residential.single.classic_01.lod_low");

    auto single_low = box("proxy", 0.65f, 1.0f, 0.10f, "wood_window_frame");
    single_low->id = "window.residential.single.classic_01.lod_low";

    // shopfront.large.urban_01 -- matches mesh_world_revival.md §4.5's own
    // explicit "window.shopfront.large.urban_03" example; a large single
    // pane, chrome frame, for shop_street/commercial buildings.
    auto shop_frame = box("frame", 2.4f, 2.0f, 0.08f, "metal_chrome");
    auto shop_glass = box("glass", 2.24f, 1.84f, 0.03f, "glass_clear", 0.08f, 0.f, 0.01f);
    auto shop = Mc3Object::makeGroup("window.shopfront.large.urban_01",
        {shop_frame, shop_glass});
    shop->assetMetadata = make_meta(
        "window", "shopfront_large", {"modern", "urban"},
        {2.4f, 2.0f, 0.08f}, {-1.2f, 0.f, -0.04f}, {1.2f, 2.0f, 0.04f}, "-Z",
        "window.shopfront.large.urban_01.lod_low");

    auto shop_low = box("proxy", 2.4f, 2.0f, 0.08f, "metal_chrome");
    shop_low->id = "window.shopfront.large.urban_01.lod_low";

    write_library("urban-windows", "1.0.0", {
        {"window.residential.double.classic_01", classic},
        {"window.residential.double.classic_01.lod_low", classic_low},
        {"window.residential.double.modern_01", modern},
        {"window.residential.double.modern_01.lod_low", modern_low},
        {"window.residential.double.classic_02", classic2},
        {"window.residential.double.classic_02.lod_low", classic2_low},
        {"window.residential.single.classic_01", single},
        {"window.residential.single.classic_01.lod_low", single_low},
        {"window.shopfront.large.urban_01", shop},
        {"window.shopfront.large.urban_01.lod_low", shop_low},
    }, outDir);
}

// --- doors ---------------------------------------------------------------

void build_doors_library(const std::filesystem::path& outDir) {
    // wood_panel_01: solid wood slab, frame backing, handle.
    auto panel_slab   = box("slab", 1.0f, 2.1f, 0.06f, "wood_door_panel");
    auto panel_frame  = box("frame", 1.1f, 2.2f, 0.03f, "wood_door_frame", 0.f, 0.f, -0.02f);
    auto panel_handle = box("handle", 0.03f, 0.15f, 0.04f, "metal_dark", 0.95f, 0.4f, 0.05f);
    auto panel = Mc3Object::makeGroup("door.residential.wood_panel_01",
        {panel_frame, panel_slab, panel_handle});
    panel->assetMetadata = make_meta(
        "door", "residential", {"central_europe"},
        {1.1f, 2.2f, 0.105f}, {-0.55f, 0.f, -0.035f}, {0.55f, 2.2f, 0.07f}, "-Z",
        "door.residential.wood_panel_01.lod_low");

    auto panel_low = box("proxy", 1.0f, 2.1f, 0.06f, "wood_door_panel");
    panel_low->id = "door.residential.wood_panel_01.lod_low";

    // wood_glass_01: wood slab with an upper glass insert.
    auto glass_slab   = box("slab", 1.0f, 2.1f, 0.06f, "wood_door_panel");
    auto glass_frame  = box("frame", 1.1f, 2.2f, 0.03f, "wood_door_frame", 0.f, 0.f, -0.02f);
    auto glass_insert = box("glass", 0.5f, 0.6f, 0.03f, "glass_clear", 1.3f, 0.f, 0.02f);
    auto glass_handle = box("handle", 0.03f, 0.15f, 0.04f, "metal_dark", 0.95f, 0.4f, 0.05f);
    auto glazed = Mc3Object::makeGroup("door.residential.wood_glass_01",
        {glass_frame, glass_slab, glass_insert, glass_handle});
    glazed->assetMetadata = make_meta(
        "door", "residential", {"central_europe"},
        {1.1f, 2.2f, 0.105f}, {-0.55f, 0.f, -0.035f}, {0.55f, 2.2f, 0.07f}, "-Z",
        "door.residential.wood_glass_01.lod_low");

    auto glazed_low = box("proxy", 1.0f, 2.1f, 0.06f, "wood_door_panel");
    glazed_low->id = "door.residential.wood_glass_01.lod_low";

    // wood_panel_02: same family as wood_panel_01, darker wood.
    auto panel2_slab   = box("slab", 1.0f, 2.1f, 0.06f, "wood_bark_dark");
    auto panel2_frame  = box("frame", 1.1f, 2.2f, 0.03f, "wood_door_frame", 0.f, 0.f, -0.02f);
    auto panel2_handle = box("handle", 0.03f, 0.15f, 0.04f, "metal_dark", 0.95f, 0.4f, 0.05f);
    auto panel2 = Mc3Object::makeGroup("door.residential.wood_panel_02",
        {panel2_frame, panel2_slab, panel2_handle});
    panel2->assetMetadata = make_meta(
        "door", "residential", {"central_europe"},
        {1.1f, 2.2f, 0.105f}, {-0.55f, 0.f, -0.035f}, {0.55f, 2.2f, 0.07f}, "-Z",
        "door.residential.wood_panel_02.lod_low");

    auto panel2_low = box("proxy", 1.0f, 2.1f, 0.06f, "wood_bark_dark");
    panel2_low->id = "door.residential.wood_panel_02.lod_low";

    // apartment.shared_entry_01 -- genuinely different subcategory: a
    // wider double door for a shared building entrance, matching
    // mesh_world_revival.md §4.5's own explicit example.
    const float ad_w = 1.6f, ad_h = 2.3f;
    auto apt_slab_l = box("slab_l", 0.78f, ad_h, 0.06f, "wood_door_panel", 0.f, -0.4f);
    auto apt_slab_r = box("slab_r", 0.78f, ad_h, 0.06f, "wood_door_panel", 0.f,  0.4f);
    auto apt_frame  = box("frame", ad_w + 0.1f, ad_h + 0.1f, 0.03f, "wood_door_frame", 0.f, 0.f, -0.02f);
    auto apt_bar    = box("kick_bar", ad_w - 0.1f, 0.05f, 0.03f, "metal_chrome", 0.9f, 0.f, 0.05f);
    auto apt = Mc3Object::makeGroup("door.apartment.shared_entry_01",
        {apt_frame, apt_slab_l, apt_slab_r, apt_bar});
    apt->assetMetadata = make_meta(
        "door", "apartment", {"central_europe"},
        {ad_w + 0.1f, ad_h + 0.1f, 0.10f}, {-(ad_w + 0.1f) / 2.f, 0.f, -0.035f},
        {(ad_w + 0.1f) / 2.f, ad_h + 0.1f, 0.065f}, "-Z",
        "door.apartment.shared_entry_01.lod_low");

    auto apt_low = box("proxy", ad_w, ad_h, 0.06f, "wood_door_panel");
    apt_low->id = "door.apartment.shared_entry_01.lod_low";

    write_library("urban-doors", "1.0.0", {
        {"door.residential.wood_panel_01", panel},
        {"door.residential.wood_panel_01.lod_low", panel_low},
        {"door.residential.wood_glass_01", glazed},
        {"door.residential.wood_glass_01.lod_low", glazed_low},
        {"door.residential.wood_panel_02", panel2},
        {"door.residential.wood_panel_02.lod_low", panel2_low},
        {"door.apartment.shared_entry_01", apt},
        {"door.apartment.shared_entry_01.lod_low", apt_low},
    }, outDir);
}

// --- street furniture ------------------------------------------------------

void build_street_furniture_library(const std::filesystem::path& outDir) {
    // classic_01: ornate pole + housing + glowing core.
    auto classic_base    = cyl("base",  0.12f, 0.15f, "metal_lamp_ornate");
    auto classic_pole    = cyl("pole",  0.05f, 2.70f, "metal_lamp_ornate", 0.15f);
    auto classic_housing = box("housing", 0.25f, 0.35f, 0.25f, "metal_lamp_ornate", 2.85f);
    auto classic_core    = icosphere("core", 0.12f, "light_amber", 3.0f);
    auto classic = Mc3Object::makeGroup("streetlamp.classic_01",
        {classic_base, classic_pole, classic_housing, classic_core});
    classic->assetMetadata = make_meta(
        "street_furniture", "streetlamp", {"central_europe", "classic"},
        {0.25f, 3.20f, 0.25f}, {-0.125f, 0.f, -0.125f}, {0.125f, 3.20f, 0.125f}, "+X",
        "streetlamp.classic_01.lod_low");

    auto classic_low = cyl("proxy", 0.06f, 3.2f, "metal_lamp_ornate");
    classic_low->id = "streetlamp.classic_01.lod_low";

    // modern_01: slim pole + cantilevered LED arm.
    auto modern_pole    = cyl("pole", 0.04f, 3.20f, "metal_dark");
    auto modern_arm      = box("arm", 0.30f, 0.10f, 0.10f, "metal_dark", 3.15f, 0.15f);
    auto modern_head      = box("head", 0.25f, 0.06f, 0.12f, "light_amber", 3.05f, 0.30f);
    auto modern = Mc3Object::makeGroup("streetlamp.modern_01",
        {modern_pole, modern_arm, modern_head});
    modern->assetMetadata = make_meta(
        "street_furniture", "streetlamp", {"modern"},
        {0.465f, 3.25f, 0.12f}, {-0.04f, 0.f, -0.06f}, {0.425f, 3.25f, 0.06f}, "+X",
        "streetlamp.modern_01.lod_low");

    auto modern_low = cyl("proxy", 0.04f, 3.2f, "metal_dark");
    modern_low->id = "streetlamp.modern_01.lod_low";

    // classic_02: same family as classic_01, a double-headed cross-arm
    // variant (deeper coverage within the "classic" streetlamp family).
    auto c2_base      = cyl("base", 0.12f, 0.15f, "metal_lamp_ornate");
    auto c2_pole       = cyl("pole", 0.05f, 2.85f, "metal_lamp_ornate", 0.15f);
    auto c2_arm        = box("arm", 0.90f, 0.06f, 0.06f, "metal_lamp_ornate", 2.95f);
    auto c2_housing_l  = box("housing_l", 0.22f, 0.30f, 0.22f, "metal_lamp_ornate", 2.95f, -0.42f);
    auto c2_housing_r  = box("housing_r", 0.22f, 0.30f, 0.22f, "metal_lamp_ornate", 2.95f,  0.42f);
    auto c2_core_l     = icosphere("core_l", 0.10f, "light_amber", 3.10f, -0.42f);
    auto c2_core_r     = icosphere("core_r", 0.10f, "light_amber", 3.10f,  0.42f);
    auto classic2 = Mc3Object::makeGroup("streetlamp.classic_02",
        {c2_base, c2_pole, c2_arm, c2_housing_l, c2_housing_r, c2_core_l, c2_core_r});
    classic2->assetMetadata = make_meta(
        "street_furniture", "streetlamp", {"central_europe", "classic"},
        {0.95f, 3.25f, 0.25f}, {-0.53f, 0.f, -0.125f}, {0.53f, 3.25f, 0.125f}, "+X",
        "streetlamp.classic_02.lod_low");

    auto classic2_low = cyl("proxy", 0.06f, 3.2f, "metal_lamp_ornate");
    classic2_low->id = "streetlamp.classic_02.lod_low";

    // bench.classic_01 -- genuinely different subcategory within
    // street_furniture (mesh_world_revival.md's own street-furniture
    // examples include benches, not just lamps).
    auto bench_leg_l = box("leg_l", 0.06f, 0.45f, 0.40f, "metal_dark", 0.f, -0.65f);
    auto bench_leg_r = box("leg_r", 0.06f, 0.45f, 0.40f, "metal_dark", 0.f,  0.65f);
    auto bench_seat  = box("seat", 1.5f, 0.06f, 0.45f, "wood_bench", 0.45f);
    auto bench_back  = box("back", 1.5f, 0.45f, 0.06f, "wood_bench", 0.45f, 0.f, -0.20f);
    auto bench = Mc3Object::makeGroup("bench.classic_01",
        {bench_leg_l, bench_leg_r, bench_seat, bench_back});
    bench->assetMetadata = make_meta(
        "street_furniture", "bench", {"central_europe", "classic"},
        {1.5f, 0.90f, 0.45f}, {-0.75f, 0.f, -0.23f}, {0.75f, 0.90f, 0.225f}, "+Z",
        "bench.classic_01.lod_low");

    auto bench_low = box("proxy", 1.5f, 0.90f, 0.45f, "wood_bench");
    bench_low->id = "bench.classic_01.lod_low";

    write_library("urban-street-furniture", "1.0.0", {
        {"streetlamp.classic_01", classic},
        {"streetlamp.classic_01.lod_low", classic_low},
        {"streetlamp.modern_01", modern},
        {"streetlamp.modern_01.lod_low", modern_low},
        {"streetlamp.classic_02", classic2},
        {"streetlamp.classic_02.lod_low", classic2_low},
        {"bench.classic_01", bench},
        {"bench.classic_01.lod_low", bench_low},
    }, outDir);
}

// --- props -----------------------------------------------------------------

void build_props_library(const std::filesystem::path& outDir) {
    // classic_01: wood post, box body, small flag.
    auto classic_post = cyl("post", 0.04f, 0.90f, "wood_natural");
    auto classic_body  = box("body", 0.30f, 0.25f, 0.35f, "mailbox_body", 0.85f);
    auto classic_flag  = box("flag", 0.03f, 0.15f, 0.03f, "metal_dark", 0.95f, 0.16f);
    auto classic = Mc3Object::makeGroup("prop.mailbox.classic_01",
        {classic_post, classic_body, classic_flag});
    classic->assetMetadata = make_meta(
        "prop", "mailbox", {"central_europe", "classic"},
        {0.325f, 1.10f, 0.35f}, {-0.15f, 0.f, -0.175f}, {0.175f, 1.10f, 0.175f}, "+X",
        "prop.mailbox.classic_01.lod_low");

    auto classic_low = box("proxy", 0.35f, 1.10f, 0.35f, "mailbox_body");
    classic_low->id = "prop.mailbox.classic_01.lod_low";

    // modern_01: metal post, sleeker body, no flag.
    auto modern_post = box("post", 0.06f, 0.90f, 0.06f, "metal_dark");
    auto modern_body  = box("body", 0.28f, 0.22f, 0.30f, "mailbox_body", 0.85f);
    auto modern = Mc3Object::makeGroup("prop.mailbox.modern_01",
        {modern_post, modern_body});
    modern->assetMetadata = make_meta(
        "prop", "mailbox", {"modern"},
        {0.28f, 1.07f, 0.30f}, {-0.14f, 0.f, -0.15f}, {0.14f, 1.07f, 0.15f}, "+X",
        "prop.mailbox.modern_01.lod_low");

    auto modern_low = box("proxy", 0.30f, 1.07f, 0.30f, "mailbox_body");
    modern_low->id = "prop.mailbox.modern_01.lod_low";

    // classic_02: same family, a larger multi-compartment box on a
    // shorter post with a flat lid -- deeper coverage within "classic".
    auto c2_post = cyl("post", 0.05f, 0.70f, "wood_natural");
    auto c2_body = box("body", 0.40f, 0.30f, 0.28f, "mailbox_body", 0.70f);
    auto c2_lid  = box("lid", 0.42f, 0.04f, 0.30f, "metal_dark", 1.00f);
    auto classic2 = Mc3Object::makeGroup("prop.mailbox.classic_02",
        {c2_post, c2_body, c2_lid});
    classic2->assetMetadata = make_meta(
        "prop", "mailbox", {"central_europe", "classic"},
        {0.42f, 1.04f, 0.30f}, {-0.21f, 0.f, -0.15f}, {0.21f, 1.04f, 0.15f}, "+X",
        "prop.mailbox.classic_02.lod_low");

    auto classic2_low = box("proxy", 0.42f, 1.04f, 0.30f, "mailbox_body");
    classic2_low->id = "prop.mailbox.classic_02.lod_low";

    // trash_bin.classic_01 -- genuinely different subcategory within
    // prop (a common street-level fixture).
    auto bin_body = cyl("body", 0.20f, 0.55f, "plastic_black");
    auto bin_lid  = cyl("lid", 0.21f, 0.04f, "metal_dark", 0.55f);
    auto bin = Mc3Object::makeGroup("prop.trash_bin.classic_01",
        {bin_body, bin_lid});
    bin->assetMetadata = make_meta(
        "prop", "trash_bin", {"central_europe", "modern"},
        {0.42f, 0.59f, 0.42f}, {-0.21f, 0.f, -0.21f}, {0.21f, 0.59f, 0.21f}, "+X",
        "prop.trash_bin.classic_01.lod_low");

    auto bin_low = cyl("proxy", 0.20f, 0.59f, "plastic_black");
    bin_low->id = "prop.trash_bin.classic_01.lod_low";

    write_library("urban-props", "1.0.0", {
        {"prop.mailbox.classic_01", classic},
        {"prop.mailbox.classic_01.lod_low", classic_low},
        {"prop.mailbox.modern_01", modern},
        {"prop.mailbox.modern_01.lod_low", modern_low},
        {"prop.mailbox.classic_02", classic2},
        {"prop.mailbox.classic_02.lod_low", classic2_low},
        {"prop.trash_bin.classic_01", bin},
        {"prop.trash_bin.classic_01.lod_low", bin_low},
    }, outDir);
}

// --- roofs -------------------------------------------------------------
// Standalone, reusable roof modules -- geometry sits with its own eave at
// y=0 (a future socket-aware placement would instance it at
// y=wall_height, same "base_y is a base elevation" convention as
// everything else). NOT wired into house_gable_default itself: that
// asset still bakes its own roof inline (R102's "split composite objects"
// direction applies to NEW assets composed from these parts, not a
// retrofit of the existing v1 house -- explicitly deferred, see plan.md).

void build_roofs_library(const std::filesystem::path& outDir) {
    // gable_clay_04: matches house_gable_default's own proportions
    // (w=10.6, d=8.6) so it's a plausible drop-in replacement once
    // something actually composes buildings from separate roof parts.
    const float w = 10.6f, d = 8.6f, rh = 2.5f;
    const float slope_w = std::sqrt((w / 2.f) * (w / 2.f) + rh * rh) + 0.4f;
    const float angle_deg = std::atan(rh / (w / 2.f)) * (180.f / 3.14159265358979323846f);

    auto gable_l = box("panel_l", slope_w, 0.25f, d, "roof_tile_red", rh / 2.f - 0.125f, -w / 4.f);
    gable_l->transform.rotation = {0.f, 0.f, -angle_deg};
    auto gable_r = box("panel_r", slope_w, 0.25f, d, "roof_tile_red", rh / 2.f - 0.125f, w / 4.f);
    gable_r->transform.rotation = {0.f, 0.f, angle_deg};
    auto gable_ridge = box("ridge", 0.30f, 0.20f, d + 0.4f, "roof_tile_red", rh - 0.10f);

    auto gable = Mc3Object::makeGroup("roof.gable_clay_04", {gable_l, gable_r, gable_ridge});
    gable->assetMetadata = make_meta(
        "roof", "gable", {"central_europe", "gable_roof"},
        {w + 0.6f, rh + 0.3f, d + 0.6f}, {-w / 2.f - 0.3f, 0.f, -d / 2.f - 0.3f},
        {w / 2.f + 0.3f, rh + 0.3f, d / 2.f + 0.3f}, "+Z",
        "roof.gable_clay_04.lod_low");

    auto gable_low = box("proxy", w, rh + 0.2f, d, "roof_tile_red");
    gable_low->id = "roof.gable_clay_04.lod_low";

    // flat_modern_01: slab + 4-sided parapet (same "picture frame" of
    // thin boxes CaveGenerator's own ceiling_n/s/w/e entrance breach
    // already established as this codebase's go-to pattern for a
    // perimeter-hugging ring of geometry).
    const float slab_h = 0.2f, parapet_h = 0.3f, parapet_t = 0.15f;
    auto slab = box("slab", w, slab_h, d, "concrete_slab");
    auto parapet_n = box("parapet_n", w, parapet_h, parapet_t, "concrete_panel", slab_h, 0.f,  d / 2.f - parapet_t / 2.f);
    auto parapet_s = box("parapet_s", w, parapet_h, parapet_t, "concrete_panel", slab_h, 0.f, -(d / 2.f - parapet_t / 2.f));
    auto parapet_e = box("parapet_e", parapet_t, parapet_h, d, "concrete_panel", slab_h,  w / 2.f - parapet_t / 2.f);
    auto parapet_w = box("parapet_w", parapet_t, parapet_h, d, "concrete_panel", slab_h, -(w / 2.f - parapet_t / 2.f));

    auto flat = Mc3Object::makeGroup("roof.flat_modern_01",
        {slab, parapet_n, parapet_s, parapet_e, parapet_w});
    flat->assetMetadata = make_meta(
        "roof", "flat", {"modern", "flat_roof"},
        {w, slab_h + parapet_h, d}, {-w / 2.f, 0.f, -d / 2.f}, {w / 2.f, slab_h + parapet_h, d / 2.f},
        "+Z", "roof.flat_modern_01.lod_low");

    auto flat_low = box("proxy", w, slab_h + parapet_h, d, "concrete_slab");
    flat_low->id = "roof.flat_modern_01.lod_low";

    // gable_clay_05: same family/geometry as gable_clay_04 (reuses this
    // function's own w/d/rh/slope_w/angle_deg), grey tile instead of red.
    auto gable5_l = box("panel_l", slope_w, 0.25f, d, "roof_tile_grey", rh / 2.f - 0.125f, -w / 4.f);
    gable5_l->transform.rotation = {0.f, 0.f, -angle_deg};
    auto gable5_r = box("panel_r", slope_w, 0.25f, d, "roof_tile_grey", rh / 2.f - 0.125f, w / 4.f);
    gable5_r->transform.rotation = {0.f, 0.f, angle_deg};
    auto gable5_ridge = box("ridge", 0.30f, 0.20f, d + 0.4f, "roof_tile_grey", rh - 0.10f);

    auto gable5 = Mc3Object::makeGroup("roof.gable_clay_05", {gable5_l, gable5_r, gable5_ridge});
    gable5->assetMetadata = make_meta(
        "roof", "gable", {"central_europe", "gable_roof"},
        {w + 0.6f, rh + 0.3f, d + 0.6f}, {-w / 2.f - 0.3f, 0.f, -d / 2.f - 0.3f},
        {w / 2.f + 0.3f, rh + 0.3f, d / 2.f + 0.3f}, "+Z",
        "roof.gable_clay_05.lod_low");

    auto gable5_low = box("proxy", w, rh + 0.2f, d, "roof_tile_grey");
    gable5_low->id = "roof.gable_clay_05.lod_low";

    // flat_modern_02: same slab+parapet as flat_modern_01, plus a rooftop
    // HVAC unit for visual variety within the "flat" family.
    auto slab2 = box("slab", w, slab_h, d, "concrete_slab");
    auto parapet2_n = box("parapet_n", w, parapet_h, parapet_t, "concrete_panel", slab_h, 0.f,  d / 2.f - parapet_t / 2.f);
    auto parapet2_s = box("parapet_s", w, parapet_h, parapet_t, "concrete_panel", slab_h, 0.f, -(d / 2.f - parapet_t / 2.f));
    auto parapet2_e = box("parapet_e", parapet_t, parapet_h, d, "concrete_panel", slab_h,  w / 2.f - parapet_t / 2.f);
    auto parapet2_w = box("parapet_w", parapet_t, parapet_h, d, "concrete_panel", slab_h, -(w / 2.f - parapet_t / 2.f));
    auto hvac = box("hvac_unit", 1.2f, 0.6f, 0.9f, "metal_grate", slab_h, -w / 4.f, -d / 4.f);

    auto flat2 = Mc3Object::makeGroup("roof.flat_modern_02",
        {slab2, parapet2_n, parapet2_s, parapet2_e, parapet2_w, hvac});
    flat2->assetMetadata = make_meta(
        "roof", "flat", {"modern", "flat_roof"},
        {w, slab_h + 0.6f, d}, {-w / 2.f, 0.f, -d / 2.f}, {w / 2.f, slab_h + 0.6f, d / 2.f},
        "+Z", "roof.flat_modern_02.lod_low");

    auto flat2_low = box("proxy", w, slab_h + parapet_h, d, "concrete_slab");
    flat2_low->id = "roof.flat_modern_02.lod_low";

    // gable_clay_wide_01 -- same construction as gable_clay_04, scaled to
    // house.gable.wide_01's own outer footprint (15.6m x 8.6m instead of
    // 10.6m x 8.6m) so a real gable-roofed WIDE house exists (R113
    // size-aware matching's own follow-up: the only other wide house,
    // house.rowhouse.modular_01, has a genuine flat roof and so never
    // carries the "gable_roof" style tag the shipped style profile
    // requires -- this gives wide-class parcels a real gable-compatible
    // candidate too). Same rh (2.5m) as the standard-width gable roofs,
    // not scaled up with the span -- a real gable roof this much wider
    // at the same ridge height is simply a shallower pitch, which is
    // architecturally plausible, not a modeling error.
    const float ww = 15.6f, wd = 8.6f;
    const float wslope_w = std::sqrt((ww / 2.f) * (ww / 2.f) + rh * rh) + 0.4f;
    const float wangle_deg = std::atan(rh / (ww / 2.f)) * (180.f / 3.14159265358979323846f);

    auto wgable_l = box("panel_l", wslope_w, 0.25f, wd, "roof_tile_red", rh / 2.f - 0.125f, -ww / 4.f);
    wgable_l->transform.rotation = {0.f, 0.f, -wangle_deg};
    auto wgable_r = box("panel_r", wslope_w, 0.25f, wd, "roof_tile_red", rh / 2.f - 0.125f, ww / 4.f);
    wgable_r->transform.rotation = {0.f, 0.f, wangle_deg};
    auto wgable_ridge = box("ridge", 0.30f, 0.20f, wd + 0.4f, "roof_tile_red", rh - 0.10f);

    auto wgable = Mc3Object::makeGroup("roof.gable_clay_wide_01", {wgable_l, wgable_r, wgable_ridge});
    wgable->assetMetadata = make_meta(
        "roof", "gable", {"central_europe", "gable_roof"},
        {ww + 0.6f, rh + 0.3f, wd + 0.6f}, {-ww / 2.f - 0.3f, 0.f, -wd / 2.f - 0.3f},
        {ww / 2.f + 0.3f, rh + 0.3f, wd / 2.f + 0.3f}, "+Z",
        "roof.gable_clay_wide_01.lod_low");

    auto wgable_low = box("proxy", ww, rh + 0.2f, wd, "roof_tile_red");
    wgable_low->id = "roof.gable_clay_wide_01.lod_low";

    write_library("urban-roofs", "1.0.0", {
        {"roof.gable_clay_04", gable},
        {"roof.gable_clay_04.lod_low", gable_low},
        {"roof.flat_modern_01", flat},
        {"roof.flat_modern_01.lod_low", flat_low},
        {"roof.gable_clay_05", gable5},
        {"roof.gable_clay_05.lod_low", gable5_low},
        {"roof.flat_modern_02", flat2},
        {"roof.flat_modern_02.lod_low", flat2_low},
        {"roof.gable_clay_wide_01", wgable},
        {"roof.gable_clay_wide_01.lod_low", wgable_low},
    }, outDir);
}

// --- facade modules --------------------------------------------------------
// "Bay" segments meant to tile along a wall (mesh_world_revival.md
// §5.2's "facade bays"). NOT placed anywhere yet -- real tiling along a
// building face needs R103's socket-aware placement (deciding how many
// bays fit a given wall span and which face they belong on), same
// registered-but-unconsumed status as the window/door content batch.

void build_facades_library(const std::filesystem::path& outDir) {
    const float bay_w = 2.5f, bay_h = 3.2f, wt = 0.3f;

    auto win_wall  = box("wall", bay_w, bay_h, wt, "plaster_white");
    auto win_frame = box("frame", 1.2f, 1.0f, 0.06f, "wood_window_frame", 1.10f, 0.f, wt / 2.f);
    auto win_glass = box("glass", 1.04f, 0.84f, 0.03f, "glass_clear", 1.18f, 0.f, wt / 2.f + 0.02f);
    auto win_bay = Mc3Object::makeGroup("facade.residential_bay_window_01",
        {win_wall, win_frame, win_glass});
    win_bay->assetMetadata = make_meta(
        "facade_module", "residential_bay_window", {"central_europe"},
        {bay_w, bay_h, wt + 0.1f}, {-bay_w / 2.f, 0.f, -wt / 2.f}, {bay_w / 2.f, bay_h, wt / 2.f + 0.05f},
        "+Z", "facade.residential_bay_window_01.lod_low");

    auto win_bay_low = box("proxy", bay_w, bay_h, wt, "plaster_white");
    win_bay_low->id = "facade.residential_bay_window_01.lod_low";

    auto door_wall = box("wall", bay_w, bay_h, wt, "plaster_white");
    auto door_slab = box("slab", 1.0f, 2.1f, 0.06f, "wood_door_panel", 0.f, 0.f, wt / 2.f);
    auto door_bay = Mc3Object::makeGroup("facade.residential_bay_door_01",
        {door_wall, door_slab});
    door_bay->assetMetadata = make_meta(
        "facade_module", "residential_bay_door", {"central_europe"},
        {bay_w, bay_h, wt + 0.1f}, {-bay_w / 2.f, 0.f, -wt / 2.f}, {bay_w / 2.f, bay_h, wt / 2.f + 0.05f},
        "+Z", "facade.residential_bay_door_01.lod_low");

    auto door_bay_low = box("proxy", bay_w, bay_h, wt, "plaster_white");
    door_bay_low->id = "facade.residential_bay_door_01.lod_low";

    // bay_window_02: same family as bay_window_01, cream wall + shutters.
    auto win2_wall  = box("wall", bay_w, bay_h, wt, "plaster_cream");
    auto win2_frame = box("frame", 1.2f, 1.0f, 0.06f, "wood_window_frame", 1.10f, 0.f, wt / 2.f);
    auto win2_glass = box("glass", 1.04f, 0.84f, 0.03f, "glass_clear", 1.18f, 0.f, wt / 2.f + 0.02f);
    auto win2_shut_l = box("shutter_l", 0.30f, 1.0f, 0.03f, "wood_bark_dark", 1.10f, -0.75f, wt / 2.f);
    auto win2_shut_r = box("shutter_r", 0.30f, 1.0f, 0.03f, "wood_bark_dark", 1.10f,  0.75f, wt / 2.f);
    auto win2_bay = Mc3Object::makeGroup("facade.residential_bay_window_02",
        {win2_wall, win2_frame, win2_glass, win2_shut_l, win2_shut_r});
    win2_bay->assetMetadata = make_meta(
        "facade_module", "residential_bay_window", {"central_europe"},
        {bay_w, bay_h, wt + 0.1f}, {-bay_w / 2.f, 0.f, -wt / 2.f}, {bay_w / 2.f, bay_h, wt / 2.f + 0.05f},
        "+Z", "facade.residential_bay_window_02.lod_low");

    auto win2_bay_low = box("proxy", bay_w, bay_h, wt, "plaster_cream");
    win2_bay_low->id = "facade.residential_bay_window_02.lod_low";

    // bay_plain_01 -- a plain wall segment, no opening: genuinely useful
    // "filler" module for a future tiling algorithm where not every bay
    // needs a window or door.
    auto plain_wall = box("wall", bay_w, bay_h, wt, "plaster_white");
    auto plain_bay = Mc3Object::makeGroup("facade.residential_bay_plain_01", {plain_wall});
    plain_bay->assetMetadata = make_meta(
        "facade_module", "residential_bay_plain", {"central_europe"},
        {bay_w, bay_h, wt}, {-bay_w / 2.f, 0.f, -wt / 2.f}, {bay_w / 2.f, bay_h, wt / 2.f},
        "+Z", "facade.residential_bay_plain_01.lod_low");

    auto plain_bay_low = box("proxy", bay_w, bay_h, wt, "plaster_white");
    plain_bay_low->id = "facade.residential_bay_plain_01.lod_low";

    write_library("urban-facades", "1.0.0", {
        {"facade.residential_bay_window_01", win_bay},
        {"facade.residential_bay_window_01.lod_low", win_bay_low},
        {"facade.residential_bay_door_01", door_bay},
        {"facade.residential_bay_door_01.lod_low", door_bay_low},
        {"facade.residential_bay_window_02", win2_bay},
        {"facade.residential_bay_window_02.lod_low", win2_bay_low},
        {"facade.residential_bay_plain_01", plain_bay},
        {"facade.residential_bay_plain_01.lod_low", plain_bay_low},
    }, outDir);
}

// --- vehicles ------------------------------------------------------------
// Parked cars -- the one new category BuildingComposer actually PLACES
// (see BuildingComposer.cpp): parcel-level curb placement needs no
// socket, same reasoning street_furniture/props already established.

void build_vehicles_library(const std::filesystem::path& outDir) {
    // hatchback_compact_01: shorter, cabin biased toward the rear.
    auto hb_body  = box("body", 1.7f, 0.55f, 3.8f, "car_paint_blue", 0.30f);
    auto hb_cabin = box("cabin", 1.5f, 0.45f, 1.8f, "car_paint_blue", 0.85f, 0.f, -0.4f);
    auto hb_glass = box("windshield", 1.35f, 0.35f, 0.05f, "glass_clear", 0.90f, 0.f, 0.45f);
    auto hatchback = Mc3Object::makeGroup("car.hatchback.compact_01", {
        hb_body, hb_cabin, hb_glass,
        wheel("wheel_fl", 0.28f, 0.22f, "tire_rubber",  0.75f,  1.35f),
        wheel("wheel_fr", 0.28f, 0.22f, "tire_rubber", -0.75f,  1.35f),
        wheel("wheel_rl", 0.28f, 0.22f, "tire_rubber",  0.75f, -1.35f),
        wheel("wheel_rr", 0.28f, 0.22f, "tire_rubber", -0.75f, -1.35f),
    });
    hatchback->assetMetadata = make_meta(
        "vehicle", "car", {"compact"},
        {1.70f, 1.30f, 3.8f}, {-0.85f, 0.f, -1.9f}, {0.85f, 1.30f, 1.9f}, "+Z",
        "car.hatchback.compact_01.lod_low", "box");

    auto hatchback_low = box("proxy", 1.7f, 1.0f, 3.8f, "car_paint_blue");
    hatchback_low->id = "car.hatchback.compact_01.lod_low";

    // sedan_family_01: longer, cabin more centered.
    auto sd_body  = box("body", 1.75f, 0.55f, 4.4f, "car_paint_white", 0.30f);
    auto sd_cabin = box("cabin", 1.55f, 0.45f, 2.2f, "car_paint_white", 0.85f, 0.f, -0.1f);
    auto sd_glass = box("windshield", 1.40f, 0.35f, 0.05f, "glass_clear", 0.90f, 0.f, 0.95f);
    auto sedan = Mc3Object::makeGroup("car.sedan.family_01", {
        sd_body, sd_cabin, sd_glass,
        wheel("wheel_fl", 0.30f, 0.22f, "tire_rubber",  0.78f,  1.55f),
        wheel("wheel_fr", 0.30f, 0.22f, "tire_rubber", -0.78f,  1.55f),
        wheel("wheel_rl", 0.30f, 0.22f, "tire_rubber",  0.78f, -1.55f),
        wheel("wheel_rr", 0.30f, 0.22f, "tire_rubber", -0.78f, -1.55f),
    });
    sedan->assetMetadata = make_meta(
        "vehicle", "car", {"family"},
        {1.75f, 1.30f, 4.4f}, {-0.875f, 0.f, -2.2f}, {0.875f, 1.30f, 2.2f}, "+Z",
        "car.sedan.family_01.lod_low", "box");

    auto sedan_low = box("proxy", 1.75f, 1.0f, 4.4f, "car_paint_white");
    sedan_low->id = "car.sedan.family_01.lod_low";

    // hatchback.compact_02: same family/geometry as compact_01, red.
    auto hb2_body  = box("body", 1.7f, 0.55f, 3.8f, "car_paint_red", 0.30f);
    auto hb2_cabin = box("cabin", 1.5f, 0.45f, 1.8f, "car_paint_red", 0.85f, 0.f, -0.4f);
    auto hb2_glass = box("windshield", 1.35f, 0.35f, 0.05f, "glass_clear", 0.90f, 0.f, 0.45f);
    auto hatchback2 = Mc3Object::makeGroup("car.hatchback.compact_02", {
        hb2_body, hb2_cabin, hb2_glass,
        wheel("wheel_fl", 0.28f, 0.22f, "tire_rubber",  0.75f,  1.35f),
        wheel("wheel_fr", 0.28f, 0.22f, "tire_rubber", -0.75f,  1.35f),
        wheel("wheel_rl", 0.28f, 0.22f, "tire_rubber",  0.75f, -1.35f),
        wheel("wheel_rr", 0.28f, 0.22f, "tire_rubber", -0.75f, -1.35f),
    });
    hatchback2->assetMetadata = make_meta(
        "vehicle", "car", {"compact"},
        {1.70f, 1.30f, 3.8f}, {-0.85f, 0.f, -1.9f}, {0.85f, 1.30f, 1.9f}, "+Z",
        "car.hatchback.compact_02.lod_low", "box");

    auto hatchback2_low = box("proxy", 1.7f, 1.0f, 3.8f, "car_paint_red");
    hatchback2_low->id = "car.hatchback.compact_02.lod_low";

    // van.delivery_01 -- genuinely different vehicle type (one tall boxy
    // volume, no separate cabin), matching mesh_world_revival.md §4.5's
    // own explicit "car.van.delivery_02" example.
    auto van_body       = box("body", 1.9f, 1.9f, 5.0f, "car_paint_white", 0.30f);
    auto van_windshield = box("windshield", 1.7f, 0.6f, 0.05f, "glass_clear", 1.5f, 0.f, 2.30f);
    auto van = Mc3Object::makeGroup("car.van.delivery_01", {
        van_body, van_windshield,
        wheel("wheel_fl", 0.32f, 0.24f, "tire_rubber",  0.85f,  1.70f),
        wheel("wheel_fr", 0.32f, 0.24f, "tire_rubber", -0.85f,  1.70f),
        wheel("wheel_rl", 0.32f, 0.24f, "tire_rubber",  0.85f, -1.70f),
        wheel("wheel_rr", 0.32f, 0.24f, "tire_rubber", -0.85f, -1.70f),
    });
    van->assetMetadata = make_meta(
        "vehicle", "van", {"delivery"},
        {1.9f, 2.20f, 5.0f}, {-0.95f, 0.f, -2.5f}, {0.95f, 2.20f, 2.5f}, "+Z",
        "car.van.delivery_01.lod_low", "box");

    auto van_low = box("proxy", 1.9f, 2.20f, 5.0f, "car_paint_white");
    van_low->id = "car.van.delivery_01.lod_low";

    write_library("urban-vehicles", "1.0.0", {
        {"car.hatchback.compact_01", hatchback},
        {"car.hatchback.compact_01.lod_low", hatchback_low},
        {"car.sedan.family_01", sedan},
        {"car.sedan.family_01.lod_low", sedan_low},
        {"car.hatchback.compact_02", hatchback2},
        {"car.hatchback.compact_02.lod_low", hatchback2_low},
        {"car.van.delivery_01", van},
        {"car.van.delivery_01.lod_low", van_low},
    }, outDir);
}

// --- buildings (R103/R104 v1) -----------------------------------------
// The real worked example: a MODULAR house that imports its windows/
// door/roof (R101/R102) instead of baking them in, with an attached Lua
// script (Mc3ScriptRunner, MeshWorld-side -- see include/Mc3ScriptRunner
// .hpp for why the engine lives in MeshWorld, not mesh-craft, for v1)
// that places them at this definition's own assetMetadata.sockets at
// compose time. house_gable_default (ObjectDefinitionLibrary.cpp)
// remains a separate, unmodified asset -- this is a second, additive
// house candidate, not a retrofit.
//
// Unlike every other build_*_library() function above, this library also
// needs its OWN doc.imports (resolved recursively when THIS library is
// loaded and merged, so the script's def:place() calls can find
// "windows:...", "doors:...", "roofs:..." in doc.definitions) and a
// doc.scripts entry -- write_library() only handles definitions, so this
// function builds its Mc3Document directly instead of using it.
void build_buildings_library(const std::filesystem::path& outDir) {
    // Same dimensions as ObjectDefinitionLibrary.cpp's own
    // make_house_gable(), minus the baked-in windows/door/roof --
    // reproduces that asset's exact visual layout via imported
    // references instead of inline geometry.
    const float w = 10.0f, d = 8.0f, wh = 3.2f, wt = 0.30f, rh = 2.5f;
    const float half_w = w / 2.0f, half_d = d / 2.0f;

    auto floor       = box("floor", w + wt * 2.0f, 0.20f, d + wt * 2.0f, "concrete", -0.10f);
    auto wall_front  = box("wall_front", w + wt * 2.0f, wh, wt, "plaster_white", 0.0f, 0.0f,  half_d + wt / 2.0f);
    auto wall_back   = box("wall_back",  w + wt * 2.0f, wh, wt, "plaster_white", 0.0f, 0.0f, -(half_d + wt / 2.0f));
    auto wall_left   = box("wall_left",  wt, wh, d, "plaster_white", 0.0f, -(half_w + wt / 2.0f));
    auto wall_right  = box("wall_right", wt, wh, d, "plaster_white", 0.0f,  (half_w + wt / 2.0f));
    auto gable_front = box("gable_front", w + wt * 2.0f, rh, wt, "plaster_white", wh, 0.0f,  half_d + wt / 2.0f);
    auto gable_back  = box("gable_back",  w + wt * 2.0f, rh, wt, "plaster_white", wh, 0.0f, -(half_d + wt / 2.0f));

    auto house = Mc3Object::makeGroup("house.gable.modular_01",
        {floor, wall_front, wall_back, wall_left, wall_right, gable_front, gable_back});

    Mc3AssetMetadata meta;
    meta.category    = "house";
    meta.subcategory = "detached";
    meta.semanticTags = {"residential", "detached", "gable_roof", "modular"};
    meta.styleTags    = {"central_europe", "gable_roof"};
    meta.regionTags   = {"central_europe"};
    meta.nominalSize  = {w + wt * 2.0f, wh + rh, d + wt * 2.0f};
    meta.boundsMin    = {-(half_w + wt), 0.0f, -(half_d + wt)};
    meta.boundsMax    = { (half_w + wt), wh + rh, (half_d + wt)};
    meta.facing       = "+Z";
    // Base positions (not centers -- matches every socket-filled
    // definition's own "base_y" convention) for the imported window/
    // door/roof to land on, mirroring exactly where make_house_gable()'s
    // own inline win_front_l/r, door, and roof panels sat.
    meta.sockets = {
        {"window_front_l", {-w / 4.0f, 1.10f, half_d + wt / 2.0f + 0.001f}},
        {"window_front_r", { w / 4.0f, 1.10f, half_d + wt / 2.0f + 0.001f}},
        {"door_front",     {0.0f,      0.0f,  half_d + wt / 2.0f + 0.001f}},
        {"roof_mount",     {0.0f,      wh,    0.0f}},
    };
    meta.materialSlots      = {"wall"};
    meta.collisionProxy     = "box";
    meta.instancingEligible = true;
    meta.shadowPolicy       = "cast_receive";
    meta.selectionWeight    = 1.0f;
    meta.license            = "MIT";
    meta.provenance         = "MeshWorld procedural (R103/R104, build_mc3lib_content.cpp) -- "
                              "modular variant of ObjectDefinitionLibrary's own "
                              "house_gable_default, importing windows/door/roof instead of "
                              "baking them in";
    meta.sourceGeneratorOrHash = "cpp.mc3lib.urban_buildings.v1";
    meta.semanticVersion       = "1.0.0";
    house->assetMetadata = std::move(meta);
    house->scriptId = "house.gable.modular_01.facade";

    // house.rowhouse.modular_01 -- R112 facade_module consumption: a
    // wider (15m) terraced house whose ENTIRE front wall is tiled from
    // urban-facades' own bay modules (2.5m each, 6 bays = 15m exactly,
    // no remainder) via Mc3ScriptRunner's place_at() (raw coordinates,
    // computed in Lua -- not a fixed, hand-authored socket list, since
    // the number of bays is a function of the wall's own length). Side/
    // back walls and a flat roof cap are still built directly (no
    // matching-footprint roof module exists at this width). Deliberately
    // registered under category "house" like every other real house
    // candidate -- BuildingComposer now does size-aware matching
    // (filters by each parcel's own frontage_extent against a
    // candidate's nominalSize[0]), so this wider (15.6m) house is only
    // ever picked for a wide-class parcel (derive_parcels()'s own
    // per-row width-class roll), never one that would make it overlap a
    // standard-width neighbor.
    const float rw = 15.0f, rd = 8.0f, rwh = 3.2f, rwt = 0.30f;
    const float rhalf_w = rw / 2.0f, rhalf_d = rd / 2.0f;

    auto row_floor      = box("floor", rw + rwt * 2.0f, 0.20f, rd + rwt * 2.0f, "concrete", -0.10f);
    auto row_wall_back  = box("wall_back",  rw + rwt * 2.0f, rwh, rwt, "plaster_cream", 0.0f, 0.0f, -(rhalf_d + rwt / 2.0f));
    auto row_wall_left  = box("wall_left",  rwt, rwh, rd, "plaster_cream", 0.0f, -(rhalf_w + rwt / 2.0f));
    auto row_wall_right = box("wall_right", rwt, rwh, rd, "plaster_cream", 0.0f,  (rhalf_w + rwt / 2.0f));
    auto row_roof_slab  = box("roof_slab", rw + rwt * 2.0f, 0.20f, rd + rwt * 2.0f, "concrete_slab", rwh);
    // No wall_front -- fully tiled by the attached script.

    auto rowhouse = Mc3Object::makeGroup("house.rowhouse.modular_01",
        {row_floor, row_wall_back, row_wall_left, row_wall_right, row_roof_slab});

    Mc3AssetMetadata rowMeta;
    // R113 (size-aware matching) -- "house" like the standard-width
    // candidates, not a separate category: BuildingComposer now filters
    // by frontage_extent match per parcel, so a wide (15.6m) house
    // safely coexists in the same "house" pool without ever being picked
    // for a standard-width parcel.
    rowMeta.category    = "house";
    rowMeta.subcategory = "terraced";
    rowMeta.semanticTags = {"residential", "row_house", "modular", "facade_tiled"};
    rowMeta.styleTags    = {"central_europe"};
    rowMeta.regionTags   = {"central_europe"};
    rowMeta.nominalSize  = {rw + rwt * 2.0f, rwh + 0.2f, rd + rwt * 2.0f};
    rowMeta.boundsMin    = {-(rhalf_w + rwt), 0.0f, -(rhalf_d + rwt)};
    rowMeta.boundsMax    = { (rhalf_w + rwt), rwh + 0.2f, (rhalf_d + rwt)};
    rowMeta.facing       = "+Z";
    rowMeta.materialSlots      = {"wall"};
    rowMeta.collisionProxy     = "box";
    rowMeta.instancingEligible = true;
    rowMeta.shadowPolicy       = "cast_receive";
    rowMeta.selectionWeight    = 1.0f;
    rowMeta.license            = "MIT";
    rowMeta.provenance         = "MeshWorld procedural (R103/R112 facade-tiling demo, "
                                 "build_mc3lib_content.cpp) -- demonstrates real "
                                 "facade_module consumption via Mc3ScriptRunner::place_at(), "
                                 "tiling 6 facade bay modules (2.5m each) along its own 15m "
                                 "front wall instead of a fixed baked-in facade";
    rowMeta.sourceGeneratorOrHash = "cpp.mc3lib.urban_buildings.rowhouse.v1";
    rowMeta.semanticVersion       = "1.0.0";
    rowhouse->assetMetadata = std::move(rowMeta);
    rowhouse->scriptId = "house.rowhouse.modular_01.facade";

    // house.gable.wide_01 -- R113 size-aware matching's own follow-up: a
    // SECOND wide (15.6m) house candidate, this one with a real gable
    // roof (imports the new roof.gable_clay_wide_01 module below), so
    // wide-class parcels have a style-profile-compatible ("gable_roof"
    // tagged) house too -- house.rowhouse.modular_01's genuinely flat
    // roof means it never carries that tag, so it alone left wide
    // parcels with no match under the only shipped style profile
    // (central_europe_default, which requires roofFamily "gable_roof").
    // Same wall/floor construction style as house.gable.modular_01, just
    // wider, with 2 windows on each side of a central door (rather than
    // 1 each) since the front wall is 1.5x as wide.
    const float w2 = 15.0f, d2 = 8.0f, wh2 = 3.2f, wt2 = 0.30f, rh2 = 2.5f;
    const float half_w2 = w2 / 2.0f, half_d2 = d2 / 2.0f;

    auto floor2       = box("floor", w2 + wt2 * 2.0f, 0.20f, d2 + wt2 * 2.0f, "concrete", -0.10f);
    auto wall_front2  = box("wall_front", w2 + wt2 * 2.0f, wh2, wt2, "plaster_yellow", 0.0f, 0.0f,  half_d2 + wt2 / 2.0f);
    auto wall_back2   = box("wall_back",  w2 + wt2 * 2.0f, wh2, wt2, "plaster_yellow", 0.0f, 0.0f, -(half_d2 + wt2 / 2.0f));
    auto wall_left2   = box("wall_left",  wt2, wh2, d2, "plaster_yellow", 0.0f, -(half_w2 + wt2 / 2.0f));
    auto wall_right2  = box("wall_right", wt2, wh2, d2, "plaster_yellow", 0.0f,  (half_w2 + wt2 / 2.0f));
    auto gable_front2 = box("gable_front", w2 + wt2 * 2.0f, rh2, wt2, "plaster_yellow", wh2, 0.0f,  half_d2 + wt2 / 2.0f);
    auto gable_back2  = box("gable_back",  w2 + wt2 * 2.0f, rh2, wt2, "plaster_yellow", wh2, 0.0f, -(half_d2 + wt2 / 2.0f));

    auto wideHouse = Mc3Object::makeGroup("house.gable.wide_01",
        {floor2, wall_front2, wall_back2, wall_left2, wall_right2, gable_front2, gable_back2});

    Mc3AssetMetadata wideMeta;
    wideMeta.category    = "house";
    wideMeta.subcategory = "wide_detached";
    wideMeta.semanticTags = {"residential", "detached", "gable_roof", "modular", "wide"};
    wideMeta.styleTags    = {"central_europe", "gable_roof"};
    wideMeta.regionTags   = {"central_europe"};
    wideMeta.nominalSize  = {w2 + wt2 * 2.0f, wh2 + rh2, d2 + wt2 * 2.0f};
    wideMeta.boundsMin    = {-(half_w2 + wt2), 0.0f, -(half_d2 + wt2)};
    wideMeta.boundsMax    = { (half_w2 + wt2), wh2 + rh2, (half_d2 + wt2)};
    wideMeta.facing       = "+Z";
    wideMeta.sockets = {
        {"window_front_l1", {-half_w2 * 0.4f, 1.10f, half_d2 + wt2 / 2.0f + 0.001f}},
        {"window_front_l2", {-half_w2 * 0.8f, 1.10f, half_d2 + wt2 / 2.0f + 0.001f}},
        {"window_front_r1", { half_w2 * 0.4f, 1.10f, half_d2 + wt2 / 2.0f + 0.001f}},
        {"window_front_r2", { half_w2 * 0.8f, 1.10f, half_d2 + wt2 / 2.0f + 0.001f}},
        {"door_front",      {0.0f,             0.0f,  half_d2 + wt2 / 2.0f + 0.001f}},
        {"roof_mount",      {0.0f,             wh2,   0.0f}},
    };
    wideMeta.materialSlots      = {"wall"};
    wideMeta.collisionProxy     = "box";
    wideMeta.instancingEligible = true;
    wideMeta.shadowPolicy       = "cast_receive";
    wideMeta.selectionWeight    = 1.0f;
    wideMeta.license            = "MIT";
    wideMeta.provenance         = "MeshWorld procedural (R113 size-aware matching follow-up, "
                                  "build_mc3lib_content.cpp) -- a wide (15.6m) house with a "
                                  "real gable roof, closing the wide-class style-coverage gap "
                                  "house.rowhouse.modular_01's genuine flat roof left open";
    wideMeta.sourceGeneratorOrHash = "cpp.mc3lib.urban_buildings.wide_gable.v1";
    wideMeta.semanticVersion       = "1.0.0";
    wideHouse->assetMetadata = std::move(wideMeta);
    wideHouse->scriptId = "house.gable.wide_01.facade";

    Mc3Document doc;
    doc.model    = "urban-buildings";
    doc.library  = Mc3LibraryInfo{"urban-buildings", "1.0.0", ""};
    doc.imports  = {
        Mc3Import{"windows", "mc3lib://urban-windows@1.0.0", ""},
        Mc3Import{"doors",   "mc3lib://urban-doors@1.0.0", ""},
        Mc3Import{"roofs",   "mc3lib://urban-roofs@1.0.0", ""},
        Mc3Import{"facades", "mc3lib://urban-facades@1.0.0", ""},
    };

    Mc3Script script;
    script.id     = "house.gable.modular_01.facade";
    script.type   = "lua";
    script.source =
        "def:place(\"window_l\", \"windows:window.residential.double.classic_01\", \"window_front_l\")\n"
        "def:place(\"window_r\", \"windows:window.residential.double.classic_01\", \"window_front_r\")\n"
        "def:place(\"door\", \"doors:door.residential.wood_panel_01\", \"door_front\")\n"
        "def:place(\"roof\", \"roofs:roof.gable_clay_04\", \"roof_mount\")\n";
    doc.addScript(script);

    Mc3Script rowScript;
    rowScript.id   = "house.rowhouse.modular_01.facade";
    rowScript.type = "lua";
    rowScript.source =
        "local bay_w = 2.5\n"
        "local count = 6\n"
        "local half_total = (bay_w * count) / 2.0\n"
        "local front_z = " + std::to_string(rhalf_d + rwt / 2.0f + 0.001f) + "\n"
        "for i = 0, count - 1 do\n"
        "  local x = -half_total + bay_w * (i + 0.5)\n"
        "  local id = \"bay_\" .. i\n"
        "  if i == 0 then\n"
        "    def:place_at(id, \"facades:facade.residential_bay_door_01\", x, 0.0, front_z)\n"
        "  elseif i % 2 == 0 then\n"
        "    def:place_at(id, \"facades:facade.residential_bay_window_01\", x, 0.0, front_z)\n"
        "  else\n"
        "    def:place_at(id, \"facades:facade.residential_bay_window_02\", x, 0.0, front_z)\n"
        "  end\n"
        "end\n";
    doc.addScript(rowScript);

    Mc3Script wideScript;
    wideScript.id     = "house.gable.wide_01.facade";
    wideScript.type   = "lua";
    wideScript.source =
        "def:place(\"window_l1\", \"windows:window.residential.double.classic_01\", \"window_front_l1\")\n"
        "def:place(\"window_l2\", \"windows:window.residential.double.classic_02\", \"window_front_l2\")\n"
        "def:place(\"window_r1\", \"windows:window.residential.double.classic_01\", \"window_front_r1\")\n"
        "def:place(\"window_r2\", \"windows:window.residential.double.classic_02\", \"window_front_r2\")\n"
        "def:place(\"door\", \"doors:door.residential.wood_panel_02\", \"door_front\")\n"
        "def:place(\"roof\", \"roofs:roof.gable_clay_wide_01\", \"roof_mount\")\n";
    doc.addScript(wideScript);

    doc.defineObject("house.gable.modular_01", house);
    doc.defineObject("house.rowhouse.modular_01", rowhouse);
    doc.defineObject("house.gable.wide_01", wideHouse);
    doc.library->contentHash = "sha256:" + doc.computeLibraryContentHash();

    const auto path = outDir / "urban-buildings-1.0.0.mc3lib.json";
    doc.saveToLibraryJsonFile(path);
    std::printf("wrote %s (%zu definitions, %zu scripts)\n",
                path.string().c_str(), doc.definitions.size(), doc.scripts.size());
}

} // namespace

int main() {
    const std::filesystem::path outDir = "data/mc3lib";
    std::filesystem::create_directories(outDir);

    build_windows_library(outDir);
    build_doors_library(outDir);
    build_street_furniture_library(outDir);
    build_props_library(outDir);
    build_roofs_library(outDir);
    build_facades_library(outDir);
    build_vehicles_library(outDir);
    build_buildings_library(outDir);

    std::printf("done.\n");
    return 0;
}
