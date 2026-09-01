// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#include "generators/JungleGenerator.hpp"
#include "MC3Writer.hpp"
#include "NatureAssets.hpp"
#include "GenerationMetadata.hpp"
#include "ObjectBoundingBox.hpp"
#include <random>

namespace MeshWorld {

std::string JungleGenerator::generate(const ChunkContext& ctx) {
    MC3Writer w(ctx);
    w.set_metadata_json(
        GenerationMetadata::from_chunk_context(ctx, "cpp.chunk.jungle").to_json()
    );
    const float s = ctx.chunk_size_m;
    std::mt19937_64 rng(ctx.seed);
    std::uniform_real_distribution<float> pos(1.0f, s - 1.0f);
    std::uniform_real_distribution<float> rot(0.0f, 360.0f);
    std::uniform_real_distribution<float> sc(0.9f, 1.6f);
    std::uniform_int_distribution<int> tree_pick(0, 3);
    const char* trees[4] = {"tree_palm","tree_banyan","tree_tropical_fern","tree_bamboo"};

    w.ground("jungle_floor");
    w.plane("undergrowth", 0, 0, s, s, "jungle_undergrowth", 0.05f);

    for (int i = 0; i < 45; ++i)
        w.instance("tree_" + std::to_string(i),
                   [&] { const auto* asset = pick_nature_asset(ctx, "nature_tree", "jungle", i);
                          return asset ? resolve_nature_asset_id(*asset, ctx) : std::string(trees[tree_pick(rng)]); }(),
                   pos(rng), pos(rng), rot(rng), 0.0f, sc(rng));

    for (int i = 0; i < 8; ++i)
        w.cylinder("vine_" + std::to_string(i),
                   pos(rng), pos(rng), 0.05f, 6.0f, "vine_green", 2.0f);

    for (int i = 0; i < 5; ++i)
        w.instance("rock_" + std::to_string(i), "rock_mossy",
                   pos(rng), pos(rng), rot(rng));

    // MAP17 -- extra vines near a mapped river, mirroring ForestGenerator's
    // own M160 river-proximity pattern (jungle vegetation is even denser
    // near water than the base loop above already assumes). Purely
    // additive after every pre-MAP17 draw above.
    if (ctx.map_context.available && ctx.map_context.nearest_river_distance_m >= 0.0f
        && ctx.map_context.nearest_river_distance_m < 100.0f) {
        for (int i = 0; i < 4; ++i)
            w.cylinder("river_vine_" + std::to_string(i),
                       pos(rng), pos(rng), 0.05f, 6.0f, "vine_green", 2.0f);
    }

    return w.build();
}

// MAP20, M319 -- ModelPlacements for this chunk's canopy trees + mossy
// rocks, mirroring ForestGenerator's M168 pattern (own independent RNG
// draw, not a bit-match of generate()'s inline sequence -- see that
// generator's own doc comment for why). Vines (generate()'s w.cylinder()
// calls above) are NOT converted here: ModelPlacement::definition_id
// references an ObjectDefinitionLibrary entry, and vines are drawn as raw
// MC3 primitives with no such definition -- same "only instance() calls
// become placements" scope ForestGenerator's own fallen-logs/litter boxes
// were already excluded from.
std::vector<ModelPlacement> JungleGenerator::placements(const ChunkContext& ctx) const {
    std::vector<ModelPlacement> result;
    result.reserve(50);

    const float s = ctx.chunk_size_m;
    std::mt19937_64 rng(ctx.seed);
    std::uniform_real_distribution<float> pos(1.0f, s - 1.0f);
    std::uniform_real_distribution<float> rot(0.0f, 360.0f);
    std::uniform_real_distribution<float> sc(0.9f, 1.6f);
    std::uniform_int_distribution<int> tree_pick(0, 3);
    const char* trees[4] = {"tree_palm", "tree_banyan", "tree_tropical_fern", "tree_bamboo"};

    const int chunk_size_i = static_cast<int>(ctx.chunk_size_m);
    const double origin_x = ctx.coord.world_x(chunk_size_i);
    const double origin_z = ctx.coord.world_z(chunk_size_i);
    const double ground_elevation_m =
        ctx.map_context.available ? static_cast<double>(ctx.map_context.elevation_m) : 0.0;

    const auto add = [&](const std::string& definition_id, float local_x, float local_z,
                          float ry, float scale) {
        ModelPlacement p;
        p.definition_id = definition_id;
        p.pos_x = origin_x + local_x;
        p.pos_z = origin_z + local_z;
        p.pos_y = ground_elevation_m;
        p.y_min = ground_elevation_m;
        const float height_m = object_height_m(definition_id);
        p.y_max = ground_elevation_m + static_cast<double>(height_m);
        p.rot_y = ry;
        p.scale = scale;
        // M328 -- LOD tier derived from this definition's own real
        // (M327, geometry-derived) height, not a fixed 0.
        p.lod_min = lod_tier_for_height(height_m);
        result.push_back(std::move(p));
    };

    for (int i = 0; i < 45; ++i)
        add(trees[tree_pick(rng)], pos(rng), pos(rng), rot(rng), sc(rng));

    for (int i = 0; i < 5; ++i)
        add("rock_mossy", pos(rng), pos(rng), rot(rng), 1.0f);

    return result;
}

} // namespace MeshWorld
