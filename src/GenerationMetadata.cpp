// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "GenerationMetadata.hpp"
#include "ZoneType.hpp"
#include "RegionType.hpp"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace MeshWorld {

namespace {

std::string current_utc_iso8601() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    gmtime_r(&t, &tm_buf);
    std::ostringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

// Minimal JSON string escaping (no Unicode escaping needed for our values).
std::string json_str(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (char c : s) {
        if (c == '"')  { out += "\\\""; }
        else if (c == '\\') { out += "\\\\"; }
        else if (c == '\n')  { out += "\\n"; }
        else { out += c; }
    }
    out += '"';
    return out;
}

} // namespace

std::string GenerationMetadata::to_json() const {
    std::ostringstream j;
    j << "{"
      << "\n  \"generator\": {"
      << "\n    \"id\": "       << json_str(generator_id)       << ","
      << "\n    \"version\": "  << json_str(generator_version)  << ","
      << "\n    \"language\": " << json_str(generator_language) << ","
      << "\n    \"category\": " << json_str(category)
      << "\n  },"
      << "\n  \"chunk\": {"
      << "\n    \"x\": "          << chunk_x << ","
      << "\n    \"y\": "          << chunk_y << ","
      << "\n    \"size_m\": "     << chunk_size_m
      << "\n  },"
      << "\n  \"generation\": {"
      << "\n    \"variationInput\": " << variation_input << ","
      << "\n    \"style\": "          << json_str(style)  << ","
      << "\n    \"zone\": "           << json_str(zone)   << ","
      << "\n    \"region\": "         << json_str(region) << ","
      << "\n    \"meshworldVersion\": " << json_str(meshworld_version) << ","
      << "\n    \"generatedAt\": "     << json_str(generated_at) << ","
      << "\n    \"aiAssisted\": "      << (ai_assisted ? "true" : "false") << ","
      << "\n    \"notes\": \"Variation input is not a long-term compatibility guarantee.\""
      << "\n  }"
      << "\n}";
    return j.str();
}

GenerationMetadata GenerationMetadata::from_chunk_context(const ChunkContext& ctx,
                                                           const std::string& generator_id,
                                                           const std::string& category) {
    GenerationMetadata m;
    m.generator_id    = generator_id;
    m.category        = category;
    m.variation_input = ctx.seed;
    m.style           = ctx.style;
    // R129 (zone-metadata bug fix) -- reports ctx.authored_zone (the flat
    // WorldMap/WorldConfig-derived zone), NOT ctx.zone, which the M157
    // map-layer override may have replaced with an unrelated planet's own
    // sampled biome (see ChunkContext::authored_zone's own doc comment).
    // ctx.region is unaffected: no equivalent "authored" ambiguity exists
    // for it in this bug -- only the zone field was ever reported wrong.
    m.zone            = to_string(ctx.authored_zone);
    m.region          = to_string(ctx.region);
    m.chunk_x         = ctx.coord.x;
    m.chunk_y         = ctx.coord.y;
    m.chunk_size_m    = ctx.chunk_size_m;
    m.generated_at    = current_utc_iso8601();
    return m;
}

} // namespace MeshWorld
