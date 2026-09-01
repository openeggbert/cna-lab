// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#include "generators/SwampGenerator.hpp"
#include "MC3Writer.hpp"
#include "NatureAssets.hpp"
#include "GenerationMetadata.hpp"
#include "ObjectBoundingBox.hpp"
#include <random>

namespace MeshWorld {

std::string SwampGenerator::generate(const ChunkContext& ctx) {
    MC3Writer w(ctx);
    w.set_metadata_json(
        GenerationMetadata::from_chunk_context(ctx, "cpp.chunk.swamp").to_json()
    );
    const float s = ctx.chunk_size_m;
    std::mt19937_64 rng(ctx.seed);
    std::uniform_real_distribution<float> pos(1.5f, s - 1.5f);
    std::uniform_real_distribution<float> rot(0.0f, 360.0f);
    std::uniform_real_distribution<float> wsize(4.0f, 14.0f);

    w.ground("swamp_mud");

    // Murky water pools
    for (int i = 0; i < 5; ++i) {
        float wx = wsize(rng), wz = wsize(rng);
        w.plane("pool_" + std::to_string(i),
                pos(rng) - wx*0.5f, pos(rng) - wz*0.5f,
                wx, wz, "water_swamp", -0.05f);
    }

    // Dead and gnarled trees
    for (int i = 0; i < 14; ++i)
        w.instance("dead_tree_" + std::to_string(i),
                   [&] { const auto* asset = pick_nature_asset(ctx, "nature_tree", "swamp", i);
                          return asset ? resolve_nature_asset_id(*asset, ctx) : std::string("tree_dead_gnarled"); }(),
                   pos(rng), pos(rng), rot(rng));

    // Lily pads on water
    for (int i = 0; i < 10; ++i)
        w.plane("lilypad_" + std::to_string(i),
                pos(rng) - 0.6f, pos(rng) - 0.6f,
                1.2f, 1.2f, "plant_lily_pad", 0.02f);

    // Marsh grass clumps
    for (int i = 0; i < 18; ++i)
        w.instance("marsh_" + std::to_string(i), "plant_marsh_grass",
                   pos(rng), pos(rng), rot(rng));

    // Fog wisps (low cylinder placeholders)
    for (int i = 0; i < 4; ++i)
        w.cylinder("mist_" + std::to_string(i),
                   pos(rng), pos(rng), 4.0f, 0.5f, "vfx_mist", 0.1f);

    // MAP17 -- extra murky pools right at/below sea level (a genuinely
    // waterlogged lowland swamp rather than a merely damp one). Purely
    // additive after every pre-MAP17 draw above.
    if (ctx.map_context.available && ctx.map_context.elevation_m < 20.0f) {
        for (int i = 0; i < 3; ++i) {
            float wx = wsize(rng), wz = wsize(rng);
            w.plane("deep_pool_" + std::to_string(i),
                    pos(rng) - wx*0.5f, pos(rng) - wz*0.5f,
                    wx, wz, "water_swamp", -0.05f);
        }
    }

    return w.build();
}

// MAP20, M323 -- ModelPlacements for this chunk's dead gnarled trees +
// marsh grass (reeds), mirroring ForestGenerator's M168 pattern (own
// independent RNG draw). Water pools/lily pads/mist wisps (generate()'s
// w.plane()/w.cylinder() calls above) are NOT converted -- raw MC3
// primitives, no ObjectDefinitionLibrary definition_id (same scope
// decision as the other MAP20 generators' own placements()).
std::vector<ModelPlacement> SwampGenerator::placements(const ChunkContext& ctx) const {
    std::vector<ModelPlacement> result;
    result.reserve(32);

    const float s = ctx.chunk_size_m;
    std::mt19937_64 rng(ctx.seed);
    std::uniform_real_distribution<float> pos(1.5f, s - 1.5f);
    std::uniform_real_distribution<float> rot(0.0f, 360.0f);

    const int chunk_size_i = static_cast<int>(ctx.chunk_size_m);
    const double origin_x = ctx.coord.world_x(chunk_size_i);
    const double origin_z = ctx.coord.world_z(chunk_size_i);
    const double ground_elevation_m =
        ctx.map_context.available ? static_cast<double>(ctx.map_context.elevation_m) : 0.0;

    const auto add = [&](const std::string& definition_id, float local_x, float local_z, float ry) {
        ModelPlacement p;
        p.definition_id = definition_id;
        p.pos_x = origin_x + local_x;
        p.pos_z = origin_z + local_z;
        p.pos_y = ground_elevation_m;
        p.y_min = ground_elevation_m;
        const float height_m = object_height_m(definition_id);
        p.y_max = ground_elevation_m + static_cast<double>(height_m);
        p.rot_y = ry;
        p.scale = 1.0f;
        // M328 -- LOD tier derived from this definition's own real
        // (M327, geometry-derived) height, not a fixed 0.
        p.lod_min = lod_tier_for_height(height_m);
        result.push_back(std::move(p));
    };

    for (int i = 0; i < 14; ++i)
        add("tree_dead_gnarled", pos(rng), pos(rng), rot(rng));

    for (int i = 0; i < 18; ++i)
        add("plant_marsh_grass", pos(rng), pos(rng), rot(rng));

    return result;
}

} // namespace MeshWorld
