// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <string>
#include <stdexcept>

namespace MeshWorld {

// RegionType defines the specific chunk type within a zone.
// The combination (ZoneType, RegionType) fully determines what to generate.
//
// Universal regions work in any zone; zone-specific regions are only valid
// in certain zones (but the generator gracefully handles mismatches).
enum class RegionType {
    // --- Universal ---
    open,           // default surface of a zone (jungle dense, city block, desert sand…)
    water,          // water body (lake, river, sea; appearance depends on zone)
    ruins,          // ruins (city rubble, jungle temple, desert fort…)
    cliff,          // steep rock face / canyon wall
    clearing,       // open area inside dense vegetation
    highland,       // elevated sub-area (plateau, hilltop)

    // --- City-specific ---
    road,
    crossroad,
    small_house_block,
    apartment_block,
    shop_street,
    square,
    park,
    river_bank,
    bridge,

    // --- Desert-specific ---
    oasis,

    // --- Cave-specific ---
    cave_chamber,

    // --- Fallback ---
    empty,          // completely empty ground
};

inline std::string to_string(RegionType r) {
    switch (r) {
        case RegionType::open:              return "open";
        case RegionType::water:             return "water";
        case RegionType::ruins:             return "ruins";
        case RegionType::cliff:             return "cliff";
        case RegionType::clearing:          return "clearing";
        case RegionType::highland:          return "highland";
        case RegionType::road:              return "road";
        case RegionType::crossroad:         return "crossroad";
        case RegionType::small_house_block: return "small_house_block";
        case RegionType::apartment_block:   return "apartment_block";
        case RegionType::shop_street:       return "shop_street";
        case RegionType::square:            return "square";
        case RegionType::park:              return "park";
        case RegionType::river_bank:        return "river_bank";
        case RegionType::bridge:            return "bridge";
        case RegionType::oasis:             return "oasis";
        case RegionType::cave_chamber:      return "cave_chamber";
        case RegionType::empty:             return "empty";
    }
    return "empty";
}

inline RegionType region_from_string(const std::string& s) {
    if (s == "open")              return RegionType::open;
    if (s == "water")             return RegionType::water;
    if (s == "ruins")             return RegionType::ruins;
    if (s == "cliff")             return RegionType::cliff;
    if (s == "clearing")          return RegionType::clearing;
    if (s == "highland")          return RegionType::highland;
    if (s == "road")              return RegionType::road;
    if (s == "crossroad")         return RegionType::crossroad;
    if (s == "small_house_block") return RegionType::small_house_block;
    if (s == "apartment_block")   return RegionType::apartment_block;
    if (s == "shop_street")       return RegionType::shop_street;
    if (s == "square")            return RegionType::square;
    if (s == "park")              return RegionType::park;
    if (s == "river_bank")        return RegionType::river_bank;
    if (s == "bridge")            return RegionType::bridge;
    if (s == "oasis")             return RegionType::oasis;
    if (s == "cave_chamber")      return RegionType::cave_chamber;
    if (s == "empty")             return RegionType::empty;
    // Legacy aliases
    if (s == "empty_field")       return RegionType::empty;
    throw std::invalid_argument("Unknown region type: " + s);
}

inline bool is_road_region(RegionType r) {
    return r == RegionType::road || r == RegionType::crossroad;
}

} // namespace MeshWorld
