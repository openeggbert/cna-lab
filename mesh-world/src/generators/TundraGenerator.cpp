// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#include "generators/TundraGenerator.hpp"
#include "MC3Writer.hpp"
#include "GenerationMetadata.hpp"
#include "ObjectBoundingBox.hpp"
#include <random>

namespace MeshWorld {

std::string TundraGenerator::generate(const ChunkContext& ctx) {
    MC3Writer w(ctx);
    w.set_metadata_json(
        GenerationMetadata::from_chunk_context(ctx, "cpp.chunk.tundra").to_json()
    );
    const float s = ctx.chunk_size_m;
    std::mt19937_64 rng(ctx.seed);
    std::uniform_real_distribution<float> pos(1.5f, s - 1.5f);
    std::uniform_real_distribution<float> rot(0.0f, 360.0f);
    std::uniform_real_distribution<float> rsize(0.8f, 3.5f);
    std::uniform_real_distribution<float> isize(3.0f, 10.0f);

    w.ground("tundra_snow");

    // Ice/snow variation patches
    for (int i = 0; i < 8; ++i) {
        float pw = isize(rng), pd = isize(rng);
        w.plane("ice_" + std::to_string(i),
                pos(rng) - pw*0.5f, pos(rng) - pd*0.5f,
                pw, pd, "ice_sheet", 0.03f);
    }

    // Snow-covered boulders
    for (int i = 0; i < 8; ++i) {
        float rs = rsize(rng);
        w.box("boulder_" + std::to_string(i),
              pos(rng), pos(rng), rs, rs * 0.6f, rs * 0.8f,
              "rock_snow_covered", 0.0f, rot(rng));
    }

    // Bare dead trees (very sparse)
    for (int i = 0; i < 4; ++i)
        w.instance("bare_tree_" + std::to_string(i), "tree_bare_winter",
                   pos(rng), pos(rng), rot(rng));

    // Sparse tundra vegetation (lichen, moss)
    for (int i = 0; i < 10; ++i)
        w.instance("lichen_" + std::to_string(i), "plant_lichen",
                   pos(rng), pos(rng), rot(rng));

    // Frozen stream (thin low water plane)
    std::uniform_int_distribution<int> stream_dir(0, 1);
    if (stream_dir(rng) == 0)
        w.plane("frozen_stream", s*0.5f - 2.0f, 0, 4.0f, s, "ice_stream", 0.02f);
    else
        w.plane("frozen_stream", 0, s*0.5f - 2.0f, s, 4.0f, "ice_stream", 0.02f);

    // MAP17 -- extra snow-covered boulders at high elevation (mountain
    // tundra vs. lowland tundra), mirroring MeadowGenerator's own M160
    // alpine-rock pattern. Purely additive after every pre-MAP17 draw
    // above.
    if (ctx.map_context.available && ctx.map_context.elevation_m > 1000.0f) {
        const int extra = ctx.map_context.elevation_m > 2000.0f ? 6 : 3;
        for (int i = 0; i < extra; ++i) {
            float rs = rsize(rng);
            w.box("alpine_boulder_" + std::to_string(i),
                  pos(rng), pos(rng), rs, rs * 0.6f, rs * 0.8f,
                  "rock_snow_covered", 0.0f, rot(rng));
        }
    }

    return w.build();
}

// MAP20, M322 -- ModelPlacements for this chunk's sparse bare trees +
// ground lichen, mirroring ForestGenerator's M168 pattern (own independent
// RNG draw). Snow-covered boulders/ice patches/frozen stream (generate()'s
// w.box()/w.plane() calls above) are NOT converted -- raw MC3 primitives,
// no ObjectDefinitionLibrary definition_id (same scope decision as the
// other MAP20 generators' own placements()).
std::vector<ModelPlacement> TundraGenerator::placements(const ChunkContext& ctx) const {
    std::vector<ModelPlacement> result;
    result.reserve(14);

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

    for (int i = 0; i < 4; ++i)
        add("tree_bare_winter", pos(rng), pos(rng), rot(rng));

    for (int i = 0; i < 10; ++i)
        add("plant_lichen", pos(rng), pos(rng), rot(rng));

    return result;
}

} // namespace MeshWorld
