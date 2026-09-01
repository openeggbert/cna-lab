// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#include "generators/ForestGenerator.hpp"
#include "MC3Writer.hpp"
#include "NatureAssets.hpp"
#include "GenerationMetadata.hpp"
#include "ObjectBoundingBox.hpp"
#include <cmath>
#include <cstdint>
#include <random>
#include <utility>

namespace MeshWorld {

namespace {

struct Clearing {
    float x{0.0f};
    float z{0.0f};
    static constexpr float kRadiusM = 5.0f;
};

Clearing make_clearing(std::uint64_t seed, float chunk_size_m) {
    // A separate stream keeps this structural decision stable when a later
    // foliage variant changes the main placement RNG draw count.
    std::mt19937_64 layout_rng(seed ^ 0xC1EA71A6ULL);
    std::uniform_real_distribution<float> pos(12.0f, chunk_size_m - 12.0f);
    return {pos(layout_rng), pos(layout_rng)};
}

bool outside_clearing(float x, float z, const Clearing& clearing, float margin = 0.0f) {
    const float dx = x - clearing.x;
    const float dz = z - clearing.z;
    const float radius = Clearing::kRadiusM + margin;
    return dx * dx + dz * dz >= radius * radius;
}

std::pair<float, float> sample_tree_position(std::mt19937_64& rng, float s,
                                              const Clearing& clearing) {
    std::uniform_real_distribution<float> pos(1.5f, s - 1.5f);
    // A 5 m clearing occupies under 2% of a 64 m chunk; eight attempts make
    // accidental canopy placement in it vanishingly unlikely while retaining
    // a bounded deterministic runtime.
    for (int attempt = 0; attempt < 8; ++attempt) {
        const float x = pos(rng);
        const float z = pos(rng);
        if (outside_clearing(x, z, clearing, 1.5f)) return {x, z};
    }
    // The bounded fallback is still deterministic and safe at unusual small
    // chunk sizes, where a strict rejection loop could otherwise never end.
    return {1.5f, 1.5f};
}

// T226 -- a few dense tree clusters, distinct from generate()'s own
// uniform-scatter loop (real forests aren't perfectly uniform -- trees
// tend to clump). Each cluster is a center point + a handful of extra
// trees jittered tightly around it (small radius), supplementing (not
// replacing) the uniform scatter above it in generate()'s own draw order.
void add_tree_clusters(MC3Writer& w, std::mt19937_64& rng, float s,
                       const char* const trees[4], const Clearing& clearing) {
    std::uniform_real_distribution<float> center_pos(6.0f, s - 6.0f);
    std::uniform_real_distribution<float> jitter(-2.5f, 2.5f);
    std::uniform_real_distribution<float> rot(0.0f, 360.0f);
    std::uniform_real_distribution<float> sc(0.8f, 1.3f);
    std::uniform_int_distribution<int> tree_pick(0, 3);

    constexpr int kClusterCount = 2;
    constexpr int kTreesPerCluster = 5;
    for (int c = 0; c < kClusterCount; ++c) {
        float cx = center_pos(rng);
        float cz = center_pos(rng);
        for (int attempt = 0; attempt < 8 && !outside_clearing(cx, cz, clearing, 4.0f); ++attempt) {
            cx = center_pos(rng);
            cz = center_pos(rng);
        }
        for (int i = 0; i < kTreesPerCluster; ++i) {
            float x = std::clamp(cx + jitter(rng), 1.5f, s - 1.5f);
            float z = std::clamp(cz + jitter(rng), 1.5f, s - 1.5f);
            // A center outside the clearing is not by itself sufficient:
            // jitter could still push an individual canopy back into it.
            // Keep the retry bounded and share the normal-tree fallback so
            // every authored tree obeys the same exclusion radius.
            for (int attempt = 0;
                 attempt < 4 && !outside_clearing(x, z, clearing, 1.5f);
                 ++attempt) {
                x = std::clamp(cx + jitter(rng), 1.5f, s - 1.5f);
                z = std::clamp(cz + jitter(rng), 1.5f, s - 1.5f);
            }
            if (!outside_clearing(x, z, clearing, 1.5f)) {
                std::tie(x, z) = sample_tree_position(rng, s, clearing);
            }
            w.instance("cluster_" + std::to_string(c) + "_" + std::to_string(i),
                      trees[tree_pick(rng)], x, z, rot(rng), 0.0f, sc(rng));
        }
    }
}

// T227 -- small bush clusters: each bush is 2-3 small overlapping cylinders
// (a cheap, cheap-to-render leafy-clump silhouette, same "primitives, not a
// dedicated mesh" approach every other small-object generator in this file
// already uses for logs/mushrooms).
void add_bushes(MC3Writer& w, std::mt19937_64& rng, float s) {
    std::uniform_real_distribution<float> pos(2.0f, s - 2.0f);
    std::uniform_real_distribution<float> radius(0.4f, 0.7f);
    constexpr int kBushCount = 5;
    for (int i = 0; i < kBushCount; ++i) {
        const float x = pos(rng);
        const float z = pos(rng);
        const float r = radius(rng);
        w.cylinder("bush_" + std::to_string(i) + "_a", x,        z,        r,        r * 1.2f, "shrub_foliage");
        w.cylinder("bush_" + std::to_string(i) + "_b", x + r*0.5f, z + r*0.3f, r * 0.7f, r,        "shrub_foliage");
    }
}

// R143b -- an actual open patch: the same Clearing is supplied to tree and
// cluster placement, so this is no longer merely green ground hidden below a
// full canopy.
void add_clearing(MC3Writer& w, const Clearing& clearing) {
    constexpr float kClearingSize = Clearing::kRadiusM * 2.0f;
    w.plane("clearing", clearing.x - kClearingSize * 0.5f,
           clearing.z - kClearingSize * 0.5f,
           kClearingSize, kClearingSize, "grass_courtyard", 0.015f);
}

// T229 -- a winding dirt path crossing the chunk: a short random walk of
// connected plane segments (same "cheap per-segment plane strip" approach
// city.lua's own street-grid bending uses, simplified to a single path
// rather than a full grid).
void add_forest_path(MC3Writer& w, std::mt19937_64& rng, float s) {
    std::uniform_real_distribution<float> start_z(s * 0.3f, s * 0.7f);
    std::uniform_real_distribution<float> drift(-3.0f, 3.0f);
    constexpr float kPathWidth = 1.5f;
    constexpr float kSegmentLen = 6.0f;

    float x = 0.0f;
    float z = std::clamp(start_z(rng), kPathWidth, s - kPathWidth);
    int seg = 0;
    while (x < s) {
        const float next_z = std::clamp(z + drift(rng), kPathWidth, s - kPathWidth);
        const float len = std::min(kSegmentLen, s - x);
        w.plane("path_" + std::to_string(seg), x, next_z - kPathWidth * 0.5f,
               len, kPathWidth, "path_gravel");
        x += kSegmentLen;
        z = next_z;
        ++seg;
    }
}

} // namespace

std::string ForestGenerator::generate(const ChunkContext& ctx) {
    MC3Writer w(ctx);
    w.set_metadata_json(
        GenerationMetadata::from_chunk_context(ctx, "cpp.chunk.forest").to_json()
    );
    const float s = ctx.chunk_size_m;
    std::mt19937_64 rng(ctx.seed);
    std::uniform_real_distribution<float> pos(1.5f, s - 1.5f);
    // Planes use their supplied x/z as their minimum corner. Reserve their
    // maximum 7.2 m width / 3 m depth so litter cannot cross a chunk edge.
    std::uniform_real_distribution<float> litter_x(1.5f, s - 7.5f);
    std::uniform_real_distribution<float> litter_z(1.5f, s - 3.5f);
    std::uniform_real_distribution<float> rot(0.0f, 360.0f);
    std::uniform_real_distribution<float> sc(0.8f, 1.3f);
    std::uniform_int_distribution<int> tree_pick(0, 3);
    const char* trees[4] = {"tree_oak","tree_pine","tree_birch","tree_beech"};
    const Clearing clearing = make_clearing(ctx.seed, s);

    w.ground("forest_floor");
    // Leaf litter patches
    for (int i = 0; i < 6; ++i)
        w.plane("litter_" + std::to_string(i), litter_x(rng), litter_z(rng),
                4.0f + pos(rng)*0.05f, 3.0f, "leaf_litter", 0.01f);

    for (int i = 0; i < 32; ++i) {
        const auto [x, z] = sample_tree_position(rng, s, clearing);
        w.instance("tree_" + std::to_string(i),
                   [&] { const auto* asset = pick_nature_asset(ctx, "nature_tree", "temperate_forest", i);
                          return asset ? resolve_nature_asset_id(*asset, ctx) : std::string(trees[tree_pick(rng)]); }(),
                   x, z, rot(rng), 0.0f, sc(rng));
    }

    // Fallen logs
    std::uniform_real_distribution<float> log_r(0.0f, 1.0f);
    for (int i = 0; i < 4; ++i)
        w.box("log_" + std::to_string(i),
              pos(rng), pos(rng), 3.0f + log_r(rng)*2.0f, 0.4f, 0.4f,
              "bark_log", 0.0f, rot(rng));

    // Mushrooms
    for (int i = 0; i < 6; ++i)
        w.instance("mushroom_" + std::to_string(i), "mushroom_brown",
                   pos(rng), pos(rng), rot(rng));

    // M160 — extra moisture-loving mushrooms near a river. Purely additive
    // (appended after every pre-M160 draw above), so the base 6-mushroom
    // loop's own rng sequence — and everything before it — stays
    // byte-for-byte identical with or without a map layer attached.
    // nearest_river_distance_m < 0 means "no river found" (see MapContext's
    // own doc comment), not "distance zero" — must not be treated as close.
    if (ctx.map_context.available && ctx.map_context.nearest_river_distance_m >= 0.0f
        && ctx.map_context.nearest_river_distance_m < 200.0f) {
        const int extra = ctx.map_context.nearest_river_distance_m < 80.0f ? 4 : 2;
        for (int i = 0; i < extra; ++i)
            w.instance("river_mushroom_" + std::to_string(i), "mushroom_brown",
                       pos(rng), pos(rng), rot(rng));
    }

    // T226-229 -- dense tree clusters, bushes, a clearing, and a winding
    // path, each its own focused helper (see their own doc comments above)
    // rather than folding more inline loops into this already-long
    // function. Purely additive, appended after every pre-existing draw
    // above (same "extend the rng sequence, don't disturb it" discipline
    // the M160 river-mushroom block above already established).
    add_tree_clusters(w, rng, s, trees, clearing);
    add_bushes(w, rng, s);
    add_clearing(w, clearing);
    add_forest_path(w, rng, s);

    return w.build();
}

// M168 (MAP11) -- emits ModelPlacements for this chunk's trees, the first
// generator slice proving the mechanism (mirrors M160's "2-of-~20 generator
// slice" precedent for map_context). Uses its own independent RNG draw
// rather than trying to bit-match generate()'s inline tree instances above:
// generate()'s single-expression w.instance(..., trees[tree_pick(rng)],
// pos(rng), pos(rng), rot(rng), 0.0f, sc(rng)) call already has an
// unspecified (compiler-defined) function-argument evaluation order, so
// there is no reliable "exact" sequence to replicate here anyway. This is a
// new, separate output channel (the persistent 3D model store) -- it does
// not need to visually equal today's non-persisted inline MC3 preview;
// migrating the preview itself to stream from placements is a later task
// (M175). M170 -- y_min/y_max come from object_height_m()'s approximate
// per-definition lookup (this codebase has no real bounding-box
// infrastructure to derive them from geometry) plus ground elevation.
std::vector<ModelPlacement> ForestGenerator::placements(const ChunkContext& ctx) const {
    std::vector<ModelPlacement> result;
    result.reserve(32);

    const float s = ctx.chunk_size_m;
    std::mt19937_64 rng(ctx.seed);
    std::uniform_real_distribution<float> pos(1.5f, s - 1.5f);
    std::uniform_real_distribution<float> rot(0.0f, 360.0f);
    std::uniform_real_distribution<float> sc(0.8f, 1.3f);
    std::uniform_int_distribution<int> tree_pick(0, 3);
    const char* trees[4] = {"tree_oak", "tree_pine", "tree_birch", "tree_beech"};

    // World-space chunk origin. ChunkCoord::world_x/world_z return float
    // (an existing, pre-M168 precision limit at true planetary scale, not
    // something this task changes) -- widened to double only when combined
    // with the local offset below, matching ModelPlacement's own
    // double-precision field types.
    const int chunk_size_i = static_cast<int>(ctx.chunk_size_m);
    const double origin_x = ctx.coord.world_x(chunk_size_i);
    const double origin_z = ctx.coord.world_z(chunk_size_i);

    // M170 -- ground elevation from the map layer when available, mirroring
    // the existing available-gated fallback pattern generate() already uses
    // (see its own M160 river-mushroom check above): falls back to 0.0f when
    // unavailable, today's actual behavior since nothing wires a real
    // MapPipeline into any real binary yet (NEXT.md section 5 #17).
    const double ground_elevation_m =
        ctx.map_context.available ? static_cast<double>(ctx.map_context.elevation_m) : 0.0;

    for (int i = 0; i < 32; ++i) {
        const int tree_index = tree_pick(rng);
        const float local_x = pos(rng);
        const float local_z = pos(rng);
        const float ry = rot(rng);
        const float scale = sc(rng);
        const std::string definition_id = trees[tree_index];

        const float height_m = object_height_m(definition_id);

        ModelPlacement p;
        p.definition_id = definition_id;
        p.pos_x = origin_x + local_x;
        p.pos_z = origin_z + local_z;
        p.pos_y = ground_elevation_m;
        p.y_min = ground_elevation_m;
        p.y_max = ground_elevation_m + static_cast<double>(height_m);
        p.rot_y = ry;
        p.scale = scale;
        // M328 -- LOD tier derived from this definition's own real
        // (M327, geometry-derived) height, not a fixed 0: taller trees stay
        // visible from farther away (see lod_tier_for_height()'s own doc
        // comment).
        p.lod_min = lod_tier_for_height(height_m);
        result.push_back(std::move(p));
    }

    return result;
}

} // namespace MeshWorld
