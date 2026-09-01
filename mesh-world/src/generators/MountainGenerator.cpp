// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#include "generators/MountainGenerator.hpp"
#include "MC3Writer.hpp"
#include "NatureAssets.hpp"
#include "GenerationMetadata.hpp"
#include "ObjectBoundingBox.hpp"
#include <algorithm>
#include <random>

namespace MeshWorld {

std::string MountainGenerator::generate(const ChunkContext& ctx) {
    MC3Writer w(ctx);
    w.set_metadata_json(
        GenerationMetadata::from_chunk_context(ctx, "cpp.chunk.mountain").to_json()
    );
    const float s = ctx.chunk_size_m;
    std::mt19937_64 rng(ctx.seed);
    std::uniform_real_distribution<float> pos(2.0f, s - 2.0f);
    std::uniform_real_distribution<float> rot(0.0f, 360.0f);
    std::uniform_real_distribution<float> bsize(1.0f, 4.5f);

    w.ground("rock_grey");

    // Cliff face along one side (deterministic from seed)
    std::uniform_int_distribution<int> cliff_side(0, 3);
    int cs = cliff_side(rng);

    // M160 — taller cliffs where the map layer reports higher elevation above
    // sea level. Falls back to the fixed 12 m when no map layer is attached
    // (ctx.map_context.available == false), matching pre-M160 behavior exactly.
    float ch = 12.0f;
    if (ctx.map_context.available) {
        const float above_sea = std::max(ctx.map_context.elevation_m, 0.0f);
        ch = std::clamp(12.0f + above_sea * 0.005f, 8.0f, 20.0f);
    }
    if      (cs == 0) w.box("cliff", s*0.5f, 3.0f,  s, ch, 6.0f, "rock_cliff");
    else if (cs == 1) w.box("cliff", s*0.5f, s-3.0f, s, ch, 6.0f, "rock_cliff");
    else if (cs == 2) w.box("cliff", 3.0f,  s*0.5f, 6.0f, ch, s, "rock_cliff");
    else              w.box("cliff", s-3.0f, s*0.5f, 6.0f, ch, s, "rock_cliff");

    // Boulders scattered — a rounded rock reads more naturally as a sphere
    // than a rotated box; also gives MC3Writer::sphere() its first real
    // caller (previously an unused wrapper, NEXT.md §5 #7). No rotation
    // parameter needed (spheres are rotationally symmetric), so `rot` is no
    // longer drawn here -- the rockpile/pine instance loops below still
    // draw their own `rot(rng)` calls independently, just starting one step
    // earlier in the shared RNG stream than before this change.
    for (int i = 0; i < 15; ++i) {
        float bs = bsize(rng);
        w.sphere("boulder_" + std::to_string(i),
                 pos(rng), pos(rng), bs * 0.5f, "rock_grey");
    }

    // Small rock piles (instances)
    for (int i = 0; i < 8; ++i)
        w.instance("rockpile_" + std::to_string(i),
                   [&] { const auto* asset = pick_nature_asset(ctx, "nature_rock", "mountain", i);
                          return asset ? resolve_nature_asset_id(*asset, ctx) : std::string("rock_pile_small"); }(),
                   pos(rng), pos(rng), rot(rng));

    // Sparse mountain pine trees
    for (int i = 0; i < 5; ++i)
        w.instance("pine_" + std::to_string(i), "tree_pine_mountain",
                   pos(rng), pos(rng), rot(rng));

    // Snow patches near cliff base
    for (int i = 0; i < 4; ++i)
        w.plane("snow_" + std::to_string(i),
                pos(rng) - 2.0f, pos(rng) - 1.5f, 4.0f, 3.0f, "snow", 0.05f);

    return w.build();
}

// MAP20, M321 -- ModelPlacements for this chunk's small rock-pile
// formations + sparse pine trees, mirroring ForestGenerator's M168 pattern
// (own independent RNG draw). Boulders/cliff face (generate()'s w.box()
// calls above) are NOT converted -- raw MC3 primitives, no
// ObjectDefinitionLibrary definition_id (same scope decision as
// JungleGenerator's/DesertGenerator's own placements()).
std::vector<ModelPlacement> MountainGenerator::placements(const ChunkContext& ctx) const {
    std::vector<ModelPlacement> result;
    result.reserve(13);

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

    for (int i = 0; i < 8; ++i)
        add("rock_pile_small", pos(rng), pos(rng), rot(rng));

    for (int i = 0; i < 5; ++i)
        add("tree_pine_mountain", pos(rng), pos(rng), rot(rng));

    return result;
}

} // namespace MeshWorld
