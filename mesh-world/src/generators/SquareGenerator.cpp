// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#include "generators/SquareGenerator.hpp"
#include "MC3Writer.hpp"
#include "GenerationMetadata.hpp"
#include <random>

namespace MeshWorld {

std::string SquareGenerator::generate(const ChunkContext& ctx) {
    MC3Writer w(ctx);
    w.set_metadata_json(
        GenerationMetadata::from_chunk_context(ctx, "cpp.chunk.square").to_json()
    );
    const float s = ctx.chunk_size_m;
    const float c = s * 0.5f;

    w.ground("cobblestone_square");

    // Central fountain / monument
    w.cylinder("plinth",        c, c, 3.5f, 1.5f, "stone_granite");
    w.cylinder("fountain_ring", c, c, 3.0f, 1.0f, "stone_light", 1.5f);
    w.cylinder("water_bowl",    c, c, 2.5f, 0.4f, "water",       2.5f);
    w.cylinder("jet",           c, c, 0.08f, 2.0f,"water",       2.9f);

    // 4 corner lamp posts
    const float lr = 12.0f;
    w.cylinder("lamp_ne", c + lr, c - lr, 0.15f, 5.0f, "metal_lamp_ornate");
    w.cylinder("lamp_nw", c - lr, c - lr, 0.15f, 5.0f, "metal_lamp_ornate");
    w.cylinder("lamp_se", c + lr, c + lr, 0.15f, 5.0f, "metal_lamp_ornate");
    w.cylinder("lamp_sw", c - lr, c + lr, 0.15f, 5.0f, "metal_lamp_ornate");

    // 8 benches around fountain
    const float br = 7.0f;
    const float angles[8] = {0,45,90,135,180,225,270,315};
    for (int i = 0; i < 8; ++i) {
        float rad = angles[i] * 3.14159f / 180.0f;
        float bx  = c + std::cos(rad) * br;
        float bz  = c + std::sin(rad) * br;
        w.instance("bench_" + std::to_string(i), "bench_stone", bx, bz, angles[i]);
    }

    // Paths from edges to fountain in cardinal directions
    const float pw = 4.0f;
    w.plane("path_n", c - pw*0.5f, 0,   pw, c - br - 1.0f, "cobblestone_path");
    w.plane("path_s", c - pw*0.5f, c + br + 1.0f, pw, c - br - 1.0f, "cobblestone_path");
    w.plane("path_w", 0,   c - pw*0.5f, c - br - 1.0f, pw, "cobblestone_path");
    w.plane("path_e", c + br + 1.0f, c - pw*0.5f, c - br - 1.0f, pw, "cobblestone_path");

    // Decorative trees at 4 outer corners
    w.instance("tree_ne", "tree_lime", c + 20.0f, c - 20.0f, 0.0f);
    w.instance("tree_nw", "tree_lime", c - 20.0f, c - 20.0f, 45.0f);
    w.instance("tree_se", "tree_lime", c + 20.0f, c + 20.0f, 90.0f);
    w.instance("tree_sw", "tree_lime", c - 20.0f, c + 20.0f, 135.0f);

    // MAP17 -- a monument plinth at the square's edge when it's the
    // named center of a real settlement, distinct from an ordinary
    // neighborhood square. Purely additive after every pre-MAP17 draw
    // above.
    if (ctx.map_context.available && !ctx.map_context.nearest_place_name.empty())
        w.cylinder("monument", c, c - br - 3.5f, 1.2f, 3.0f, "stone_granite");

    return w.build();
}

} // namespace MeshWorld
