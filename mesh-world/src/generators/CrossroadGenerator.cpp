// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#include "generators/CrossroadGenerator.hpp"
#include "MC3Writer.hpp"
#include "GenerationMetadata.hpp"

namespace MeshWorld {

std::string CrossroadGenerator::generate(const ChunkContext& ctx) {
    MC3Writer w(ctx);
    w.set_metadata_json(
        GenerationMetadata::from_chunk_context(ctx, "cpp.chunk.crossroad").to_json()
    );
    const float s  = ctx.chunk_size_m;
    const float c  = s * 0.5f;
    const float rw = 8.0f;
    const float sw = 2.0f;

    w.ground("grass_strip");

    // T230 -- auto-detect 3-way (T-junction) vs 4-way from ctx.exits:
    // the N-S/E-W strips used to always span the WHOLE tile regardless of
    // whether a road actually continues in every direction. Decomposed into
    // a center junction square + 4 independent half-length arms, so a
    // missing direction's arm is simply omitted instead of drawing a road
    // toward an edge nothing connects to. R134 deliberately removes the old
    // ambiguous (0/1 exits) -> all-four fallback: the canonical edge graph is
    // now authoritative, and guessing produces the visible road-to-nowhere
    // defect this generator is meant to prevent.
    const bool has_n = ctx.exits.north_road;
    const bool has_s = ctx.exits.south_road;
    const bool has_e = ctx.exits.east_road;
    const bool has_w = ctx.exits.west_road;
    const float half_arm = c - rw * 0.5f;
    if (has_n) w.plane("road_n", c - rw*0.5f, 0,             rw,       half_arm, "asphalt");
    if (has_s) w.plane("road_s", c - rw*0.5f, c + rw*0.5f,   rw,       half_arm, "asphalt");
    if (has_e) w.plane("road_e", c + rw*0.5f, c - rw*0.5f,   half_arm, rw,       "asphalt");
    if (has_w) w.plane("road_w", 0,           c - rw*0.5f,   half_arm, rw,       "asphalt");
    // Central junction square where every arm meets.
    w.plane("road_center", c - rw*0.5f, c - rw*0.5f, rw, rw, "asphalt");

    // Sidewalks in 4 corners
    w.plane("sw_nw", c - rw*0.5f - sw, 0,          sw, c - rw*0.5f, "pavement");
    w.plane("sw_ne", c + rw*0.5f,       0,          sw, c - rw*0.5f, "pavement");
    w.plane("sw_sw", c - rw*0.5f - sw, c + rw*0.5f, sw, c - rw*0.5f, "pavement");
    w.plane("sw_se", c + rw*0.5f,       c + rw*0.5f, sw, c - rw*0.5f, "pavement");

    // Central road marking dot
    w.cylinder("center_mark", c, c, 1.0f, 0.05f, "paint_white", 0.01f);

    // Corner lamp posts
    w.cylinder("lamp_nw", c - rw*0.5f - 1.0f, c - rw*0.5f - 1.0f, 0.15f, 4.5f, "metal_lamp");
    w.cylinder("lamp_ne", c + rw*0.5f + 1.0f, c - rw*0.5f - 1.0f, 0.15f, 4.5f, "metal_lamp");
    w.cylinder("lamp_sw", c - rw*0.5f - 1.0f, c + rw*0.5f + 1.0f, 0.15f, 4.5f, "metal_lamp");
    w.cylinder("lamp_se", c + rw*0.5f + 1.0f, c + rw*0.5f + 1.0f, 0.15f, 4.5f, "metal_lamp");

    // R121/R138 -- north/south crosswalk stripes are only meaningful when
    // their matching canonical approach exists. (The legacy Lua layout only
    // has these two orientations, so do not invent east/west markings here.)
    {
        const float stripe_w  = rw;
        const float cw_stripe = 0.5f;
        const float cw_gap    = 0.5f;
        const float stripe_z0 = c + rw * 0.5f;       // south edge of junction
        const float stripe_z1 = c - rw * 0.5f - 4.0f; // north approach end

        if (has_s) {
            int si = 0;
            for (float cx = c - stripe_w * 0.5f; cx < c + stripe_w * 0.5f; cx += cw_stripe + cw_gap) {
                ++si;
                w.plane("cw_s_" + std::to_string(si), cx, stripe_z0 + 0.5f, cw_stripe, 3.5f, "road_line_white");
            }
        }
        if (has_n) {
            int si = 0;
            for (float cx = c - stripe_w * 0.5f; cx < c + stripe_w * 0.5f; cx += cw_stripe + cw_gap) {
                ++si;
                w.plane("cw_n_" + std::to_string(si), cx, stripe_z1 - 4.0f, cw_stripe, 3.5f, "road_line_white");
            }
        }
    }

    // R138 -- traffic lights at all 4 corners. MC3 actions/states are not
    // advanced by MeshWorldApp yet, so use a deterministic snapshot phase
    // per generated junction instead of falsely showing every lens as lit.
    // North/south and east/west approaches receive complementary states;
    // different chunks select green, amber, or red from their stable seed.
    {
        enum class Signal { Red, Amber, Green };
        const auto signal_for = [&](bool north_south) {
            switch (ctx.seed % 3U) {
                case 0: return north_south ? Signal::Green : Signal::Red;
                case 1: return north_south ? Signal::Amber : Signal::Red;
                default: return north_south ? Signal::Red : Signal::Green;
            }
        };
        const auto lens_material = [](Signal lit, Signal lens) -> const char* {
            if (lens == Signal::Red)   return lit == Signal::Red   ? "light_red"   : "light_red_dim";
            if (lens == Signal::Amber) return lit == Signal::Amber ? "light_amber" : "light_amber_dim";
            return lit == Signal::Green ? "light_green" : "light_green_dim";
        };
        const float corners[4][2] = {
            {c - rw * 0.5f - 1.0f, c - rw * 0.5f - 1.0f},
            {c + rw * 0.5f + 1.0f, c - rw * 0.5f - 1.0f},
            {c - rw * 0.5f - 1.0f, c + rw * 0.5f + 1.0f},
            {c + rw * 0.5f + 1.0f, c + rw * 0.5f + 1.0f},
        };
        for (int i = 0; i < 4; ++i) {
            const std::string idx = std::to_string(i + 1);
            const float px = corners[i][0];
            const float pz = corners[i][1];
            const Signal active = signal_for(i == 0 || i == 3);
            w.cylinder("tl_pole_" + idx, px, pz, 0.06f, 4.5f, "metal_dark");
            w.box("tl_head_" + idx, px, pz, 0.34f, 0.90f, 0.28f, "metal_dark", 4.35f);
            w.sphere("tl_red_"   + idx, px, pz, 0.12f, lens_material(active, Signal::Red),   4.95f);
            w.sphere("tl_amber_" + idx, px, pz, 0.12f, lens_material(active, Signal::Amber), 4.65f);
            w.sphere("tl_green_" + idx, px, pz, 0.12f, lens_material(active, Signal::Green), 4.35f);
        }
    }

    // MAP17 -- a signpost when a named place (city/country/region) is
    // nearby -- a real-feeling wayfinding cue a crossroad plausibly has.
    // No literal text glyph (this project has no font-rendering
    // dependency, same constraint --legend's own PNG key works around) --
    // just a marker instance at the signpost's usual corner spot. Purely
    // additive after every pre-MAP17 draw above.
    if (ctx.map_context.available && !ctx.map_context.nearest_place_name.empty())
        w.cylinder("signpost", c - rw*0.5f - 2.5f, c - rw*0.5f - 1.0f, 0.08f, 3.0f, "metal_lamp");

    return w.build();
}

} // namespace MeshWorld
