// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <string>
#include <stdexcept>

namespace MeshWorld {

// ZoneType defines the broad biome/environment of a world area.
// One zone covers many chunks. The region type then specifies what
// a particular chunk within that zone looks like.
//
// M235 (MAP16, 2026-07-10): grown from 12 to 52 values at explicit user
// request ("maximum possible number of biomes"). The 40 new values below
// are grouped by climate family (see plan.md's MAP16 section for the full
// rationale); each still needs its own BiomeClassifier::classify() branch
// (M236-M275) before it's reachable from real terrain generation -- until
// then these are valid, nameable, colorable, but unused-by-classify()
// enum values, same "exists but not yet wired up" state ZoneType::cave had
// before MAP21. `empty` MUST stay the last value: MapValidator's
// `max_valid` sentinel (src/MapValidator.cpp) is
// `static_cast<uint8_t>(ZoneType::empty)`, so any new value must be
// inserted BEFORE it, never after.
enum class ZoneType {
    city,
    jungle,
    desert,
    forest,
    ocean,
    mountain,
    tundra,
    swamp,
    cave,
    meadow,
    beach,

    // Grassland / dry-climate (5)
    savanna,
    steppe,
    prairie,
    chaparral,
    shrubland,

    // Forest variants (8)
    taiga,
    temperate_rainforest,
    mixed_forest,
    cloud_forest,
    mangrove,
    bamboo_forest,
    riparian_forest,
    tropical_dry_forest,

    // Wetland variants (4)
    marsh,
    floodplain,
    bog,
    muskeg,

    // Desert / arid variants (8)
    dunes,
    rocky_desert,
    cold_desert,
    salt_flat,
    badlands,
    mesa,
    canyon,
    oasis,

    // Cold / high-elevation (4)
    glacier,
    permafrost,
    alpine_meadow,
    ice_cap,

    // Volcanic / geothermal (4)
    volcanic,
    geothermal,
    ash_plain,
    volcanic_island,

    // Aquatic / coastal (7)
    coral_reef,
    kelp_forest,
    deep_ocean,
    lagoon,
    fjord,
    tidal_flat,
    sea_cliff,

