// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#include "generators/RiverBankGenerator.hpp"
#include "MC3Writer.hpp"
#include "GenerationMetadata.hpp"
#include <random>

namespace MeshWorld {

std::string RiverBankGenerator::generate(const ChunkContext& ctx) {
    MC3Writer w(ctx);
    w.set_metadata_json(
        GenerationMetadata::from_chunk_context(ctx, "cpp.chunk.river_bank").to_json()
    );
    const float s = ctx.chunk_size_m;
    const float ww = s * 0.35f;  // water width
    const float ew = 3.0f;       // embankment wall width

    // Water, embankment, grass
    w.plane("water",      0,          -0.3f,    ww,       s,    "water",       -0.3f);
    w.plane("embank",     ww,          0,        ew,       s,    "stone_embank");
    w.plane("grass",      ww + ew,     0,        s - ww - ew, s, "grass_bank");

    // Railing along embankment edge (water side)
    w.box("railing", ww + 0.2f, s * 0.5f, 0.12f, 1.1f, s, "metal_railing");

    // Railing posts every 5m
    std::mt19937_64 rng(ctx.seed);
    for (int i = 0; i <= static_cast<int>(s / 5.0f); ++i)
        w.box("post_" + std::to_string(i), ww + 0.2f, i * 5.0f, 0.2f, 1.2f, 0.2f, "metal_railing");

    // Willow trees on grass side
    std::uniform_real_distribution<float> tx(ww + ew + 3.0f, s - 2.0f);
    std::uniform_real_distribution<float> tz(2.0f, s - 2.0f);
    std::uniform_real_distribution<float> rot(0.0f, 360.0f);
    for (int i = 0; i < 8; ++i)
        w.instance("willow_" + std::to_string(i), "tree_willow",
                   tx(rng), tz(rng), rot(rng));

    // T234 -- reeds along the waterline, just inside the water plane's own
    // edge nearest the embankment (the one strip of this layout that's
    // actually water, not the walled/paved embankment or grass beyond it).
    std::uniform_real_distribution<float> reed_x(ww * 0.55f, ww * 0.85f);
    std::uniform_real_distribution<float> reed_z(1.0f, s - 1.0f);
    for (int i = 0; i < 10; ++i)
        w.instance("reed_" + std::to_string(i), "plant_marsh_grass",
                   reed_x(rng), reed_z(rng), rot(rng));

    // T234 -- scattered stones on the grass side, a natural riverside
    // feature distinct from the embankment's own dressed stone wall.
    std::uniform_real_distribution<float> stone_x(ww + ew + 1.0f, s - 1.0f);
    std::uniform_real_distribution<float> stone_z(1.0f, s - 1.0f);
    static const char* kStoneDefs[] = {"rock_grey_small", "rock_mossy", "rock_pile_small"};
    std::uniform_int_distribution<int> stone_pick(0, 2);
    for (int i = 0; i < 6; ++i)
        w.instance("stone_" + std::to_string(i), kStoneDefs[stone_pick(rng)],
                   stone_x(rng), stone_z(rng), rot(rng));

    // Bench facing water
    w.instance("bench_0", "bench_park", ww + ew + 2.0f, s * 0.25f, 270.0f);
    w.instance("bench_1", "bench_park", ww + ew + 2.0f, s * 0.75f, 270.0f);

    // Lamp posts along embankment
    for (int i = 0; i < 4; ++i)
        w.cylinder("lamp_" + std::to_string(i),
                   ww + ew + 1.5f, 8.0f + i * 16.0f, 0.12f, 4.0f, "metal_lamp");

    // MAP17 -- extra willows at low elevation (a lush, near-sea-level bank
    // rather than a sparser highland stream bank). Purely additive after
    // every pre-MAP17 draw above.
    if (ctx.map_context.available && ctx.map_context.elevation_m < 100.0f) {
        for (int i = 0; i < 3; ++i)
            w.instance("lush_willow_" + std::to_string(i), "tree_willow",
                       tx(rng), tz(rng), rot(rng));
    }

    return w.build();
}

} // namespace MeshWorld
