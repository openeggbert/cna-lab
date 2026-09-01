// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <array>
#include <vector>

#include "Map/Hydrology.hpp"       // HydrologyNetwork
#include "Map/MapTilePayload.hpp"  // FieldGrid
#include "Map/Settlements.hpp"     // SettlementNetwork, SettlementTier

namespace MeshWorld::Map {

// Road network data model (MAP9, M142). This is the in-memory shape a
// future build step (M143: minimum-ish spanning network + a few redundant
// links) will produce. Settlements.hpp (M137) forward-referenced this file
// by name when noting that settlements "carry no internal linkage to each
// other (that's RoadNetwork's job, M142)" -- this is that linkage: unlike
// SettlementNetwork's flat, unlinked list, a road network is an actual
// graph (plan.md's own wording), not a flat partition (Countries::grow(),
// M139) or a DAG of at-most-one-downstream-link segments (Hydrology,
// M121/M123).

// One node in the road graph: a position a trunk road connects to. Copies
// its {x, z} from the Settlement it represents rather than storing an
// index back into a SettlementNetwork -- same choice M139's Country made
// for capital_x/capital_z (see Countries.hpp), for the same reason: a
// dangling index would result if the source SettlementNetwork outlives or
// is regenerated independently of this RoadNetwork. Which settlements
// become nodes ("trunk roads" implies major settlements, not every
// village) is a decision for M143's build step, not stored here -- this
// struct only describes the shape of a node once chosen.
struct RoadNode {
    double x{0.0};
    double z{0.0};
};

// One edge in the road graph: a road connecting two nodes. `from`/`to` are
// indices into this same RoadNetwork's own `nodes` (not into an external
// SettlementNetwork -- no dangling-index risk here since both live in the
// same struct and are always built together), mirroring how
// RiverSegment::downstream_segment (Hydrology.hpp) indexes into its own
// HydrologyNetwork::rivers. `path` is the road's actual route in world
// coordinates, ordered from `from` to `to` -- present from this data-model
// task already rather than added later, the same way RiverSegment::points
// existed from Hydrology.hpp's own data-model task (M121) even though
// Hydrology::trace() didn't populate it until M122: a road, like a river,
// isn't a meaningful concept without a path (M144's slope-avoiding,
// river-bridging route is the whole point of the feature). Empty until
// M143's build step populates it.
struct RoadEdge {
    int                                 from{-1};
    int                                 to{-1};
    std::vector<std::array<double, 2>> path;
};

// The complete road graph built in one pass.
struct RoadNetwork {
    std::vector<RoadNode> nodes;
    std::vector<RoadEdge> edges;

