// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#include "generators/OceanGenerator.hpp"
#include "MC3Writer.hpp"
#include "GenerationMetadata.hpp"
#include "ObjectBoundingBox.hpp"
#include <random>

namespace MeshWorld {

std::string OceanGenerator::generate(const ChunkContext& ctx) {
    MC3Writer w(ctx);
    w.set_metadata_json(
        GenerationMetadata::from_chunk_context(ctx, "cpp.chunk.ocean").to_json()
    );
    const float s = ctx.chunk_size_m;
    std::mt19937_64 rng(ctx.seed);
    std::uniform_real_distribution<float> pos(3.0f, s - 3.0f);
    std::uniform_real_distribution<float> rot(0.0f, 360.0f);
    std::uniform_real_distribution<float> rsize(0.8f, 3.0f);

    // Deep water surface
    w.ground("water_deep");
    // Slight wave variation layer
    w.plane("surface", 0, 0, s, s, "water_ocean", 0.05f);

    // Occasional rocky outcrop / small island. Draw the roll unconditionally so
    // downstream rng draws stay in the same sequence regardless of map_context.
    std::uniform_int_distribution<int> has_rock(0, 3);
    bool spawn_rock = has_rock(rng) == 0;

    // M160 — near-coast (shallow) water is more likely to break the surface;
    // deep ocean floor essentially never does. Falls back to the plain 1-in-4
    // roll when no map layer is attached (ctx.map_context.available == false).
    if (ctx.map_context.available) {
        if (ctx.map_context.elevation_m < -1000.0f)
            spawn_rock = false;
        else if (ctx.map_context.elevation_m > -50.0f)
            spawn_rock = true;
    }

    if (spawn_rock) {
        float rx = pos(rng), rz = pos(rng);
        float rs = rsize(rng);
        w.box("rock_0", rx, rz, rs*2.0f, rs, rs*1.5f, "rock_sea", 0.0f);
        // Seagull / sea plant on rock
        w.instance("seaplant_0", "plant_sea_weed", rx, rz, rot(rng), rs);
    }

    // Floating seaweed patches
    for (int i = 0; i < 5; ++i)
        w.plane("weed_" + std::to_string(i),
                pos(rng) - 1.5f, pos(rng) - 1.5f, 3.0f, 3.0f,
                "plant_sea_weed_float", 0.02f);

    // MAP20, M326 -- coral_reef/kelp_forest sub-areas (MAP16's new aquatic
    // biomes, M236-M280) now dispatch here instead of falling through to
    // EmptyGenerator (ChunkGenerator.cpp). Appended after every pre-M326
    // draw above, so ordinary ZoneType::ocean chunks (where ctx.zone is
    // never coral_reef/kelp_forest) keep their exact prior RNG sequence and
    // output -- this branch is unreachable for them.
    if (ctx.zone == ZoneType::coral_reef) {
        for (int i = 0; i < 10; ++i)
            w.instance("coral_" + std::to_string(i), "coral_branching",
                       pos(rng), pos(rng), rot(rng));
    } else if (ctx.zone == ZoneType::kelp_forest) {
        for (int i = 0; i < 14; ++i)
            w.instance("kelp_" + std::to_string(i), "kelp_strand",
                       pos(rng), pos(rng), rot(rng));
    }

    return w.build();
}

// MAP20, M326 -- ModelPlacements for this chunk's conditional sea-weed
// (mirrors generate()'s own spawn_rock roll) plus, for coral_reef/
// kelp_forest sub-areas, the new coral/kelp instances above. Mirrors
// ForestGenerator's M168 pattern (own independent RNG draw). The rock
// itself (generate()'s w.box() call) is not converted -- a raw MC3
// primitive, no ObjectDefinitionLibrary definition_id (same scope decision
// as the other MAP20 generators' own placements()).
std::vector<ModelPlacement> OceanGenerator::placements(const ChunkContext& ctx) const {
    std::vector<ModelPlacement> result;

    const float s = ctx.chunk_size_m;
    std::mt19937_64 rng(ctx.seed);
    std::uniform_real_distribution<float> pos(3.0f, s - 3.0f);
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

    std::uniform_int_distribution<int> has_rock(0, 3);
    bool spawn_rock = has_rock(rng) == 0;
    if (ctx.map_context.available) {
        if (ctx.map_context.elevation_m < -1000.0f)
            spawn_rock = false;
        else if (ctx.map_context.elevation_m > -50.0f)
            spawn_rock = true;
    }
    if (spawn_rock) {
        const float rx = pos(rng), rz = pos(rng);
        const float ry = rot(rng);
        add("plant_sea_weed", rx, rz, ry);
    }

    if (ctx.zone == ZoneType::coral_reef) {
        for (int i = 0; i < 10; ++i)
            add("coral_branching", pos(rng), pos(rng), rot(rng));
    } else if (ctx.zone == ZoneType::kelp_forest) {
        for (int i = 0; i < 14; ++i)
            add("kelp_strand", pos(rng), pos(rng), rot(rng));
    }

    return result;
}

} // namespace MeshWorld
