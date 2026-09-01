// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <string>

namespace MeshWorld::Map {

// M156 (MAP10) — a Map::-native "zone candidate" hint, one per raster
// cell (see MapTilePayload::zone_candidates), one level more specific than
// ZoneType::city within an urbanized block. Deliberately NOT the legacy
// chunk-system RegionType (include/RegionType.hpp, at repo root, outside
// Map::) -- plan.md's own M157 describes the hand-off as converting
// Map:: output INTO legacy-system inputs, so Map:: itself should not
// depend on a legacy-system enum. M157 is where a ZoneCandidate here gets
// translated into an actual per-chunk RegionType value; this enum only
// needs to survive that translation, not match RegionType's exact API.
//
// Naming mirrors plan.md's own M156 parenthetical (house_block/
// apartment_block/shop_street/park/square), using RegionType's exact
// spelling ("small_house_block") where the two differ so a future M157
// translation is a direct 1:1 lookup, not a fuzzy remap.
//
// New enumerators are appended (never inserted) so MapPayloadCodec's
// integer ordinals stay stable across versions (same rule as FeatureType).
enum class ZoneCandidate {
    none,               // not part of a zoned block (non-urban land, ocean, etc.)
    small_house_block,
    apartment_block,
    shop_street,
    park,
    square,
};

inline std::string to_string(ZoneCandidate z) {
    switch (z) {
        case ZoneCandidate::none:              return "none";
        case ZoneCandidate::small_house_block: return "small_house_block";
        case ZoneCandidate::apartment_block:   return "apartment_block";
        case ZoneCandidate::shop_street:       return "shop_street";
        case ZoneCandidate::park:              return "park";
        case ZoneCandidate::square:            return "square";
    }
    return "none";
}

} // namespace MeshWorld::Map
