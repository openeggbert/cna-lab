// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Map/Hydrology.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>

namespace MeshWorld::Map {

namespace {
// Cardinal directions first, diagonals last: on an elevation tie, descent
// prefers an axis-aligned step over a diagonal one. Without this, a
// perfectly flat (or ramp-like) run of terrain lets steepest-descent drift
// diagonally between adjacent rows/columns on every tied step, artificially
// merging parallel features (e.g. two side-by-side ramps down to the same
// coastline) that should stay independent.
constexpr int DX[8] = {0, 0, -1, 1, -1, 1, -1, 1};
constexpr int DY[8] = {-1, 1, 0, 0, -1, -1, 1, 1};

// M-fix — an endorheic (no-outlet) basin can legitimately be sizable, but
// without a cap, a broad, gently-undulating landlocked area (e.g. a near-flat
// tundra plain) floods almost the ENTIRE tile into one "lake" -- confirmed on
// a real 100%-land map tile (6 separate lake names, land_ratio 100%). A pit
// deep in the interior with a real but far-off outlet looks identical, from
// this tile's own local view, to one with no outlet at all -- unlike M345's
// exits_tile fix, which only fires when the dead end sits ON the tile's own
// edge. Bound the FLOODED SET (not the search itself, which still finds the
// true spill elevation by continuing to expand) at a fraction of the grid;
// cells are popped in ascending-elevation order, so what's kept is the
// lowest, most physically plausible portion of the basin, not an arbitrary
// truncation. The absolute floor keeps every existing small synthetic test
// grid (down to HydrologyTests.cpp's 3x3 fully-enclosed fixture) behaving
// exactly as before -- this is a map-scale rendering consideration, not a
// correctness fix to the flood-fill algorithm itself.
constexpr double kMaxBasinAreaFraction = 0.12;
constexpr int    kMinBasinCellFloor    = 50;

// M123 — priority-flood / watershed fill starting from a landlocked pit
// (elevation.at(pit_x, pit_y)), stopping as soon as the flood reaches an
// ocean cell (elevation <= sea_level_m) or exhausts the whole grid (a fully
// enclosed endorheic basin with no ocean outlet at all). The cells popped
// from the min-priority-queue come out in non-decreasing elevation order,
// so the running max of popped elevations is exactly the basin's true
// spill elevation — the lowest ridge point with a path out.
Lake fill_basin(const FieldGrid& elevation, double sea_level_m, int pit_x, int pit_y,
                 const std::function<double(int)>& world_x,
                 const std::function<double(int)>& world_z) {
    const int W = elevation.w;
    const int H = elevation.h;
    const auto in_bounds = [&](int x, int y) { return x >= 0 && x < W && y >= 0 && y < H; };
    const auto idx       = [&](int x, int y) { return y * W + x; };

    using Cell = std::pair<float, int>;  // (elevation, flat index)
    std::priority_queue<Cell, std::vector<Cell>, std::greater<>> pq;
    std::vector<char> queued(static_cast<std::size_t>(W) * H, 0);

    const int pit_idx = idx(pit_x, pit_y);
    queued[static_cast<std::size_t>(pit_idx)] = 1;
    pq.push({elevation.at(pit_x, pit_y), pit_idx});

    std::vector<int> basin_cells;  // confirmed-underwater cells, in pop order
    double spill_elevation = elevation.at(pit_x, pit_y);
    const int max_basin_cells = std::max(kMinBasinCellFloor,
        static_cast<int>(kMaxBasinAreaFraction * static_cast<double>(W) * static_cast<double>(H)));

    while (!pq.empty()) {
        const auto [e, id] = pq.top();
        pq.pop();
        spill_elevation = std::max(spill_elevation, static_cast<double>(e));

        if (e <= sea_level_m) break;  // found an outlet to the ocean

        // Cap which cells count as "flooded" (see kMaxBasinAreaFraction's own
        // comment above) -- the search itself still expands past the cap so
        // spill_elevation stays the true physical value.
        if (static_cast<int>(basin_cells.size()) < max_basin_cells) basin_cells.push_back(id);
        const int cx = id % W;
        const int cy = id / W;
        for (int k = 0; k < 8; ++k) {
            const int nx = cx + DX[k];
            const int ny = cy + DY[k];
            if (!in_bounds(nx, ny)) continue;
            const int nidx = idx(nx, ny);
            if (queued[static_cast<std::size_t>(nidx)]) continue;
            queued[static_cast<std::size_t>(nidx)] = 1;
            pq.push({elevation.at(nx, ny), nidx});
        }
    }
    // If the queue emptied without finding an outlet, the whole reachable
    // region is a fully enclosed endorheic basin — spill_elevation is then
    // just the highest point in that region, which is the correct "surface
    // elevation" for a basin that never overflows.

    Lake lake;
    double sum_x = 0.0, sum_z = 0.0;
    lake.shoreline.reserve(basin_cells.size());
    for (int id : basin_cells) {
        const int cx = id % W;
        const int cy = id / W;
        const double wx = world_x(cx);
        const double wz = world_z(cy);
        lake.shoreline.push_back({wx, wz});
        sum_x += wx;
        sum_z += wz;
    }
    if (!basin_cells.empty()) {
        lake.x = sum_x / static_cast<double>(basin_cells.size());
        lake.z = sum_z / static_cast<double>(basin_cells.size());
    } else {
        // Extremely small/degenerate grid: pit itself had no room to flood.
        lake.x = world_x(pit_x);
        lake.z = world_z(pit_y);
    }
    lake.surface_elevation_m = spill_elevation;
    return lake;
}

} // namespace

HydrologyNetwork Hydrology::trace(const FieldGrid& elevation, double sea_level_m,
                                   double world_x0, double world_z0,
                                   double world_x1, double world_z1) {
    HydrologyNetwork net;
    if (elevation.empty()) return net;

    const int W = elevation.w;
    const int H = elevation.h;
    const double cell_w = (world_x1 - world_x0) / static_cast<double>(W);
    const double cell_h = (world_z1 - world_z0) / static_cast<double>(H);

    const std::function<double(int)> world_x = [=](int gx) { return world_x0 + (gx + 0.5) * cell_w; };
    const std::function<double(int)> world_z = [=](int gy) { return world_z0 + (gy + 0.5) * cell_h; };
    const auto in_bounds = [&](int x, int y) { return x >= 0 && x < W && y >= 0 && y < H; };

    // Sources: land-cell local maxima (>= every in-bounds neighbor; ties
    // count, so a flat plateau produces one source per plateau cell).
    std::vector<std::pair<int, int>> sources;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const float e = elevation.at(x, y);
            if (e <= sea_level_m) continue;
            bool is_max = true;
            for (int k = 0; k < 8 && is_max; ++k) {
                const int nx = x + DX[k];
                const int ny = y + DY[k];
                if (in_bounds(nx, ny) && elevation.at(nx, ny) > e) is_max = false;
            }
            if (is_max) sources.emplace_back(x, y);
        }
    }

    // Dedupes basin fills: multiple segments dead-ending at the same pit
    // share one Lake (keyed by the pit's flat grid index).
    std::unordered_map<int, int> pit_to_lake;

    // M124 — confluence detection. `owner[cell]` names the segment/point
    // that "owns" a cell once some earlier-processed source's trace has
    // confirmed flow continues past it (i.e. the cell is not that
    // segment's own terminus — see the interior-only registration below).
    // Because steepest descent is a pure function of the elevation grid,
    // two traces that ever reach the same cell take an identical route
    // from there on; a later trace stepping onto an already-owned cell
    // stops and defers to the existing owner as its `downstream_segment`
    // instead of re-tracing (and duplicating) the same downstream polyline.
    // Terminal cells (ocean mouths, pits) are deliberately never
    // registered, so two segments independently reaching the same mouth or
    // pit still go through the existing reaches_ocean/pit_to_lake sharing
    // above, unchanged from M122/M123.
    struct Owner {
        int segment;
        int point;
    };
    std::unordered_map<int, Owner> owner;
    // merge_point[i]: the point index within net.rivers[i]'s
    // downstream_segment where segment i joined, so a confluence further
    // upstream can propagate its flow through the whole downstream chain.
    // -1 if segment i has no downstream confluence (ocean/lake terminus).
    std::vector<int> merge_point(sources.size(), -1);

    net.rivers.reserve(sources.size());
    for (std::size_t src_i = 0; src_i < sources.size(); ++src_i) {
        const auto [sx, sy] = sources[src_i];
        const int seg_idx   = static_cast<int>(src_i);  // one segment pushed per source, in order
        RiverSegment seg;
        int   x    = sx;
        int   y    = sy;
        float flow = 0.0f;

        // Elevation strictly decreases every step (steepest descent to a
        // strictly lower neighbor), so this always terminates and never
        // revisits a cell. A source (local max) can never itself be an
        // already-owned cell: owning requires a strictly-higher neighbor to
        // have descended into it, which contradicts being a local max.
        for (;;) {
            const int   cell_idx = y * W + x;
            const float e        = elevation.at(x, y);
            flow += 1.0f;
            seg.points.push_back(RiverPoint{world_x(x), world_z(y), flow});

            const auto owner_it = owner.find(cell_idx);
            if (owner_it != owner.end()) {
                seg.downstream_segment = owner_it->second.segment;
                merge_point[src_i]     = owner_it->second.point;

                // Propagate this tributary's flow-at-merge down the whole
                // downstream chain (the owner may itself already be a
                // tributary of something further down).
                int   target_seg = owner_it->second.segment;
                int   target_pt  = owner_it->second.point;
                const float amount = flow;
                for (;;) {
                    RiverSegment& tseg = net.rivers[static_cast<std::size_t>(target_seg)];
                    for (std::size_t p = static_cast<std::size_t>(target_pt); p < tseg.points.size(); ++p)
                        tseg.points[p].flow += amount;
                    if (merge_point[static_cast<std::size_t>(target_seg)] < 0) break;
                    const int next_pt  = merge_point[static_cast<std::size_t>(target_seg)];
                    target_seg         = tseg.downstream_segment;
                    target_pt          = next_pt;
                }
                break;
            }

            if (e <= sea_level_m) {
                seg.reaches_ocean = true;
                break;
            }

            int   best_x = -1;
            int   best_y = -1;
            float best_e = e;
            for (int k = 0; k < 8; ++k) {
                const int nx = x + DX[k];
                const int ny = y + DY[k];
                if (!in_bounds(nx, ny)) continue;
                const float ne = elevation.at(nx, ny);
                if (ne < best_e) {
                    best_e = ne;
                    best_x = nx;
                    best_y = ny;
                }
            }
            if (best_x >= 0) {
                // Flow continues past this cell (it's interior, not a
                // terminus): register it so a later trace reaching it
                // merges here instead of duplicating the rest of the path.
                owner[cell_idx] = Owner{seg_idx, static_cast<int>(seg.points.size()) - 1};
                x = best_x;
                y = best_y;
                continue;
            }

            // M345 (MAP22) — a cell on this tile's own OUTER row/column with
            // no in-bounds lower neighbor previously got basin-filled into a
            // Lake here unconditionally, as if the water were genuinely
            // trapped -- wrong whenever the real terrain just beyond this
            // tile's own edge (invisible to this trace, which only ever
            // sees its own W x H grid) keeps sloping downward, the common
            // case for a river flowing across a tile boundary rather than
            // coincidentally hitting a matching local minimum right at the
            // seam. Marks this segment as exiting the tile instead of
            // damming it into a spurious lake; see RiverSegment::
            // exits_tile's own doc comment and Hydrology::exportCrossings().
            if (x == 0 || x == W - 1 || y == 0 || y == H - 1) {
                seg.exits_tile = true;
                break;
            }

            // Landlocked local minimum (M123): fill the basin into a Lake,
            // shared across every segment that dead-ends at this same pit.
            const int pit_idx = y * W + x;
            auto      it       = pit_to_lake.find(pit_idx);
            if (it == pit_to_lake.end()) {
                net.lakes.push_back(fill_basin(elevation, sea_level_m, x, y, world_x, world_z));
                it = pit_to_lake.emplace(pit_idx, static_cast<int>(net.lakes.size()) - 1).first;
            }
            seg.lake_index = it->second;
            break;
        }

        net.rivers.push_back(std::move(seg));
    }

    return net;
}

