// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <string>
#include <vector>
#include "RegionType.hpp"
#include "ZoneType.hpp"

namespace MeshWorld {

// R108 -- per-chunk generator/asset-selection diagnostics. Populated by
// ChunkPipeline::get(x, y, ChunkDiagnostics*) so a debug HUD/chunk
// inspector (or a test) can see exactly which generator/composer produced
// a chunk's content, why a fallback happened (never silent -- source ==
// "cpp_fallback" always comes with a non-empty fallback_reason), and basic
// object/triangle/material/light counts + validation findings, without
// re-parsing stderr logs.
struct ChunkDiagnostics {
    // Where this chunk's content actually came from this call.
    // Composer (R113, docs/world-composer-design.md §8/§11): the C++
    // world composer (BuildingComposer), tried first, before Lua --
    // added as a real enum value rather than overloading CppFallback with
    // a distinguishing generator_id prefix, since diagnostics/tests should
    // be able to tell "the new composer path won" apart from "the old
    // C++ fallback chain won" without string-parsing generator_id.
    enum class Source { Lua, CppFallback, Cache, Composer };

    Source      source{Source::CppFallback};

    // Id read back out of the generated <metadata> JSON (e.g.
    // "lua.zone.park" or "cpp.chunk.park") -- the authoritative record of
    // which generator produced the content, independent of `source`
    // (a cache hit still reports the id the content was ORIGINALLY built
    // with). Empty only if metadata was missing/unparsable (itself a
    // validation error, see validation_errors below).
    std::string generator_id;

    // Non-empty only when source == CppFallback: why the Lua attempt
    // (if any was even registered) didn't win. Empty for Lua/Cache.
    std::string fallback_reason;

    // Selection context this chunk was generated/looked-up under.
    ZoneType    zone{ZoneType::empty};
    RegionType  region{RegionType::empty};
    std::string style;
    int         lod{0};
    bool        map_context_available{false};

    // Best-effort content stats (see ValidationResult's own doc comment on
    // the same fields for exactly what's counted/not counted).
    int                      object_count{0};
    int                      material_count{0};
    std::vector<std::string> materials_used;
    int                      light_count{0};
    int                      triangle_count{0};

    // Carried over verbatim from MC3Validator::validate() -- never
    // dropped, so a fallback (or even a Lua success with warnings) is
    // always inspectable, not just logged to stderr.
    std::vector<std::string> validation_errors;
    std::vector<std::string> validation_warnings;
};

// Human-readable Source, for logs/debug HUD text.
inline const char* to_string(ChunkDiagnostics::Source s) {
    switch (s) {
        case ChunkDiagnostics::Source::Lua:         return "lua";
        case ChunkDiagnostics::Source::CppFallback: return "cpp_fallback";
        case ChunkDiagnostics::Source::Cache:       return "cache";
        case ChunkDiagnostics::Source::Composer:    return "composer";
    }
    return "unknown";
}

} // namespace MeshWorld
