// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#include "generators/ShopStreetGenerator.hpp"
#include "MC3Writer.hpp"
#include "GenerationMetadata.hpp"
#include <random>

namespace MeshWorld {

std::string ShopStreetGenerator::generate(const ChunkContext& ctx) {
    MC3Writer w(ctx);
    w.set_metadata_json(
        GenerationMetadata::from_chunk_context(ctx, "cpp.chunk.shop_street").to_json()
    );
    const float s = ctx.chunk_size_m;

    w.ground("cobblestone");
    // Central pedestrian zone (slightly different surface)
    w.plane("ped_zone", 0, s*0.5f - 10.0f, s, 20.0f, "cobblestone_light");

    std::mt19937_64 rng(ctx.seed);
    const char* shopfronts[4] = {
        "plaster_blue", "plaster_green", "brick_red", "plaster_yellow"
    };
    std::uniform_int_distribution<int> smat(0, 3);
    std::uniform_real_distribution<float> sh(3.5f, 5.5f);
    std::uniform_real_distribution<float> aw_col_val(0.0f, 1.0f);

    // North row: 4 shops
    const float shop_w = (s - 4.0f) / 4.0f;
    for (int i = 0; i < 4; ++i) {
        float cx = 2.0f + shop_w * 0.5f + i * shop_w;
        float fh  = sh(rng);
        std::string idx = std::to_string(i);
        w.box("shop_n_" + idx, cx, 5.5f, shop_w - 0.5f, fh, 9.0f, shopfronts[smat(rng)]);
        // Awning plane
        w.plane("awning_n_" + idx, 2.0f + i * shop_w, 9.0f, shop_w - 0.5f, 2.5f,
                "awning_stripe", 2.8f);
    }
    // South row: 4 shops (mirrored)
    for (int i = 0; i < 4; ++i) {
        float cx = 2.0f + shop_w * 0.5f + i * shop_w;
        float fh  = sh(rng);
        std::string idx = std::to_string(i);
        w.box("shop_s_" + idx, cx, s - 5.5f, shop_w - 0.5f, fh, 9.0f, shopfronts[smat(rng)]);
        w.plane("awning_s_" + idx, 2.0f + i * shop_w, s - 11.5f, shop_w - 0.5f, 2.5f,
                "awning_stripe", 2.8f);
    }

    // Lamp posts and benches in pedestrian zone
    for (int i = 0; i < 3; ++i) {
        float lx = 8.0f + i * 24.0f;
        w.cylinder("lamp_" + std::to_string(i), lx, s*0.5f, 0.12f, 4.0f, "metal_lamp");
        w.instance("bench_" + std::to_string(i), "bench_street", lx, s*0.5f + 4.0f);
    }

    // MAP17 -- an extra bench cluster near a mapped named place (a shop
    // street next to a real settlement draws more foot traffic than one on
    // the outskirts). Purely additive after every pre-MAP17 draw above.
    if (ctx.map_context.available && !ctx.map_context.nearest_place_name.empty()) {
        w.instance("bench_place_0", "bench_street", s*0.5f - 6.0f, s*0.5f - 4.0f, 180.0f);
        w.instance("bench_place_1", "bench_street", s*0.5f + 6.0f, s*0.5f - 4.0f, 180.0f);
    }

    return w.build();
}

} // namespace MeshWorld