void Hydrology::exportCrossings(const HydrologyNetwork& network, int grid_w, int grid_h,
                                 double world_x0, double world_z0,
                                 double world_x1, double world_z1,
                                 std::array<TileEdge, 4>& edges) {
    if (network.rivers.empty() || grid_w <= 0 || grid_h <= 0) return;
    const double cell_w = (world_x1 - world_x0) / static_cast<double>(grid_w);
    const double cell_h = (world_z1 - world_z0) / static_cast<double>(grid_h);
    if (cell_w <= 0.0 || cell_h <= 0.0) return;

    for (const RiverSegment& seg : network.rivers) {
        if (!seg.exits_tile || seg.points.empty()) continue;
        const RiverPoint& p = seg.points.back();
        // Inverse of trace()'s own world_x(gx) = world_x0 + (gx+0.5)*cell_w.
        const int gx = static_cast<int>(std::lround((p.x - world_x0) / cell_w - 0.5));
        const int gy = static_cast<int>(std::lround((p.z - world_z0) / cell_h - 0.5));
        if (gx < 0 || gx >= grid_w || gy < 0 || gy >= grid_h) continue;  // shouldn't happen; be defensive

        if (gy == 0)
            edges[0].crossings.push_back({EdgeCrossingType::River,
                                           static_cast<float>((gx + 0.5) / grid_w)});
        if (gy == grid_h - 1)
            edges[2].crossings.push_back({EdgeCrossingType::River,
                                           static_cast<float>((gx + 0.5) / grid_w)});
        if (gx == 0)
            edges[3].crossings.push_back({EdgeCrossingType::River,
                                           static_cast<float>((gy + 0.5) / grid_h)});
        if (gx == grid_w - 1)
            edges[1].crossings.push_back({EdgeCrossingType::River,
                                           static_cast<float>((gy + 0.5) / grid_h)});
    }
}

