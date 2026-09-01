// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "ChunkGenerator.hpp"
#include "generators/EmptyGenerator.hpp"
#include "generators/RoadGenerator.hpp"
#include "generators/CrossroadGenerator.hpp"
#include "generators/ParkGenerator.hpp"
#include "generators/SmallHouseBlockGenerator.hpp"
#include "generators/ApartmentBlockGenerator.hpp"
#include "generators/ShopStreetGenerator.hpp"
#include "generators/SquareGenerator.hpp"
#include "generators/RiverBankGenerator.hpp"
#include "generators/BridgeGenerator.hpp"
#include "generators/ForestGenerator.hpp"
#include "generators/JungleGenerator.hpp"
#include "generators/DesertGenerator.hpp"
#include "generators/CaveGenerator.hpp"
#include "generators/MountainGenerator.hpp"
#include "generators/MeadowGenerator.hpp"
#include "generators/BeachGenerator.hpp"
#include "generators/OceanGenerator.hpp"
#include "generators/SwampGenerator.hpp"
#include "generators/TundraGenerator.hpp"

namespace MeshWorld {

static EmptyGenerator           s_empty;
static RoadGenerator            s_road;
static CrossroadGenerator       s_crossroad;
static ParkGenerator            s_park;
static SmallHouseBlockGenerator s_small_house;
static ApartmentBlockGenerator  s_apartment;
static ShopStreetGenerator      s_shop_street;
static SquareGenerator          s_square;
static RiverBankGenerator       s_river_bank;
static BridgeGenerator          s_bridge;
static ForestGenerator          s_forest;
static JungleGenerator          s_jungle;
static DesertGenerator          s_desert;
static CaveGenerator            s_cave;
static MountainGenerator        s_mountain;
static MeadowGenerator          s_meadow;
static BeachGenerator           s_beach;
static OceanGenerator           s_ocean;
static SwampGenerator           s_swamp;
static TundraGenerator          s_tundra;

// Fallback chain: exact (zone,region) → any-zone region → zone open → EmptyGenerator.
ChunkGenerator* get_generator(ZoneType z, RegionType r) {
    // 1. Exact (zone, region) matches.
    if (z == ZoneType::city) {
        switch (r) {
            case RegionType::road:              return &s_road;
            case RegionType::crossroad:         return &s_crossroad;
            case RegionType::park:              return &s_park;
            case RegionType::small_house_block: return &s_small_house;
            case RegionType::apartment_block:   return &s_apartment;
            case RegionType::shop_street:       return &s_shop_street;
            case RegionType::square:            return &s_square;
            case RegionType::river_bank:        return &s_river_bank;
            case RegionType::bridge:            return &s_bridge;
            default: break;
        }
    }

    // 2. Region-only fallbacks that make sense in any zone.
    switch (r) {
        case RegionType::road:              return &s_road;
        case RegionType::crossroad:         return &s_crossroad;
        case RegionType::park:              return &s_park;
        case RegionType::small_house_block: return &s_small_house;
        case RegionType::apartment_block:   return &s_apartment;
        case RegionType::shop_street:       return &s_shop_street;
        case RegionType::square:            return &s_square;
        case RegionType::river_bank:        return &s_river_bank;
        case RegionType::bridge:            return &s_bridge;
        case RegionType::oasis:             return &s_desert;
        case RegionType::cave_chamber:      return &s_cave;
        default: break;
    }

    // 3. Zone open fallback — use the zone's primary generator.
    switch (z) {
        case ZoneType::forest:   return &s_forest;
        case ZoneType::jungle:   return &s_jungle;
        case ZoneType::desert:   return &s_desert;
        case ZoneType::cave:     return &s_cave;
        case ZoneType::mountain: return &s_mountain;
        case ZoneType::meadow:   return &s_meadow;
        case ZoneType::beach:    return &s_beach;
        case ZoneType::ocean:    return &s_ocean;
        case ZoneType::swamp:    return &s_swamp;
        case ZoneType::tundra:   return &s_tundra;
        // R142 -- MAP16's named sub-biomes reuse the closest mature natural
        // generator until Stage 6 supplies distinct MC3 asset libraries for
        // each one. This is deliberate family fallback, not a claim that a
        // mesa already looks like a bespoke mesa: the important invariant is
        // that a valid, reachable biome never becomes an empty flat chunk.
        case ZoneType::savanna:
        case ZoneType::steppe:
        case ZoneType::prairie:
        case ZoneType::chaparral:
        case ZoneType::shrubland:
        case ZoneType::alpine_meadow: return &s_meadow;

        case ZoneType::taiga:
        case ZoneType::temperate_rainforest:
        case ZoneType::mixed_forest:
        case ZoneType::cloud_forest:
        case ZoneType::mangrove:
        case ZoneType::riparian_forest:
        case ZoneType::tropical_dry_forest: return &s_forest;
        case ZoneType::bamboo_forest: return &s_jungle;

        case ZoneType::marsh:
        case ZoneType::floodplain:
        case ZoneType::bog:
        case ZoneType::muskeg: return &s_swamp;

        case ZoneType::dunes:
        case ZoneType::rocky_desert:
        case ZoneType::cold_desert:
        case ZoneType::salt_flat:
        case ZoneType::badlands:
        case ZoneType::mesa:
        case ZoneType::canyon:
        case ZoneType::oasis: return &s_desert;

        case ZoneType::glacier:
        case ZoneType::permafrost:
        case ZoneType::ice_cap: return &s_tundra;

        case ZoneType::volcanic:
        case ZoneType::geothermal:
        case ZoneType::ash_plain:
        case ZoneType::volcanic_island:
        case ZoneType::sea_cliff: return &s_mountain;

        // OceanGenerator branches its content on ctx.zone where it has a
        // dedicated variation. The remaining family members still get its
        // established water scene rather than EmptyGenerator.
        case ZoneType::coral_reef:
        case ZoneType::kelp_forest:
        case ZoneType::deep_ocean:
        case ZoneType::lagoon:
        case ZoneType::fjord: return &s_ocean;
        case ZoneType::tidal_flat: return &s_beach;
        default: break;
    }

    return &s_empty;
}

} // namespace MeshWorld
