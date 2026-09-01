// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#include "generators/RoadGenerator.hpp"
#include "MC3Writer.hpp"
#include "GenerationMetadata.hpp"
#include "StyleRegistry.hpp"
#include <random>

namespace MeshWorld {

std::string RoadGenerator::generate(const ChunkContext& ctx) {
    MC3Writer w(ctx);
    w.set_metadata_json(
        GenerationMetadata::from_chunk_context(ctx, "cpp.chunk.road").to_json()
    );

    const Style* style_ptr = StyleRegistry::instance().has(ctx.style)
                             ? &StyleRegistry::instance().get(ctx.style)
                             : nullptr;
    auto mat = [&](const std::string& key, const char* fallback) -> std::string {
        if (style_ptr) return style_ptr->mat(key, fallback);
        return fallback;
    };

    const float s   = ctx.chunk_size_m;
    const float c   = s * 0.5f;
    const float rw  = 8.0f;   // road width (carriageway)
    const float sw  = 2.5f;   // sidewalk width
    const float cw  = 0.25f;  // curb width
    const float ch  = 0.15f;  // curb height
    const float dlw = 0.25f;  // dash (centre-line marking) width
    const float dll = 2.5f;   // dash length
    const float dlg = 2.5f;   // dash gap

    const bool ns = ctx.exits.north_road || ctx.exits.south_road;
    const bool ew = ctx.exits.east_road  || ctx.exits.west_road;

    std::mt19937_64 rng(ctx.seed);
    std::uniform_real_distribution<float> jitter(-1.0f, 1.0f);

    const auto road_ground   = mat("road.ground",   "grass_strip");
    const auto road_surface  = mat("road.surface",  "asphalt");
    const auto road_sidewalk = mat("road.sidewalk", "pavement_slab");
    const auto road_curb     = mat("road.curb",     "stone_curb");
    const auto road_marking  = mat("road.marking",  "road_marking_white");
    const auto road_lamp     = mat("road.lamp",     "metal_lamp");
    const auto road_drain    = mat("road.drain",    "metal_grate");

    if (ctx.exits.north_road && ctx.exits.south_road && !ew) {
        // ── North-South road ──────────────────────────────────────────
        w.ground(road_ground);
        w.plane("road",       c - rw * 0.5f,      0.0f, rw, s, road_surface);
        w.plane("sidewalk_w", c - rw * 0.5f - sw, 0.0f, sw, s, road_sidewalk);
        w.plane("sidewalk_e", c + rw * 0.5f,       0.0f, sw, s, road_sidewalk);

        // Raised curbs
        w.box("curb_w", c - rw * 0.5f - cw * 0.5f, c, cw, ch, s, road_curb);
        w.box("curb_e", c + rw * 0.5f + cw * 0.5f, c, cw, ch, s, road_curb);

        // Centre-line dashes
        for (float z = dlg; z + dll <= s - dlg; z += dll + dlg) {
            std::string id = "dash_" + std::to_string(static_cast<int>(z));
            w.plane(id, c - dlw * 0.5f, z, dlw, dll, road_marking);
        }

        // Lamp posts with slight position jitter
        for (int i = 0; i < 4; ++i) {
            float lz = 6.0f + i * 13.0f + jitter(rng);
            w.cylinder("lamp_w" + std::to_string(i), c - rw * 0.5f - sw * 0.6f, lz, 0.12f, 4.5f, road_lamp);
            w.cylinder("lamp_e" + std::to_string(i), c + rw * 0.5f + sw * 0.6f, lz, 0.12f, 4.5f, road_lamp);
        }

        // Drain grates at kerb edges (flat, recessed)
        for (int i = 0; i < 2; ++i) {
            float gz = 14.0f + i * 36.0f;
            w.plane("drain_w" + std::to_string(i), c - rw * 0.5f - 0.5f, gz - 0.5f, 1.0f, 1.0f, road_drain);
            w.plane("drain_e" + std::to_string(i), c + rw * 0.5f - 0.5f, gz - 0.5f, 1.0f, 1.0f, road_drain);
        }

    } else if (ctx.exits.east_road && ctx.exits.west_road && !ns) {
        // ── East-West road ────────────────────────────────────────────
        w.ground(road_ground);
        w.plane("road",       0.0f, c - rw * 0.5f,      s, rw, road_surface);
        w.plane("sidewalk_n", 0.0f, c - rw * 0.5f - sw, s, sw, road_sidewalk);
        w.plane("sidewalk_s", 0.0f, c + rw * 0.5f,       s, sw, road_sidewalk);

        // Raised curbs
        w.box("curb_n", c, c - rw * 0.5f - cw * 0.5f, s, ch, cw, road_curb);
        w.box("curb_s", c, c + rw * 0.5f + cw * 0.5f, s, ch, cw, road_curb);

        // Centre-line dashes
        for (float x = dlg; x + dll <= s - dlg; x += dll + dlg) {
            std::string id = "dash_" + std::to_string(static_cast<int>(x));
            w.plane(id, x, c - dlw * 0.5f, dll, dlw, road_marking);
        }

        // Lamp posts
        for (int i = 0; i < 4; ++i) {
            float lx = 6.0f + i * 13.0f + jitter(rng);
            w.cylinder("lamp_n" + std::to_string(i), lx, c - rw * 0.5f - sw * 0.6f, 0.12f, 4.5f, road_lamp);
            w.cylinder("lamp_s" + std::to_string(i), lx, c + rw * 0.5f + sw * 0.6f, 0.12f, 4.5f, road_lamp);
        }

        // Drain grates
        for (int i = 0; i < 2; ++i) {
            float gx = 14.0f + i * 36.0f;
            w.plane("drain_n" + std::to_string(i), gx - 0.5f, c - rw * 0.5f - 0.5f, 1.0f, 1.0f, road_drain);
            w.plane("drain_s" + std::to_string(i), gx - 0.5f, c + rw * 0.5f - 0.5f, 1.0f, 1.0f, road_drain);
        }

    } else {
        // R134 -- a road that turns or terminates must draw only arms backed
        // by the canonical symmetric edge graph. The old 16m patch looked
        // harmless in isolation but painted a road toward every missing
        // neighbour, which is exactly the visual "road to nowhere" defect.
        w.ground(road_ground);
        const float half_arm = c - rw * 0.5f;
        w.plane("road_center", c - rw * 0.5f, c - rw * 0.5f, rw, rw, road_surface);
        if (ctx.exits.north_road) {
            w.plane("road_n", c - rw * 0.5f, 0.0f, rw, half_arm, road_surface);
            w.plane("sidewalk_nw", c - rw * 0.5f - sw, 0.0f, sw, half_arm, road_sidewalk);
            w.plane("sidewalk_ne", c + rw * 0.5f, 0.0f, sw, half_arm, road_sidewalk);
        }
        if (ctx.exits.south_road) {
            w.plane("road_s", c - rw * 0.5f, c + rw * 0.5f, rw, half_arm, road_surface);
            w.plane("sidewalk_sw", c - rw * 0.5f - sw, c + rw * 0.5f, sw, half_arm, road_sidewalk);
            w.plane("sidewalk_se", c + rw * 0.5f, c + rw * 0.5f, sw, half_arm, road_sidewalk);
        }
        if (ctx.exits.east_road) {
            w.plane("road_e", c + rw * 0.5f, c - rw * 0.5f, half_arm, rw, road_surface);
            w.plane("sidewalk_en", c + rw * 0.5f, c - rw * 0.5f - sw, half_arm, sw, road_sidewalk);
            w.plane("sidewalk_es", c + rw * 0.5f, c + rw * 0.5f, half_arm, sw, road_sidewalk);
        }
        if (ctx.exits.west_road) {
            w.plane("road_w", 0.0f, c - rw * 0.5f, half_arm, rw, road_surface);
            w.plane("sidewalk_wn", 0.0f, c - rw * 0.5f - sw, half_arm, sw, road_sidewalk);
            w.plane("sidewalk_ws", 0.0f, c + rw * 0.5f, half_arm, sw, road_sidewalk);
        }
    }

    // MAP17 -- guardrail bollards along both curbs at high elevation (a
    // mountain road needs more edge safety than a lowland street), applies
    // regardless of orientation/dead-end above. Purely additive after every
    // pre-MAP17 draw above.
    if (ctx.map_context.available && ctx.map_context.elevation_m > 500.0f) {
        for (int i = 0; i < 4; ++i) {
            float t = 6.0f + i * 13.0f;
            w.cylinder("guardrail_a" + std::to_string(i), t, c - rw * 0.5f - cw, 0.1f, 0.9f, road_curb);
            w.cylinder("guardrail_b" + std::to_string(i), t, c + rw * 0.5f + cw, 0.1f, 0.9f, road_curb);
        }
    }

    return w.build();
}

} // namespace MeshWorld