namespace {
// M125 carve tuning: depth and radius both grow with sqrt(flow) (a
// diminishing-returns curve — a river needs to grow a lot bigger to get much
// deeper/wider), capped so a single very-high-flow mouth can't carve an
// unreasonably large crater.
constexpr double kCarveBaseDepthM   = 2.0;
constexpr double kCarveDepthPerFlow = 3.0;
constexpr double kCarveMaxDepthM    = 120.0;
constexpr double kCarveBaseRadius   = 1.0;
constexpr double kCarveRadiusPerFlow = 0.5;
constexpr double kCarveMaxRadius    = 6.0;
} // namespace

void Hydrology::carve(FieldGrid& elevation, const HydrologyNetwork& network, double sea_level_m,
                       double world_x0, double world_z0, double world_x1, double world_z1) {
    if (elevation.empty() || network.rivers.empty()) return;

    const int W = elevation.w;
    const int H = elevation.h;
    if (W < 3 || H < 3) return;  // no interior cells to carve without touching an edge

    const double cell_w = (world_x1 - world_x0) / static_cast<double>(W);
    const double cell_h = (world_z1 - world_z0) / static_cast<double>(H);
    if (cell_w <= 0.0 || cell_h <= 0.0) return;

    // Largest reduction requested per cell, accumulated across every river
    // point whose falloff reaches it; applied once at the end so overlapping
    // falloffs (e.g. near a confluence) take the max, not the sum.
    std::vector<float> reduction(static_cast<std::size_t>(W) * static_cast<std::size_t>(H), 0.0f);

    for (const RiverSegment& seg : network.rivers) {
        for (const RiverPoint& p : seg.points) {
            const double fgx = (p.x - world_x0) / cell_w - 0.5;
            const double fgy = (p.z - world_z0) / cell_h - 0.5;
            const int    gx  = static_cast<int>(std::lround(fgx));
            const int    gy  = static_cast<int>(std::lround(fgy));
            if (gx < 0 || gx >= W || gy < 0 || gy >= H) continue;  // shouldn't happen; be defensive

            const double flow   = static_cast<double>(p.flow);
            const double depth  = std::min(kCarveMaxDepthM, kCarveBaseDepthM + kCarveDepthPerFlow * std::sqrt(flow));
            const double radius = std::min(kCarveMaxRadius, kCarveBaseRadius + kCarveRadiusPerFlow * std::sqrt(flow));
            const int    ir     = static_cast<int>(std::ceil(radius));

            for (int dy = -ir; dy <= ir; ++dy) {
                const int ny = gy + dy;
                if (ny <= 0 || ny >= H - 1) continue;  // never touch the edge rows
                for (int dx = -ir; dx <= ir; ++dx) {
                    const int nx = gx + dx;
                    if (nx <= 0 || nx >= W - 1) continue;  // never touch the edge columns

                    const double dist = std::sqrt(static_cast<double>(dx * dx + dy * dy));
                    if (dist > radius) continue;
                    const double falloff = 1.0 - dist / (radius + 1.0);  // >0 within radius, 1 at center
                    const float  amount  = static_cast<float>(depth * falloff);

                    const std::size_t idx = static_cast<std::size_t>(ny) * static_cast<std::size_t>(W)
                                            + static_cast<std::size_t>(nx);
                    reduction[idx] = std::max(reduction[idx], amount);
                }
            }
        }
    }

    for (int gy = 1; gy < H - 1; ++gy) {
        for (int gx = 1; gx < W - 1; ++gx) {
            const std::size_t idx = static_cast<std::size_t>(gy) * static_cast<std::size_t>(W)
                                    + static_cast<std::size_t>(gx);
            if (reduction[idx] <= 0.0f) continue;
            const float original = elevation.data[idx];
            float       carved   = original - reduction[idx];
            // Rivers cut valleys into land, not the ocean floor: never push a
            // cell that started above sea level down to or below it here.
            if (original > static_cast<float>(sea_level_m))
                carved = std::max(carved, static_cast<float>(sea_level_m) + 1e-3f);
            elevation.data[idx] = carved;
        }
    }
}

} // namespace MeshWorld::Map
