// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#include "generators/ApartmentBlockGenerator.hpp"
#include "MC3Writer.hpp"
#include "GenerationMetadata.hpp"
#include <random>

namespace MeshWorld {

std::string ApartmentBlockGenerator::generate(const ChunkContext& ctx) {
    MC3Writer w(ctx);
    w.set_metadata_json(
        GenerationMetadata::from_chunk_context(ctx, "cpp.chunk.apartment_block").to_json()
    );
    const float s = ctx.chunk_size_m;

    w.ground("concrete_pavement");
    // Inner courtyard grass
    w.plane("courtyard", 16.0f, 20.0f, s - 32.0f, s - 40.0f, "grass_courtyard");

    std::mt19937_64 rng(ctx.seed);
    // T233 -- floor count is now a real discrete property (5-7 floors,
    // 3.2 m each -- an ordinary residential storey height), not a
    // continuous height jitter with no "floor" concept behind it at all.
    // The resulting height range (16-22.4 m) matches this block's own
    // pre-existing 16-22 m range closely enough that every other
    // dimension/placement below still reads the same way.
    constexpr float kFloorHeightM = 3.2f;
    std::uniform_int_distribution<int> floor_count_dist(5, 7);
    const int floor_count = floor_count_dist(rng);

    const char* facades[3] = {"concrete_panel", "brick_dark", "plaster_grey"};
    std::uniform_int_distribution<int> fmat(0, 2);

    float fh = static_cast<float>(floor_count) * kFloorHeightM;
    std::string fm = facades[fmat(rng)];

    // North block
    w.box("block_n", s*0.5f, 10.0f, s - 10.0f, fh, 14.0f, fm);
    // South block
    w.box("block_s", s*0.5f, s - 10.0f, s - 10.0f, fh, 14.0f, fm);
    // Connecting side wings (narrower)
    w.box("wing_w", 4.0f,    s*0.5f, 5.0f, fh - 4.0f, s - 20.0f, fm);
    w.box("wing_e", s - 4.0f, s*0.5f, 5.0f, fh - 4.0f, s - 20.0f, fm);

    // Entrance canopy
    w.box("canopy_n", s*0.5f, 5.0f, 8.0f, 0.3f, 3.0f, "concrete_slab", 3.5f);
    w.box("canopy_s", s*0.5f, s - 5.0f, 8.0f, 0.3f, 3.0f, "concrete_slab", 3.5f);

    // Courtyard lamp posts
    std::uniform_real_distribution<float> lx(18.0f, s - 18.0f);
    std::uniform_real_distribution<float> lz(22.0f, s - 22.0f);
    for (int i = 0; i < 4; ++i)
        w.cylinder("lamp_" + std::to_string(i), lx(rng), lz(rng), 0.12f, 3.5f, "metal_lamp");

    // MAP17 -- extra bike racks at the entrance canopies when this block sits
    // on a mapped road crossing (more foot/bike traffic than an interior
    // block). Purely additive after every pre-MAP17 draw above, so the base
    // rng sequence is identical whether or not a map layer is attached (same
    // "no behavior change when unavailable" rule M160 established).
    if (ctx.map_context.available && ctx.map_context.has_road_crossing) {
        w.box("bikerack_n", s*0.5f - 6.0f, 6.5f, 3.0f, 0.6f, 0.4f, "metal_lamp");
        w.box("bikerack_s", s*0.5f - 6.0f, s - 6.5f, 3.0f, 0.6f, 0.4f, "metal_lamp");
    }

    return w.build();
}

} // namespace MeshWorld
