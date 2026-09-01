// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "ZoneType.hpp"
#include "RegionType.hpp"
#include "Map/PlanetConstants.hpp"

namespace MeshWorld {

// A region override (chunk-level) within a specific zone block.
struct RegionOverride {
    int        x_min{0}, x_max{0};
    int        y_min{0}, y_max{0};
    RegionType type{RegionType::open};
};

// A zone block covering a rectangular area of the world.
// Chunk-level regions within the block are refined by the nested region list.
struct ZoneOverride {
    int        x_min{0}, x_max{0};
    int        y_min{0}, y_max{0};
    ZoneType   type{ZoneType::empty};
    RegionType region_default{RegionType::open};
    std::vector<RegionOverride> regions;
};

// R128 (city showcase completion) -- one fixed-position, config-driven
// "landmark" instance targeting a SPECIFIC chunk (chunk_x/chunk_y), placed
// by BuildingComposer in ADDITION to (never through) its own generic
// Parcel/AssetRegistry-driven house/apartment/shop placement -- v1 is
// deliberately this simple (one config-listed instance per targeted
// chunk), not a generic "many landmarks, auto-placed by rules" system.
// x/z are chunk-local meters (same convention as MC3Writer::instance()),
// must stay within [0, chunk_size_m].
struct LandmarkPlacement {
    int         chunk_x{0};
    int         chunk_y{0};
    std::string definition_id;
    float       x{0.0f};
    float       z{0.0f};
    float       rotation_y{0.0f};
};

// R134 -- an intentional end of a road corridor. `boundary` is reserved for
// a world-grid edge; `terminus` permits a deliberately authored internal dead
// end. Every other degree-one road cell is a validation error, so a road can
// no longer silently stop where a neighbouring chunk forgot its matching arm.
enum class RoadTerminusKind { boundary, terminus };

struct RoadTerminus {
    int               chunk_x{0};
    int               chunk_y{0};
    std::string       edge;  // "north", "south", "east", or "west"
    RoadTerminusKind  kind{RoadTerminusKind::terminus};
};

struct WorldConfig {
    // Keep construction out of line. Apart from making the ABI boundary
    // explicit, this avoids stale inline default-constructor copies when an
    // incremental build follows an additive configuration-field change.
    WorldConfig();

    std::string version{"0.1"};
    std::string name{"demo_world"};
    uint64_t    seed{42};
    std::string style{"central_europe_small_city"};
    int         map_size_m{1280};
    int         chunk_size_m{64};
    int         grid_w{20};
    int         grid_h{20};

    ZoneType    zone_default{ZoneType::empty};
    RegionType  region_default{RegionType::empty};

    bool procedural{false};
    int  procedural_cell_size{4};

    // R113 v1 (docs/world-composer-design.md §7/§8) -- opt-in gate for the
    // C++ world composer (BuildingComposer). Default false: existing
    // worlds/tests are byte-for-byte unaffected until explicitly enabled.
    // When true, ChunkPipeline tries BuildingComposer first (before Lua,
    // before the C++ fallback) for regions it has a real Parcel layout
    // for (v1: RegionType::small_house_block only) -- falls through to
    // the existing chain unchanged for every other region regardless of
    // this flag.
    bool use_world_composer{false};

    // --- Planetary map fields (MAP3) ---
    // The map layer sits *above* the legacy chunk grid (the fields above); these
    // describe the whole planet. They are optional in world.json — a file that
    // omits them loads with these defaults, so existing worlds stay valid.
    // Range validation (continents 4..20) lives in is_consistent() (M040).
    double   planet_size_m{Map::PLANET_SIZE_M};  // flat square planet edge, meters
    int      continents_min{5};                  // continent-count range (inclusive),
    int      continents_max{12};                 //   default 5..12, allowed 4..20 (M040)
    double   sea_level_m{0.0};                    // ocean threshold elevation, meters
    double   equator_temp_c{30.0};                // climate band: temperature at equator
    double   pole_temp_c{-20.0};                  // climate band: temperature at poles

    // Time-based map entropy (MAP3), owned/persisted by PlanetWorld. 0 means a
    // legacy/chunk world.json that predates the planet map — not a planet world.
    // Non-reproducible by design: revisit consistency comes from persisting this
    // value, never from a fixed seed (see map.md §8).
    uint64_t world_entropy{0};

    std::vector<ZoneOverride> zones;

    // R128 -- see LandmarkPlacement's own doc comment. Empty (the
    // default) means no landmarks configured -- existing worlds are
    // unaffected.
    std::vector<LandmarkPlacement> landmarks;

    // R134 -- authored legal road endings. Empty is valid for worlds with no
    // road regions; worlds that do contain a degree-one road cell must name
    // its terminating edge explicitly (or use a boundary entry).
    std::vector<RoadTerminus> road_termini;

    // Loads `path`, then merges "world.local.json" (same directory, if it
    // exists) on top via RFC 7396 JSON Merge Patch (T242) -- a local,
    // typically-gitignored override file for per-developer tweaks without
    // editing the tracked config. Returns false if `path` itself can't be
    // opened/parsed, or if a present "world.local.json" fails to parse.
    bool load_from_file(const std::string& path);
    bool is_consistent() const;
};

} // namespace MeshWorld
