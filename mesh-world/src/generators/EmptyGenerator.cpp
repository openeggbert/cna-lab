// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#include "generators/EmptyGenerator.hpp"
#include "MC3Writer.hpp"
#include "GenerationMetadata.hpp"

namespace MeshWorld {
std::string EmptyGenerator::generate(const ChunkContext& ctx) {
    MC3Writer w(ctx);
    w.set_metadata_json(
        GenerationMetadata::from_chunk_context(ctx, "cpp.chunk.empty").to_json()
    );
    // MAP17 -- this is the fallback for any zone with no dedicated
    // generator (per MAP16's M280 audit, that's currently all 40 new
    // ZoneType values too), so an elevation-aware ground texture is a real,
    // broadly-visible improvement over a single flat "dirt" everywhere:
    // below sea level reads as wet mud, high elevation as bare frozen rock,
    // in between stays plain dirt (unchanged when map_context isn't
    // available, same "no behavior change when unavailable" rule M160
    // established).
    const char* ground = "dirt";
    if (ctx.map_context.available) {
        if (ctx.map_context.elevation_m < 0.0f) ground = "swamp_mud";
        else if (ctx.map_context.elevation_m > 1500.0f) ground = "rock_snow_covered";
    }
    w.ground(ground);
    return w.build();
}
} // namespace MeshWorld
