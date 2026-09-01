// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#include "generators/ParkGenerator.hpp"
#include "MC3Writer.hpp"
#include "GenerationMetadata.hpp"
#include "StyleRegistry.hpp"
#include <random>
#include <cmath>

namespace MeshWorld {

std::string ParkGenerator::generate(const ChunkContext& ctx) {
    MC3Writer w(ctx);
    w.set_metadata_json(
        GenerationMetadata::from_chunk_context(ctx, "cpp.chunk.park").to_json()
    );
    const float s  = ctx.chunk_size_m;
    const float c  = s * 0.5f;
    const float pw = 3.0f;   // path width

    const Style* style_ptr = StyleRegistry::instance().has(ctx.style)
                             ? &StyleRegistry::instance().get(ctx.style)
                             : nullptr;
    auto mat = [&](const std::string& key, const char* fallback) -> std::string {
        if (style_ptr) return style_ptr->mat(key, fallback);
        return fallback;
    };

    w.ground(mat("park.ground", "grass_park"));

    // ── Ring path around fountain ─────────────────────────────────────
    const float rr  = 9.0f;   // ring radius (center to middle of ring path)
    const float rph = 2.0f;   // ring path half-width
    const auto park_path = mat("park.path", "path_gravel");
    // Four straight segments approximating the circular ring
    w.plane("ring_n",  c - rr - rph, c - rr - rph, (rr + rph) * 2.0f, rph * 2.0f, park_path);
    w.plane("ring_s",  c - rr - rph, c + rr - rph, (rr + rph) * 2.0f, rph * 2.0f, park_path);
    w.plane("ring_w",  c - rr - rph, c - rr + rph, rph * 2.0f, (rr - rph) * 2.0f, park_path);
    w.plane("ring_e",  c + rr - rph, c - rr + rph, rph * 2.0f, (rr - rph) * 2.0f, park_path);

    // ── Paths from road exits toward ring ─────────────────────────────
    if (ctx.exits.north_road || ctx.exits.north_path)
        w.plane("path_n", c - pw * 0.5f, 0.0f,   pw, c - rr - rph, park_path);
    if (ctx.exits.south_road || ctx.exits.south_path)
        w.plane("path_s", c - pw * 0.5f, c + rr + rph, pw, c - rr - rph, park_path);
    if (ctx.exits.west_road  || ctx.exits.west_path)
        w.plane("path_w", 0.0f, c - pw * 0.5f, c - rr - rph, pw, park_path);
    if (ctx.exits.east_road  || ctx.exits.east_path)
        w.plane("path_e", c + rr + rph, c - pw * 0.5f, c - rr - rph, pw, park_path);

    // ── Fountain ──────────────────────────────────────────────────────
    w.cylinder("fountain_base",  c, c, 2.8f, 0.5f,  mat("park.fountain.base",  "stone_granite"));
    w.cylinder("fountain_basin", c, c, 2.3f, 0.35f, mat("park.fountain.basin", "stone_light"));
    w.cylinder("fountain_water", c, c, 1.8f, 0.25f, mat("park.fountain.water", "water"));
    w.cylinder("fountain_jet",   c, c, 0.08f, 2.2f, mat("park.fountain.water", "water"), 0.75f);

    // ── Flower beds between fountain and ring path ─────────────────────
    const float fb = 5.5f;
    const auto fb_a = mat("park.flowerbed.a", "flower_red");
    const auto fb_b = mat("park.flowerbed.b", "flower_yellow");
    w.box("flowerbed_n", c,      c - fb, 2.5f, 0.35f, 1.2f, fb_a);
    w.box("flowerbed_s", c,      c + fb, 2.5f, 0.35f, 1.2f, fb_b);
    w.box("flowerbed_e", c + fb, c,      1.2f, 0.35f, 2.5f, fb_a);
    w.box("flowerbed_w", c - fb, c,      1.2f, 0.35f, 2.5f, fb_b);

    // ── Benches: 4 cardinal (close, face fountain) + 4 diagonal ──────
    const float br1 = 4.5f;   // inner ring radius
    const float br2 = 13.0f;  // outer ring radius
    struct BenchPt { float x, z, ry; };
    const BenchPt inner[4] = {
        {c,       c - br1,  0.0f},
        {c + br1, c,       90.0f},
        {c,       c + br1, 180.0f},
        {c - br1, c,       270.0f},
    };
    const BenchPt outer[4] = {
        {c + br2 * 0.71f, c - br2 * 0.71f,  45.0f},
        {c + br2 * 0.71f, c + br2 * 0.71f, 135.0f},
        {c - br2 * 0.71f, c + br2 * 0.71f, 225.0f},
        {c - br2 * 0.71f, c - br2 * 0.71f, 315.0f},
    };
    for (int i = 0; i < 4; ++i)
        w.instance("bench_in_"  + std::to_string(i), "bench_park", inner[i].x, inner[i].z, inner[i].ry);
    for (int i = 0; i < 4; ++i)
        w.instance("bench_out_" + std::to_string(i), "bench_park", outer[i].x, outer[i].z, outer[i].ry);

    // ── Lamp posts: corners of inner ring + at exit paths ─────────────
    // Instances resolve to ObjectDefinitionLibrary lamp_post_ornate (pole+head+base).
    const float lr = 11.0f;
    w.instance("lamp_ne", "lamp_post_ornate", c + lr, c - lr);
    w.instance("lamp_nw", "lamp_post_ornate", c - lr, c - lr);
    w.instance("lamp_se", "lamp_post_ornate", c + lr, c + lr);
    w.instance("lamp_sw", "lamp_post_ornate", c - lr, c + lr);

    if (ctx.exits.north_road || ctx.exits.north_path) {
        w.instance("lamp_exit_nw", "lamp_post_ornate", c - pw - 0.8f, 3.0f);
        w.instance("lamp_exit_ne", "lamp_post_ornate", c + pw + 0.8f, 3.0f);
    }
    if (ctx.exits.south_road || ctx.exits.south_path) {
        w.instance("lamp_exit_sw", "lamp_post_ornate", c - pw - 0.8f, s - 3.0f);
        w.instance("lamp_exit_se", "lamp_post_ornate", c + pw + 0.8f, s - 3.0f);
    }
    if (ctx.exits.west_road || ctx.exits.west_path) {
        w.instance("lamp_exit_wn", "lamp_post_ornate", 3.0f, c - pw - 0.8f);
        w.instance("lamp_exit_ws", "lamp_post_ornate", 3.0f, c + pw + 0.8f);
    }
    if (ctx.exits.east_road || ctx.exits.east_path) {
        w.instance("lamp_exit_en", "lamp_post_ornate", s - 3.0f, c - pw - 0.8f);
        w.instance("lamp_exit_es", "lamp_post_ornate", s - 3.0f, c + pw + 0.8f);
    }

    // ── Varied trees (seed-driven positions, types, and scales) ───────
    std::mt19937_64 rng(ctx.seed);
    std::uniform_real_distribution<float> pos(4.0f, s - 4.0f);
    std::uniform_real_distribution<float> rot(0.0f, 360.0f);
    std::uniform_real_distribution<float> scale(0.85f, 1.25f);
    const char* tree_refs[4] = {"tree_lime", "tree_chestnut", "tree_oak", "tree_birch"};
    std::uniform_int_distribution<int> tree_pick(0, 3);

    int planted = 0;
    for (int attempt = 0; attempt < 400 && planted < 22; ++attempt) {
        float tx = pos(rng), tz = pos(rng);
        // Avoid fountain area, ring path, and exit paths
        float dx = tx - c, dz = tz - c;
        float dist = std::sqrt(dx * dx + dz * dz);
        if (dist < rr + 2.5f) continue;                          // inside ring
        if (std::fabs(tx - c) < pw + 1.5f && tz < c - rr) continue; // N path
        if (std::fabs(tx - c) < pw + 1.5f && tz > c + rr) continue; // S path
        if (std::fabs(tz - c) < pw + 1.5f && tx < c - rr) continue; // W path
        if (std::fabs(tz - c) < pw + 1.5f && tx > c + rr) continue; // E path
        w.instance("tree_" + std::to_string(planted),
                   tree_refs[tree_pick(rng)], tx, tz, rot(rng), 0.0f, scale(rng));
        ++planted;
    }

    // MAP17 -- an extra pair of benches near a mapped named place (a park
    // next to a real settlement gets more visitor amenities than an
    // interior/rural one). Purely additive after every pre-MAP17 draw
    // above, so the base rng sequence is identical whether or not a map
    // layer is attached (same "no behavior change when unavailable" rule
    // M160 established).
    if (ctx.map_context.available && !ctx.map_context.nearest_place_name.empty()) {
        w.instance("bench_place_0", "bench_park", c + rr + 2.0f, c, 90.0f);
        w.instance("bench_place_1", "bench_park", c - rr - 2.0f, c, 270.0f);
    }

    return w.build();
}

} // namespace MeshWorld
