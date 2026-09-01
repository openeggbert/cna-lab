// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "Map/Hydrology.hpp"        // HydrologyNetwork
#include "Map/MapTilePayload.hpp"   // FieldGrid
#include "Map/MountainRanges.hpp"   // MountainRangeNetwork
#include "Map/Settlements.hpp"      // SettlementNetwork

namespace MeshWorld::Map {

// Country data model (MAP9, M139). One country per Settlements::place()
// (M138) capital, grown outward over land by Countries::grow(). Its own
// type rather than reusing Map::MapFeature directly, same reasoning as
// Settlement (M137) vs. MapFeature::Border: naming/converting a grown
// country into a named MapFeature::Border polygon for storage is a later
// step, not this one (mirrors M132's role for hydrology/mountain ranges).

// One country: the capital it grew from, and every land cell that ended up
// in its territory. `territory` is every claimed cell's world position (M123's
// Lake::shoreline sets the precedent for this codebase: the full filled
// region, not a traced boundary loop). `border` is that traced outline --
// grow() builds it from the same claimed-cell raster while it's still in
// scope (see grow()'s own doc comment): the ordered sequence of grid-corner
// points walking the region's outer edge, closed (first point repeated as
// last), suitable to hand straight to a MapFeature::Border. Empty if grow()
// couldn't trace a boundary (should not happen for a non-empty territory,
// kept as a defensive empty-is-safe default rather than an assert). V1: the
// single largest traced loop only -- a country whose territory encloses a
// hole (a rival's pocket fully surrounded) has that hole's own inner loop
// discarded, not represented as a second ring; genuinely rare for a
// priority-flood growth pattern and not worth a multi-ring MapFeature
// convention this codebase doesn't have anywhere else yet.
// `name`/`culture` are empty until a later step (M141) populates them.
struct Country {
    double                              capital_x{0.0};
    double                              capital_z{0.0};
    std::vector<std::array<double, 2>>  territory;
    std::vector<std::array<double, 2>>  border;
    std::string                         name;
    std::string                         culture;
};

// The complete set of countries grown in one pass.
struct CountryNetwork {
    std::vector<Country> countries;

    bool empty() const { return countries.empty(); }
};

// Pure static; no state (mirrors Hydrology/MountainRanges/BiomeRefinement/
// Settlements' style).
class Countries {
public:
    // M139 — grows one Country per SettlementTier::Capital in `settlements`
    // outward over land cells of `elevation`, via multi-source, uniform-cost
    // priority-flood: every capital is a seed at cost 0, and each land cell
    // ends up owned by whichever seed's growth front reaches it first/
    // cheapest (a step to a cardinal neighbor costs 1, diagonal costs
    // sqrt(2), avoiding diamond-shaped artifacts a pure step-count metric
    // would produce). This mirrors Hydrology::trace()'s M123 fill_basin()
    // technique -- a std::priority_queue popped in non-decreasing cost
    // order -- generalized from one pit to N simultaneous capital seeds
    // instead of inventing a new flood-fill approach.
    //
    // Ocean cells are absorbed (popped, marked claimed) but never assigned
    // to a country and never expanded past -- growth cannot cross open
    // ocean, so two capitals on separate landmasses never contest the same
    // territory even if one is geometrically "closer" through the water.
    //
    // City/Town/Village-tier settlements in `settlements` are not seeds
    // themselves (plan.md M139: "region growth from capitals" specifically)
    // -- only Capital-tier entries participate. Returns an empty network if
    // `settlements` has no capitals, or `elevation` is empty.
    //
    // Deterministic given the same inputs: no entropy/randomness is
    // involved, growth order is a pure function of the seeds and elevation
    // (and `hydrology`/`mountains`, when given).
    //
    // M140 — `hydrology`/`mountains` (both optional; nullptr means "not
    // considered", preserving M139's original uniform-cost behavior
    // exactly) mark river and ridge cells as natural features; land cells
    // within `coastal_radius_cells` of the ocean (the same check
    // `BiomeRefinement::applyCoastalBeach()`, M128, already established)
    // are natural features too, no extra input needed for that one. Every
    // step that *enters* a natural-feature cell costs
    // `natural_feature_cost_multiplier` times as much as a plain step, in
    // both directions equally -- growth doesn't refuse to cross a river/
    // ridge/coast, it's just discouraged from pushing *past* one, so a
    // competing capital on the other side is more likely to claim the
    // cells beyond it. The result: borders tend to settle on or near a
    // natural feature rather than cutting through open land past it (this
    // is layered on top of M139's existing growth, not a replacement for
    // it -- call with the defaults to get exactly M139's behavior).
    //
    // Also traces each country's Country::border from the same claimed-cell
    // raster this function already builds internally (cell-edge boundary
    // walk: every grid edge between an owned cell and a non-owned neighbor
    // -- different owner, ocean, unclaimed, or off-grid -- becomes a
    // boundary segment; segments are welded end-to-end into closed loops by
    // shared corner, keeping the largest loop per country). No new inputs
    // needed -- this doesn't require settlements/capitals to be visible
    // beyond what growth itself already used.
    static CountryNetwork grow(const SettlementNetwork& settlements,
                                const FieldGrid& elevation, double sea_level_m,
                                double world_x0, double world_z0,
                                double world_x1, double world_z1,
                                const HydrologyNetwork* hydrology = nullptr,
                                const MountainRangeNetwork* mountains = nullptr,
                                double natural_feature_cost_multiplier = 4.0,
                                int coastal_radius_cells = 1);

    // M141 — assigns a culture and name to every country in `net`, in
    // place. Each country gets a single per-country seed
    // (`noise::hash2i(index, axis, entropy)`); `Naming::culture()`/
    // `Naming::country()` already differentiate their own internal domains
    // from the same raw seed (see `Naming.cpp`'s `domain_hash()`), so one
    // seed per country is enough -- matching how other callers in this
    // codebase already reuse a single seed across multiple `Naming::`
    // calls, rather than deriving a separate seed per field.
    //
    // Deliberately no geographic culture correlation (e.g. blending
    // neighboring countries' cultures, or deriving culture from a
    // country's climate/biome mix): plan.md's "assign culture per country"
    // doesn't ask for that, and `Naming::culture()` itself is already a
    // pure seed-driven function with no geographic input -- a plain
    // per-country entropy draw is the smaller, scope-disciplined choice.
    static void name(CountryNetwork& net, std::uint64_t entropy);
};

} // namespace MeshWorld::Map