    bool empty() const { return nodes.empty(); }
};

// Pure static; no state (mirrors Hydrology/MountainRanges/Settlements/
// Countries' style). Named `Roads`, not `RoadNetwork` -- that name is
// already taken by the struct above, and every other data/algorithm pair
// in this codebase (Hydrology/HydrologyNetwork, MountainRanges/
// MountainRangeNetwork, Settlements/SettlementNetwork, Countries/
// CountryNetwork) names its algorithm class as the plural collection noun
// and suffixes the struct with "Network" -- this follows that pattern
// despite plan.md's own M143-M145 task lines saying "RoadNetwork: build/
// roads avoid/export..." (plan.md's file-path wording has already proven
// loose elsewhere, e.g. M137 said "include/Settlements.hpp" but it landed
// at "include/Map/Settlements.hpp").
class Roads {
public:
    // M143 -- connects `settlements` into a graph: a minimum spanning tree
    // (Kruskal's algorithm, union-find) over every settlement whose tier is
    // at or above `max_tier`'s importance (SettlementTier's declaration
    // order *is* its importance order, most to least -- Capital=0 is the
    // most important, so "at or above" means "enum value <=
    // static_cast<int>(max_tier)"), plus a few redundant links:
    // `extra_redundant_links` extra edges taken from the candidate edges
    // Kruskal's own ascending-sorted scan already rejected for closing a
    // cycle (i.e. the next-shortest connections beyond the spanning tree --
    // these come for free out of Kruskal's edge scan, no separate
    // nearest-neighbor search needed).
    //
    // Kruskal's algorithm, not Countries::grow()'s std::priority_queue
    // priority-flood, despite the surface similarity (both "grow a
    // structure by repeatedly taking the next-cheapest option"): grow()
    // floods a dense W*H grid from N seeds, where a priority queue of
    // frontier cells is the natural fit; this instead has a small, sparse
    // set of nodes (settlements, typically tens, not W*H) and needs
    // all-pairs candidate edges considered by weight, which is exactly
    // Kruskal's shape, not a grid flood's. Prim's (grow()'s closer
    // relative, since it's also priority-queue-driven) was also
    // considered, but Kruskal's single ascending-sorted edge scan
    // naturally yields the "few redundant links" as a side effect (the
    // cycle-forming edges it skips over), where Prim's frontier-only view
    // wouldn't surface those without extra bookkeeping.
    //
    // Distance-only edge cost (straight-line, std::hypot(dx, dz)) --
    // deliberately no elevation/slope/river awareness yet, mirroring the
    // M139/M140 split (M139 was uniform-cost; M140 layered natural-feature
    // costs on afterward): M144 layers routing (steep-slope avoidance,
    // river bridge crossings) onto this network's topology; this task only
    // decides *which* settlements connect to which.
    //
    // Each edge's `path` is populated as a straight two-point line between
    // its endpoints, not left empty -- M144's job is to replace this with
    // an actual routed path, not to populate an empty one from scratch.
    //
    // Deterministic given the same `settlements` (no entropy/randomness):
    // edge order is a pure function of node positions, ties broken by
    // node-index pair so the result never depends on std::sort's
    // implementation-defined behavior on equal-weight edges.
    //
    // Returns a network with nodes but no edges if fewer than 2 settlements
    // qualify (a single node has nothing to connect to -- mirrors
    // Countries::grow()'s empty-capital-list guard clause).
    //
    // M144 -- layers real per-edge routing onto the topology decided above,
    // the same "extra optional parameters, defaults reproduce the prior
    // behavior exactly" shape M140 used for Countries::grow(): passing
    // `elevation == nullptr` (the default) skips routing entirely and keeps
    // M143's straight two-point `path` on every edge, byte-for-byte. This
    // is deliberately NOT a re-decision of which settlements connect to
    // which (that question -- topology -- was already answered above by
    // Kruskal's algorithm on straight-line distance); M144 only changes
    // the *route* an already-chosen edge's road takes, replacing its
    // straight line with an actual path walked cell-by-cell across
    // `elevation` (8-directional, same step shape Countries::grow() uses)
    // from one endpoint to the other. A cell-to-cell step costs its plain
    // world distance, times `steep_slope_cost_multiplier` if entering it
    // crosses an elevation difference greater than `steep_slope_threshold_m`
    // (mirrors Settlements::place()'s max_relief_for_flat_m / BiomeRefinement
    // ::applySwampFlatnessCheck()'s (M130) own "steepness" checks, though
    // this one is a soft toll, not a hard cap -- roads CAN climb, they're
    // just discouraged from it), times `river_crossing_cost_multiplier` if
    // the cell is part of a traced `hydrology` river (re-traces
    // HydrologyNetwork::rivers into a grid mask, the same technique
    // Countries::grow()'s M140 natural-feature cost already established,
    // rather than a fourth "is this a river cell" convention) -- a soft
    // toll representing the cost of a bridge, not a hard block, so the
    // cheapest path naturally funnels multiple crossings toward one
    // sensible bridging point instead of forbidding crossings outright.
    // Deliberately does NOT avoid ocean: plan.md's M144 wording only
    // mentions slopes and river crossings, not water: adding ocean
    // avoidance here would be scope creep beyond what was asked, though
    // note `Countries::grow()` already keeps every settlement it seeds from
    // sitting on land, so in practice a route between two real settlements
    // rarely has a reason to detour through open ocean anyway.
    //
    // Runs a per-edge Dijkstra search (a priority queue of frontier cells,
    // same shape as Countries::grow()'s own priority-flood, but a
    // point-to-point shortest path instead of a multi-source flood --
    // early-exits the moment the target cell is popped) rather than one
    // flood-fill shared across all edges, since edges want independent,
    // possibly-overlapping routes, not a partition. If somehow no path is
    // found (should not happen on a normal fully-connected grid, since
    // every step is merely costly, never blocked -- kept only as a defensive
    // fallback for a malformed/disconnected `elevation` grid), the edge
    // keeps its M143 straight-line `path` rather than being left broken.
    //
    // `world_x0`/`world_z0`/`world_x1`/`world_z1` are `elevation`'s world
    // bounds, the same convention `Countries::grow()` already takes (cell
    // size derived from grid resolution over that extent); `hydrology` is
    // optional, nullptr meaning "no rivers considered", identical to
    // `Countries::grow()`'s own nullable `hydrology`/`mountains` params.
    // `steep_slope_threshold_m`/`steep_slope_cost_multiplier`/
    // `river_crossing_cost_multiplier` all have arbitrary but documented
    // defaults -- scale them to your world/tile size and desired road
    // behavior, there is no universal default, the same acknowledgment
    // `Settlements::place()`'s `min_spacing_m` and `MountainRanges::apply()`
    // 's `falloff_width_m` already make for their own tunables.
    //
    // Deterministic given the same inputs, same caveat as every other
    // priority-queue-driven generator in this codebase (Hydrology::trace(),
    // Countries::grow()): repeatable on a given build/platform, without a
    // cross-platform floating-point-order guarantee.
    static RoadNetwork build(const SettlementNetwork& settlements,
                              SettlementTier max_tier = SettlementTier::City,
                              int extra_redundant_links = 2,
                              const FieldGrid* elevation = nullptr,
                              double world_x0 = 0.0, double world_z0 = 0.0,
                              double world_x1 = 0.0, double world_z1 = 0.0,
                              const HydrologyNetwork* hydrology = nullptr,
                              double steep_slope_threshold_m = 30.0,
                              double steep_slope_cost_multiplier = 4.0,
                              double river_crossing_cost_multiplier = 4.0);

