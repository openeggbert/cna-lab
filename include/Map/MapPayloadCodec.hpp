// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <string>

#include "Map/MapTilePayload.hpp"

namespace MeshWorld::Map {

// M023/M024 — serialize a MapTilePayload to/from JSON text. Keys are emitted in
// a stable (alphabetical) order via nlohmann's sorted-object dump, so the same
// payload always produces byte-identical text. This is the encoding MapTileStore
// persists in the `tile.payload` column (M020).
class MapPayloadCodec {
public:
    // Serialize to compact JSON text.
    static std::string encode(const MapTilePayload& payload);

    // Parse JSON text back into a payload. Throws nlohmann::json::exception on
    // malformed input or a missing/wrong-typed field.
    static MapTilePayload decode(const std::string& json_text);
};

} // namespace MeshWorld::Map
