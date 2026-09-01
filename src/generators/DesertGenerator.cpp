// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#include "generators/DesertGenerator.hpp"
#include "MC3Writer.hpp"
#include "NatureAssets.hpp"
#include "GenerationMetadata.hpp"
#include "ObjectBoundingBox.hpp"
#include <random>

namespace MeshWorld {

std::string DesertGenerator::generate(const ChunkContext& ctx) {
    MC3Writer w(ctx);
    w.set_metadata_json(
        GenerationMetadata::from_chunk_context(ctx, "cpp.chunk.desert").to_json()
    );
    const float s = ctx.chunk_size_m;
    std::mt19937_64 rng(ctx.seed);
    std::uniform_real_distribution<float> pos(2.0f, s - 2.0f);
    std::uniform_real_distribution<float> rot(0.0f, 360.0f);
    std::uniform_real_distribution<float> dh(0.3f, 1.8f);
    std::uniform_real_distribution<float> dw(8.0f, 18.0f);
    std::uniform_real_distribution<float> rsize(0.6f, 2.5f);

    w.ground("sand");

    for (int i = 0; i < 6; ++i)
        w.box("dune_" + std::to_string(i),
              pos(rng), pos(rng), dw(rng), dh(rng), dw(rng) * 0.6f,
              "sand_dune", 0.0f, rot(rng));

    for (int i = 0; i < 7; ++i)
        w.instance("cactus_" + std::to_string(i),
                   [&] { const auto* asset = pick_nature_asset(ctx, "nature_plant", "desert", i);
                          return asset ? resolve_nature_asset_id(*asset, ctx) : std::string("cactus_saguaro"); }(),
                   pos(rng), pos(rng), rot(rng));

    for (int i = 0; i < 12; ++i) {
        float rs = rsize(rng);
        w.box("rock_" + std::to_string(i),
              pos(rng), pos(rng), rs, rs * 0.7f, rs * 0.9f,
              "rock_sandstone", 0.0f, rot(rng));
    }

    for (int i = 0; i < 5; ++i)
        w.instance("scrub_" + std::to_string(i),
                   [&] { const auto* asset = pick_nature_asset(ctx, "nature_plant", "desert", i + 7);
                          return asset ? resolve_nature_asset_id(*asset, ctx) : std::string("plant_desert_scrub"); }(),
                   pos(rng), pos(rng), rot(rng));

    // MAP17 -- extra scrub near a mapped river (an oasis-adjacent fringe),
    // mirroring ForestGenerator's own M160 river-proximity pattern. Purely
    // additive after every pre-MAP17 draw above.
    if (ctx.map_context.available && ctx.map_context.nearest_river_distance_m >= 0.0f
        && ctx.map_context.nearest_river_distance_m < 150.0f) {
        for (int i = 0; i < 3; ++i)
            w.instance("oasis_scrub_" + std::to_string(i), "plant_desert_scrub",
                       pos(rng), pos(rng), rot(rng));
    }

    return w.build();
}

// MAP20, M320 -- ModelPlacements for this chunk's cacti + desert scrub,
// mirroring ForestGenerator's M168 pattern (own independent RNG draw). Dunes
// and rocks (generate()'s w.box() calls above) are NOT converted -- they're
// raw MC3 primitives, no ObjectDefinitionLibrary definition_id exists for
// them (same "only instance() calls become placements" scope as
// JungleGenerator's own placements()).
std::vector<ModelPlacement> DesertGenerator::placements(const ChunkContext& ctx) const {
    std::vector<ModelPlacement> result;
    result.reserve(12);

    const float s = ctx.chunk_size_m;
    std::mt19937_64 rng(ctx.seed);
    std::uniform_real_distribution<float> pos(2.0f, s - 2.0f);
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

    for (int i = 0; i < 7; ++i)
        add("cactus_saguaro", pos(rng), pos(rng), rot(rng));

    for (int i = 0; i < 5; ++i)
        add("plant_desert_scrub", pos(rng), pos(rng), rot(rng));

    return result;
}

} // namespace MeshWorld