    empty,   // undefined / no zone assigned -- MUST stay last, see comment above
};

inline std::string to_string(ZoneType z) {
    switch (z) {
        case ZoneType::city:     return "city";
        case ZoneType::jungle:   return "jungle";
        case ZoneType::desert:   return "desert";
        case ZoneType::forest:   return "forest";
        case ZoneType::ocean:    return "ocean";
        case ZoneType::mountain: return "mountain";
        case ZoneType::tundra:   return "tundra";
        case ZoneType::swamp:    return "swamp";
        case ZoneType::cave:     return "cave";
        case ZoneType::meadow:   return "meadow";
        case ZoneType::beach:    return "beach";

        case ZoneType::savanna:               return "savanna";
        case ZoneType::steppe:                return "steppe";
        case ZoneType::prairie:                return "prairie";
        case ZoneType::chaparral:              return "chaparral";
        case ZoneType::shrubland:              return "shrubland";

        case ZoneType::taiga:                  return "taiga";
        case ZoneType::temperate_rainforest:   return "temperate_rainforest";
        case ZoneType::mixed_forest:           return "mixed_forest";
        case ZoneType::cloud_forest:           return "cloud_forest";
        case ZoneType::mangrove:               return "mangrove";
        case ZoneType::bamboo_forest:          return "bamboo_forest";
        case ZoneType::riparian_forest:        return "riparian_forest";
        case ZoneType::tropical_dry_forest:    return "tropical_dry_forest";

        case ZoneType::marsh:                  return "marsh";
        case ZoneType::floodplain:              return "floodplain";
        case ZoneType::bog:                    return "bog";
        case ZoneType::muskeg:                 return "muskeg";

        case ZoneType::dunes:                  return "dunes";
        case ZoneType::rocky_desert:           return "rocky_desert";
        case ZoneType::cold_desert:            return "cold_desert";
        case ZoneType::salt_flat:              return "salt_flat";
        case ZoneType::badlands:               return "badlands";
        case ZoneType::mesa:                   return "mesa";
        case ZoneType::canyon:                 return "canyon";
        case ZoneType::oasis:                  return "oasis";

        case ZoneType::glacier:                return "glacier";
        case ZoneType::permafrost:             return "permafrost";
        case ZoneType::alpine_meadow:          return "alpine_meadow";
        case ZoneType::ice_cap:                return "ice_cap";

        case ZoneType::volcanic:               return "volcanic";
        case ZoneType::geothermal:             return "geothermal";
        case ZoneType::ash_plain:              return "ash_plain";
        case ZoneType::volcanic_island:        return "volcanic_island";

        case ZoneType::coral_reef:             return "coral_reef";
        case ZoneType::kelp_forest:            return "kelp_forest";
        case ZoneType::deep_ocean:             return "deep_ocean";
        case ZoneType::lagoon:                 return "lagoon";
        case ZoneType::fjord:                  return "fjord";
        case ZoneType::tidal_flat:             return "tidal_flat";
        case ZoneType::sea_cliff:              return "sea_cliff";

        case ZoneType::empty:    return "empty";
    }
    return "empty";
}

inline ZoneType zone_from_string(const std::string& s) {
    if (s == "city")     return ZoneType::city;
    if (s == "jungle")   return ZoneType::jungle;
    if (s == "desert")   return ZoneType::desert;
    if (s == "forest")   return ZoneType::forest;
    if (s == "ocean")    return ZoneType::ocean;
    if (s == "mountain") return ZoneType::mountain;
    if (s == "tundra")   return ZoneType::tundra;
    if (s == "swamp")    return ZoneType::swamp;
    if (s == "cave")     return ZoneType::cave;
    if (s == "meadow")   return ZoneType::meadow;
    if (s == "beach")    return ZoneType::beach;

    if (s == "savanna")               return ZoneType::savanna;
    if (s == "steppe")                return ZoneType::steppe;
    if (s == "prairie")               return ZoneType::prairie;
    if (s == "chaparral")             return ZoneType::chaparral;
    if (s == "shrubland")             return ZoneType::shrubland;

    if (s == "taiga")                 return ZoneType::taiga;
    if (s == "temperate_rainforest")  return ZoneType::temperate_rainforest;
    if (s == "mixed_forest")          return ZoneType::mixed_forest;
    if (s == "cloud_forest")          return ZoneType::cloud_forest;
    if (s == "mangrove")              return ZoneType::mangrove;
    if (s == "bamboo_forest")         return ZoneType::bamboo_forest;
    if (s == "riparian_forest")       return ZoneType::riparian_forest;
    if (s == "tropical_dry_forest")   return ZoneType::tropical_dry_forest;

    if (s == "marsh")                 return ZoneType::marsh;
    if (s == "floodplain")            return ZoneType::floodplain;
    if (s == "bog")                   return ZoneType::bog;
    if (s == "muskeg")                return ZoneType::muskeg;

    if (s == "dunes")                 return ZoneType::dunes;
    if (s == "rocky_desert")          return ZoneType::rocky_desert;
    if (s == "cold_desert")           return ZoneType::cold_desert;
    if (s == "salt_flat")             return ZoneType::salt_flat;
    if (s == "badlands")              return ZoneType::badlands;
    if (s == "mesa")                  return ZoneType::mesa;
    if (s == "canyon")                return ZoneType::canyon;
    if (s == "oasis")                 return ZoneType::oasis;

    if (s == "glacier")               return ZoneType::glacier;
    if (s == "permafrost")            return ZoneType::permafrost;
    if (s == "alpine_meadow")         return ZoneType::alpine_meadow;
    if (s == "ice_cap")               return ZoneType::ice_cap;

    if (s == "volcanic")              return ZoneType::volcanic;
    if (s == "geothermal")            return ZoneType::geothermal;
    if (s == "ash_plain")             return ZoneType::ash_plain;
    if (s == "volcanic_island")       return ZoneType::volcanic_island;

    if (s == "coral_reef")            return ZoneType::coral_reef;
    if (s == "kelp_forest")           return ZoneType::kelp_forest;
    if (s == "deep_ocean")            return ZoneType::deep_ocean;
    if (s == "lagoon")                return ZoneType::lagoon;
    if (s == "fjord")                 return ZoneType::fjord;
    if (s == "tidal_flat")            return ZoneType::tidal_flat;
    if (s == "sea_cliff")             return ZoneType::sea_cliff;

    if (s == "empty")    return ZoneType::empty;
    throw std::invalid_argument("Unknown zone type: " + s);
}

// M236-M275 (MAP16, 2026-07-10): BiomeClassifier::classify() now produces 6
// distinct underwater outcomes by depth/temperature (ocean, deep_ocean,
// coral_reef, kelp_forest, lagoon, fjord) instead of always plain `ocean`
// -- any code that used to check `== ZoneType::ocean` to mean "this cell is
// underwater" (land/ocean ratio counting, coastal-adjacency checks, etc.)
// must use this helper instead, or it will silently miscount 5 of the 6
// underwater outcomes as land. `tidal_flat`/`sea_cliff` are coastal LAND
// features (not underwater, same family as `beach`), so deliberately
// excluded here despite living in the same "aquatic/coastal" plan.md group.
inline bool is_ocean_family(ZoneType z) {
    switch (z) {
        case ZoneType::ocean:
        case ZoneType::deep_ocean:
        case ZoneType::coral_reef:
        case ZoneType::kelp_forest:
        case ZoneType::lagoon:
        case ZoneType::fjord:
            return true;
        default:
            return false;
    }
}

} // namespace MeshWorld
