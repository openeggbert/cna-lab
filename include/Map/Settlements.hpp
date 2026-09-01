// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Map/MapTilePayload.hpp"  // FieldGrid

namespace MeshWorld::Map {

// Settlement data model (MAP9, M137). This is the in-memory shape a future
// placement step (M138: capitals per country, cities/towns by suitability)
// will produce. Deliberately its own type rather than reusing Map::
// MapFeature directly -- MapFeature's FeatureType only distinguishes
// City/Town (no Capital/Village), and mirrors the same reasoning
// Hydrology.hpp (M121) gave for river/lake data: converting placed
// settlements into named MapFeature/PlaceLabel entries for storage is a
// later step (M148, mirrors M132's role for hydrology/mountain ranges),
// not this one.

// Settlement importance, from most to least significant. Matches plan.md
// M137's "capital/city/town/village" wording exactly. New tiers are
// appended, never inserted, if this ever needs (de)serializing -- same
// rule as FeatureType (MapTilePayload.hpp).
enum class SettlementTier {
    Capital,
    City,
    Town,
    Village,
};

// One settlement: where it is, how important it is, and its name. World
// coordinates (x, z) in meters, same convention as RiverPoint/RidgePoint.
// `name` is empty until a later naming step (M148) populates it -- the
// field exists now so the record's shape matches plan.md's description,
// the same way MapFeature::name has always existed but stayed unpopulated
// for hydrology features until M132 named them.
struct Settlement {
    double         x{0.0};
    double         z{0.0};
    SettlementTier tier{SettlementTier::Village};
    std::string    name;
};

// The complete set of settlements placed in one generation pass. Unlike
// HydrologyNetwork's segments, settlements carry no internal linkage to
// each other (that's RoadNetwork's job, M142) -- this is a flat list.
struct SettlementNetwork {
    std::vector<Settlement> settlements;

    bool empty() const { return settlements.empty(); }
};

// Pure static; no state (mirrors Hydrology/MountainRanges/BiomeRefinement's style).
class Settlements {
public:
    // M138 — places `capital_count` + `city_count` + `town_count` settlements
    // (in that tier order, best sites first) on land cells of `elevation`,
    // ranked by suitability: not ocean, not above `max_settlement_elevation_m`
    // above sea level (a hard requirement — disqualifies a candidate
    // entirely), plus two soft bonuses that reuse the exact same
    // neighbor-aware checks MAP8 already established rather than inventing a
    // third convention: near water (`BiomeRefinement::applyCoastalBeach()`'s
    // own within-`coastal_radius_cells` ocean-adjacency check, M128) and flat
    // (`BiomeRefinement::applySwampFlatnessCheck()`'s own 3x3-neighborhood
    // local-relief check, M130, here capped at `max_relief_for_flat_m`).
    //
    // `min_spacing_m` enforces a minimum distance from every already-placed
    // settlement (of any tier) before a candidate is accepted, so capitals
    // don't cluster on top of each other -- scale this to the caller's own
    // world/tile size (there is no universal default, the same way
    // MountainRanges::apply()'s falloff_width_m has none).
    //
    // Deterministic given the same inputs (M070-style requirement, same as
    // every other MAP8/MAP9 generator): candidates are ranked by score, ties
    // broken by a hash of (cell, entropy), never by raster scan order alone.
    //
    // Placement only, no naming yet: every returned `Settlement::name` is
    // empty (M148 fills that in later, mirrors M132's role for hydrology/
    // mountain ranges).
    static SettlementNetwork place(std::uint64_t entropy, const FieldGrid& elevation,
                                    double sea_level_m,
                                    double world_x0, double world_z0,
                                    double world_x1, double world_z1,
                                    int capital_count, int city_count, int town_count,
                                    double min_spacing_m,
                                    double max_settlement_elevation_m = 1200.0,
                                    int coastal_radius_cells = 1,
                                    double max_relief_for_flat_m = 200.0);

    // M148 — assigns a name to every settlement in `net`, in place, via
    // `Naming::city()` fed a per-settlement seed (`noise::hash2i(index,
    // axis, entropy)`) -- the same relationship `Countries::name()` (M141)
    // has to `Countries::grow()` (M139), applied here to settlements
    // instead of countries. `Naming::city()` is used for every tier, not
    // just City -- the same choice `region.lua`'s (M116) own town
    // placement already made (it calls `names.city()` for towns too;
    // there is no separate `Naming::town()`/`Naming::capital()`).
    //
    // Unlike `Country`, `Settlement` has no `culture` field of its own
    // (culture is a per-tile/per-country attribute, not a per-settlement
    // one) -- `culture` is a parameter here, not something this function
    // assigns, matching how `MapBuilder::addCity()`/`FeatureNaming`'s own
    // callers already pass a tile's culture in rather than deriving one
    // per settlement.
    static void name(SettlementNetwork& net, const std::string& culture, std::uint64_t entropy);

    // M148 — appends one `PlaceLabel` per settlement in `network` to
    // `labels`, mirroring `FeatureNaming::appendHydrologyFeatures()`'s own
    // "append into a caller-owned vector" shape (M132), just for
    // `PlaceLabel` (plan.md's own "persist labels with positions" wording)
    // rather than `MapFeature` -- `PlaceLabel` is `MapTilePayload`'s
    // existing, already-serialized (`MapPayloadCodec`), "cheap text draw"
    // type (see its own doc comment), distinct from and not a
    // replacement for `MapFeature::City`/`Town` (which `MapBuilder::
    // addCity()` already populates from Lua). `PlaceLabel::kind` is the
    // settlement's tier, lowercased ("capital"/"city"/"town"/"village"),
    // matching the freeform `kind` convention existing `PlaceLabel` test
    // fixtures already use ("country", "city", "river", ...).
    //
    // Purely additive -- only appends, never clears `labels` first, so it
    // composes with labels from any other source the same way `Roads::
    // exportCrossings()` (M145) composes with a future river-crossing
    // exporter. Does not require `name()` to have been called first: a
    // settlement with an empty `name` simply produces a label with an
    // empty name -- this function only shapes/copies data, it does not
    // enforce a naming precondition.
    static void appendLabels(std::vector<PlaceLabel>& labels, const SettlementNetwork& network);
};

} // namespace MeshWorld::Map
