// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#include "generators/MeadowGenerator.hpp"
#include "MC3Writer.hpp"
#include "GenerationMetadata.hpp"
#include "ObjectBoundingBox.hpp"
#include <random>

namespace MeshWorld {

std::string MeadowGenerator::generate(const ChunkContext& ctx) {
    MC3Writer w(ctx);
    w.set_metadata_json(
        GenerationMetadata::from_chunk_context(ctx, "cpp.chunk.meadow").to_json()
    );
    const float s = ctx.chunk_size_m;
    std::mt19937_64 rng(ctx.seed);
    std::uniform_real_distribution<float> pos(1.0f, s - 1.0f);
    std::uniform_real_distribution<float> rot(0.0f, 360.0f);
    std::uniform_int_distribution<int> flower_pick(0, 3);
    const char* flowers[4] = {
        "flower_poppy","flower_daisy","flower_bluebell","flower_buttercup"
    };

    w.ground("grass_meadow");

    // Wildflowers scattered densely
    for (int i = 0; i < 35; ++i)
        w.instance("flower_" + std::to_string(i), flowers[flower_pick(rng)],
                   pos(rng), pos(rng), rot(rng));

    // A few trees at edges
    for (int i = 0; i < 6; ++i)
        w.instance("tree_" + std::to_string(i), "tree_oak",
                   pos(rng), pos(rng), rot(rng));

    // Small stream along one edge (water plane)
    std::uniform_int_distribution<int> stream_side(0, 1);
    if (stream_side(rng) == 0)
        w.plane("stream", 0, 0, 4.0f, s, "water_stream", -0.05f);
    else
        w.plane("stream", 0, 0, s, 4.0f, "water_stream", -0.05f);

    // Grass tufts
    for (int i = 0; i < 20; ++i)
        w.instance("tuft_" + std::to_string(i), "grass_tuft_tall",
                   pos(rng), pos(rng), rot(rng));

    // Isolated rock
    w.instance("rock_0", "rock_grey_small", pos(rng), pos(rng), rot(rng));

    // M160 — higher elevation adds a few extra scattered rocks (alpine
    // meadows above the treeline are rockier than lowland ones). Purely
    // additive (appended after every pre-M160 draw above), so the base
    // loops' own rng sequence — and everything before it — stays
    // byte-for-byte identical with or without a map layer attached.
    if (ctx.map_context.available && ctx.map_context.elevation_m > 800.0f) {
        const int extra_rocks = ctx.map_context.elevation_m > 1500.0f ? 4 : 2;
        for (int i = 0; i < extra_rocks; ++i)
            w.instance("alpine_rock_" + std::to_string(i), "rock_grey_small",
                       pos(rng), pos(rng), rot(rng));
    }

    return w.build();
}

// MAP20, M324 -- ModelPlacements for this chunk's wildflowers, edge trees,
// grass tufts, and its isolated rock, mirroring ForestGenerator's M168
// pattern (own independent RNG draw). The stream (generate()'s w.plane()
// call above) and the elevation-conditioned alpine-rock extras are NOT
// converted -- the stream is a raw MC3 primitive with no
// ObjectDefinitionLibrary definition_id, and the alpine extras are the same
// kind of conditional-extra content ForestGenerator's own placements()
// already leaves out of its "first slice" (that generator's river-mushroom
// extras aren't in its placements() either).
std::vector<ModelPlacement> MeadowGenerator::placements(const ChunkContext& ctx) const {
    std::vector<ModelPlacement> result;
    result.reserve(62);

    const float s = ctx.chunk_size_m;
    std::mt19937_64 rng(ctx.seed);
    std::uniform_real_distribution<float> pos(1.0f, s - 1.0f);
    std::uniform_real_distribution<float> rot(0.0f, 360.0f);
    std::uniform_int_distribution<int> flower_pick(0, 3);
    const char* flowers[4] = {
        "flower_poppy", "flower_daisy", "flower_bluebell", "flower_buttercup"
    };

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

    for (int i = 0; i < 35; ++i)
        add(flowers[flower_pick(rng)], pos(rng), pos(rng), rot(rng));

    for (int i = 0; i < 6; ++i)
        add("tree_oak", pos(rng), pos(rng), rot(rng));

    for (int i = 0; i < 20; ++i)
        add("grass_tuft_tall", pos(rng), pos(rng), rot(rng));

    add("rock_grey_small", pos(rng), pos(rng), rot(rng));

    return result;
}

} // namespace MeshWorld
