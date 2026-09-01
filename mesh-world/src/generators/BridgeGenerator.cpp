// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#include "generators/BridgeGenerator.hpp"
#include "MC3Writer.hpp"
#include "GenerationMetadata.hpp"

namespace MeshWorld {

std::string BridgeGenerator::generate(const ChunkContext& ctx) {
    MC3Writer w(ctx);
    w.set_metadata_json(
        GenerationMetadata::from_chunk_context(ctx, "cpp.chunk.bridge").to_json()
    );
    const float s   = ctx.chunk_size_m;
    const float c   = s * 0.5f;
    const float bw  = 10.0f;  // bridge deck width
    const float deckY = 1.5f;

    // River below
    w.plane("river", 0, -0.4f, s, s, "water", -0.4f);
    // River bed
    w.plane("river_bed", 0, 0, s, s, "gravel_riverbed");

    // Bridge deck (NS orientation by default; road exits determine real direction)
    const bool ew = ctx.exits.east_road || ctx.exits.west_road;
    if (ew) {
        // EW bridge
        w.plane("deck",    0, c - bw*0.5f, s, bw, "stone_bridge", deckY);
        // Arch support pillars
        w.box("arch_w", c * 0.5f, c, 4.0f, deckY, bw, "stone_arch");
        w.box("arch_e", c * 1.5f, c, 4.0f, deckY, bw, "stone_arch");
        // Railings
        w.box("rail_n", c, c - bw*0.5f + 0.15f, s, 1.0f, 0.3f, "stone_railing", deckY);
        w.box("rail_s", c, c + bw*0.5f - 0.15f, s, 1.0f, 0.3f, "stone_railing", deckY);
    } else {
        // NS bridge (default)
        w.plane("deck",    c - bw*0.5f, 0, bw, s, "stone_bridge", deckY);
        w.box("arch_n", c, c * 0.5f,  bw, deckY, 4.0f, "stone_arch");
        w.box("arch_s", c, c * 1.5f,  bw, deckY, 4.0f, "stone_arch");
        w.box("rail_w", c - bw*0.5f + 0.15f, c, 0.3f, 1.0f, s, "stone_railing", deckY);
        w.box("rail_e", c + bw*0.5f - 0.15f, c, 0.3f, 1.0f, s, "stone_railing", deckY);
    }

    // Corner lamp posts on bridge
    w.cylinder("lamp_0", c - bw*0.5f - 0.5f, 4.0f,  0.15f, 4.5f, "metal_lamp", deckY);
    w.cylinder("lamp_1", c + bw*0.5f + 0.5f, 4.0f,  0.15f, 4.5f, "metal_lamp", deckY);
    w.cylinder("lamp_2", c - bw*0.5f - 0.5f, s-4.0f, 0.15f, 4.5f, "metal_lamp", deckY);
    w.cylinder("lamp_3", c + bw*0.5f + 0.5f, s-4.0f, 0.15f, 4.5f, "metal_lamp", deckY);

    // MAP17 -- an extra mid-span support pier at high elevation (a
    // mountain-gorge crossing needs more structural support than a lowland
    // river crossing). Purely additive after every pre-MAP17 draw above.
    if (ctx.map_context.available && ctx.map_context.elevation_m > 500.0f) {
        if (ew) w.box("arch_mid", c, c, 4.0f, deckY, bw, "stone_arch");
        else    w.box("arch_mid", c, c, bw, deckY, 4.0f, "stone_arch");
    }

    return w.build();
}

} // namespace MeshWorld