    // M145 -- appends a `EdgeCrossingType::Road` `EdgeCrossing` to `edges`
    // (N=0/E=1/S=2/W=3, matching `MapTilePayload::edges`'s own documented
    // index convention) wherever any edge's `path` in `net` crosses one of
    // the 4 axis-aligned boundary lines of the tile spanned by
    // [world_x0, world_x1] x [world_z0, world_z1] -- the same explicit
    // world-bounds convention `build()`/`Countries::grow()` already use,
    // not `TileCoord::world_bounds()`'s `WorldBounds` struct, for
    // consistency with the rest of this file (a caller with a real
    // `MapTilePayload` unpacks `payload.tile.world_bounds()`'s 4 fields).
    //
    // Deliberately a plain, standalone, pure function -- it does NOT call
    // `Settlements::place()`/`Roads::build()` itself, and nothing calls
    // *this* function from `PlanetGenerator`/`ChildGenerator` either.
    // Mirrors M138-M144's own tested-but-not-wired pattern: `net`'s
    // topology and per-edge `path` are treated as already-finished,
    // read-only input, not recomputed or second-guessed here.
    //
    // Road crossings only, not river crossings, despite `EdgeCrossingType`
    // already having a `River` value too -- plan.md's M145 wording says
    // "road crossings" specifically, and no generator populates river
    // crossings from `HydrologyNetwork` either (a separate, still-open,
    // unscoped gap -- see NEXT.md). Conflating the two data sources into
    // one function would be scope creep beyond what was asked.
    //
    // Purely additive: only appends to `edges[i].crossings`, never clears
    // or overwrites it first, so a caller can run this after (or before)
    // anything else that populates crossings from another source (e.g. a
    // future river-crossing exporter) without one pass destroying the
    // other's work.
    //
    // Segment/boundary-line intersection: for each consecutive pair of
    // points in every edge's `path`, checks whether the segment crosses
    // each boundary line (a sign change in the perpendicular
    // coordinate -- `p[1]-world_z0`/`p[1]-world_z1` for the two horizontal
    // boundaries, `p[0]-world_x0`/`p[0]-world_x1` for the two vertical
    // ones), and if the crossing point also falls within that edge's own
    // finite span (not just the infinite line), computes `EdgeCrossing::
    // position` as its fractional 0..1 distance along the edge --
    // west-to-east for N/S, north-to-south for E/W, matching
    // `EdgeCrossing::position`'s own documented convention exactly. A
    // segment exactly collinear with a boundary (both endpoints at the
    // same perpendicular coordinate) registers no crossing -- an accepted,
    // undocumented-elsewhere-either edge case, not a new special
    // convention. This is new computational-geometry work for this
    // codebase (segment/tile-edge intersection); `Country::territory`'s
    // own doc comment (Countries.hpp, M139) explicitly deferred "tracing
    // an ordered polygon outline" as work with "no current consumer to
    // justify it yet" -- this is a narrower problem (line/line
    // intersection, not full polygon tracing) arriving because a real
    // consumer (this task) now exists.
    //
    // No-op if `world_x1 <= world_x0` or `world_z1 <= world_z0`
    // (degenerate bounds) -- mirrors `build()`'s own malformed-bounds
    // guard.
    static void exportCrossings(const RoadNetwork& net,
                                 double world_x0, double world_z0,
                                 double world_x1, double world_z1,
                                 std::array<TileEdge, 4>& edges);
};

} // namespace MeshWorld::Map
