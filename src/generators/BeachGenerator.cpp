// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#include "generators/BeachGenerator.hpp"
#include "MC3Writer.hpp"
#include "NatureAssets.hpp"
#include "GenerationMetadata.hpp"
#include "ObjectBoundingBox.hpp"
#include <random>

namespace MeshWorld {

std::string BeachGenerator::generate(const ChunkContext& ctx) {
    MC3Writer w(ctx);
    w.set_metadata_json(
        GenerationMetadata::from_chunk_context(ctx, "cpp.chunk.beach").to_json()
    );
    const float s  = ctx.chunk_size_m;
    const float ww = s * 0.4f;  // ocean water side width
    std::mt19937_64 rng(ctx.seed);
    std::uniform_real_distribution<float> pos(ww + 2.0f, s - 1.0f);
    std::uniform_real_distribution<float> along_shore(3.0f, s - 3.0f);
    std::uniform_real_distribution<float> rot(0.0f, 360.0f);
    // Ocean (western side)
    // MC3 terrain is at y=0. Keeping the water surface exactly there avoids
    // a validator-reported terrain penetration while still rendering as a
    // separate translucent plane above the beach ground.
    w.plane("ocean",    0,   0,        ww,      s, "water_ocean", 0.0f);
    // Wet sand transition strip
    w.plane("wet_sand", ww,   0,       4.0f,    s, "sand_wet");
    // Dry sand (main beach)
    w.ground("sand_beach");

    // Gentle wave lines (thin elevated planes)
    for (int i = 0; i < 3; ++i) {
        float wx = ww * (0.3f + i * 0.2f);
        w.plane("wave_" + std::to_string(i), wx, 0, 1.0f, s, "water_foam", 0.02f);
    }

    // Driftwood logs
    for (int i = 0; i < 3; ++i) {
        std::uniform_real_distribution<float> lpos(ww + 1.0f, ww + 12.0f);
        const float x = lpos(rng);
        const float z = along_shore(rng);
        const float ry = rot(rng);
        if (const auto* asset = pick_nature_asset(ctx, "nature_prop", "coast", i)) {
            w.instance("driftwood_" + std::to_string(i), resolve_nature_asset_id(*asset, ctx),
                       x, z, ry);
        } else {
            w.box("driftwood_" + std::to_string(i), x, z,
                  3.5f, 0.35f, 0.35f, "bark_driftwood", 0.0f, ry);
        }
    }

    // Shells and small rocks on beach
    for (int i = 0; i < 8; ++i) {
        std::uniform_real_distribution<float> sr_pos(ww + 0.5f, ww + 15.0f);
        w.instance("shell_" + std::to_string(i), "shell_seashell",
                   sr_pos(rng), pos(rng) * 0.8f + 1.0f, rot(rng));
    }

    // Sea grass near water edge
    for (int i = 0; i < 6; ++i) {
        std::uniform_real_distribution<float> sg_pos(ww + 1.0f, ww + 8.0f);
        w.instance("seagrass_" + std::to_string(i), "plant_sea_grass",
                   sg_pos(rng), pos(rng) * 0.7f + 1.0f, rot(rng));
    }

    // Palm tree or two further inland
    for (int i = 0; i < 2; ++i) {
        std::uniform_real_distribution<float> palm_x(ww + 20.0f, s - 4.0f);
        w.instance("palm_" + std::to_string(i), "tree_palm",
                   palm_x(rng), pos(rng), rot(rng));
    }

    // MAP17 -- extra driftwood and shells right at the waterline on a
    // near-sea-level beach (a wider tidal wrack zone than a beach sitting a
    // few metres above sea level would have). Purely additive, same
    // "no behavior change when unavailable" rule M160 established.
    if (ctx.map_context.available && ctx.map_context.elevation_m < 2.0f) {
        for (int i = 0; i < 3; ++i) {
            std::uniform_real_distribution<float> wrack_pos(ww + 0.5f, ww + 4.0f);
            w.instance("wrack_shell_" + std::to_string(i), "shell_seashell",
                       wrack_pos(rng), pos(rng) * 0.9f + 0.5f, rot(rng));
        }
    }

    return w.build();
}

// MAP20, M325 -- ModelPlacements for this chunk's shells, sea grass, and
// palm trees, mirroring ForestGenerator's M168 pattern (own independent RNG
// draw, reusing generate()'s own zoning -- shells/seagrass near the water
// line, palms further inland -- rather than its exact per-instance
// distribution objects). Driftwood (generate()'s w.box() calls above) is
// NOT converted -- a raw MC3 primitive, no ObjectDefinitionLibrary
// definition_id (same scope decision as the other MAP20 generators' own
// placements()).
std::vector<ModelPlacement> BeachGenerator::placements(const ChunkContext& ctx) const {
    std::vector<ModelPlacement> result;
    result.reserve(16);

    const float s  = ctx.chunk_size_m;
    const float ww = s * 0.4f;
    std::mt19937_64 rng(ctx.seed);
    std::uniform_real_distribution<float> waterline_x(ww + 0.5f, ww + 15.0f);
    std::uniform_real_distribution<float> inland_x(ww + 20.0f, s - 4.0f);
    std::uniform_real_distribution<float> along_z(1.0f, s - 1.0f);
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
        add("shell_seashell", waterline_x(rng), along_z(rng), rot(rng));

    for (int i = 0; i < 6; ++i)
        add("plant_sea_grass", waterline_x(rng), along_z(rng), rot(rng));

    for (int i = 0; i < 2; ++i)
        add("tree_palm", inland_x(rng), along_z(rng), rot(rng));

    return result;
}

} // namespace MeshWorld
