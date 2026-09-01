// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <string>
#include <cstdint>
#include "ChunkGenerator.hpp"

namespace MeshWorld {

struct GenerationMetadata {
    std::string generator_id;        // e.g. "cpp.chunk.park" or "lua.object.chair.simple"
    std::string generator_version{"0.1.0"};
    std::string generator_language{"cpp"};  // "cpp" or "lua"
    std::string category;            // "chunk", "object", "zone", "building", "map"

    uint64_t    variation_input{0};  // ChunkContext::seed — NOT a reproducibility guarantee
    std::string style;
    std::string zone;
    std::string region;
    std::string meshworld_version{"0.1"};
    std::string generated_at;        // ISO 8601 UTC; filled by from_chunk_context()

    int         chunk_x{0};
    int         chunk_y{0};
    float       chunk_size_m{64.0f};
    bool        ai_assisted{false};  // true only if a Claude API call modified the output

    // Serialize to compact JSON string.
    std::string to_json() const;

    // Build from a ChunkContext, automatically filling timestamps and coord fields.
    static GenerationMetadata from_chunk_context(const ChunkContext& ctx,
                                                 const std::string& generator_id,
                                                 const std::string& category = "chunk");
};

} // namespace MeshWorld
