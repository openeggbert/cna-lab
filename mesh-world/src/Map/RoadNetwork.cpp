// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Map/RoadNetwork.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <queue>
#include <vector>

namespace MeshWorld::Map {

namespace {

// Same 8-directional step shape Countries::grow() (M139) already uses.
constexpr int DX[8] = {0, 0, -1, 1, -1, 1, -1, 1};
constexpr int DY[8] = {-1, 1, 0, 0, -1, -1, 1, 1};

struct CandidateEdge {
    double dist;
    int    a;
    int    b;
};

// Union-find with path halving, same standard structure Kruskal's algorithm
// always pairs with -- no other generator in this codebase needed one yet
// (grow()'s flood uses a plain claimed[] array, not disjoint sets).
struct UnionFind {
    std::vector<int> parent;

    explicit UnionFind(std::size_t n) : parent(n) {
        for (std::size_t i = 0; i < n; ++i) parent[i] = static_cast<int>(i);
    }

    int find(int x) {
        while (parent[static_cast<std::size_t>(x)] != x) {
            parent[static_cast<std::size_t>(x)] = parent[static_cast<std::size_t>(parent[static_cast<std::size_t>(x)])];
            x = parent[static_cast<std::size_t>(x)];
        }
        return x;
    }

    // Returns true if `x` and `y` were in different sets (and are now
    // merged); false if they were already in the same set.
    bool unite(int x, int y) {
        x = find(x);
        y = find(y);
        if (x == y) return false;
        parent[static_cast<std::size_t>(x)] = y;
        return true;
    }
};

// M144 -- marks every grid cell touched by a traced river, the same
// point-rasterizing technique Countries.cpp's build_natural_feature_grid()
// (M140) already established (that function is private to Countries.cpp,
// so this mirrors its approach for rivers only rather than sharing code --
// M144 has no use for ridge/coastal membership, only rivers).
std::vector<char> mark_river_cells(const FieldGrid& elevation, double world_x0, double world_z0,
                                    double cell_w, double cell_h,
                                    const HydrologyNetwork* hydrology) {
    const int W = elevation.w;
    const int H = elevation.h;
    std::vector<char> is_river(static_cast<std::size_t>(W) * static_cast<std::size_t>(H), 0);
    if (hydrology == nullptr) return is_river;

    for (const RiverSegment& seg : hydrology->rivers) {
        for (const RiverPoint& p : seg.points) {
            const int gx = static_cast<int>((p.x - world_x0) / cell_w);
            const int gy = static_cast<int>((p.z - world_z0) / cell_h);
            if (gx < 0 || gx >= W || gy < 0 || gy >= H) continue;  // outside this grid
            is_river[static_cast<std::size_t>(gy) * static_cast<std::size_t>(W) + static_cast<std::size_t>(gx)] = 1;
        }
    }
    return is_river;
}

struct RouteEntry {
    double cost;
    int    cell_idx;
};

struct RouteEntryGreater {
    bool operator()(const RouteEntry& a, const RouteEntry& b) const { return a.cost > b.cost; }
};

// M144 -- point-to-point Dijkstra from (from_x, from_z) to (to_x, to_z)
// over `elevation`'s grid: a priority queue of frontier cells, the same
// shape Countries::grow()'s (M139) priority-flood already uses, but a
// single-target shortest path (early-exits the moment the target cell is
// popped) rather than a multi-source flood claiming the whole grid.
// Returns an empty path if the target is unreached (should not happen on a
// normal grid -- every step only costs more, never blocks outright -- kept
// as a defensive fallback the caller uses to keep M143's straight line
// instead of leaving a broken edge).
std::vector<std::array<double, 2>> route_between(const FieldGrid& elevation, double world_x0,
                                                  double world_z0, double cell_w, double cell_h,
                                                  const std::vector<char>& is_river,
                                                  double steep_slope_threshold_m,
                                                  double steep_slope_cost_multiplier,
                                                  double river_crossing_cost_multiplier,
                                                  double from_x, double from_z, double to_x,
                                                  double to_z) {
    const int W = elevation.w;
    const int H = elevation.h;
    const auto in_bounds = [&](int x, int y) { return x >= 0 && x < W && y >= 0 && y < H; };
    const auto world_x   = [&](int gx) { return world_x0 + (gx + 0.5) * cell_w; };
    const auto world_z   = [&](int gy) { return world_z0 + (gy + 0.5) * cell_h; };

    const int sx = std::clamp(static_cast<int>((from_x - world_x0) / cell_w), 0, W - 1);
    const int sy = std::clamp(static_cast<int>((from_z - world_z0) / cell_h), 0, H - 1);
    const int tx = std::clamp(static_cast<int>((to_x - world_x0) / cell_w), 0, W - 1);
    const int ty = std::clamp(static_cast<int>((to_z - world_z0) / cell_h), 0, H - 1);
    const int start_idx  = sy * W + sx;
    const int target_idx = ty * W + tx;

    std::vector<double> best_cost(static_cast<std::size_t>(W) * static_cast<std::size_t>(H),
                                   std::numeric_limits<double>::infinity());
    std::vector<int> came_from(static_cast<std::size_t>(W) * static_cast<std::size_t>(H), -1);
    best_cost[static_cast<std::size_t>(start_idx)] = 0.0;

    std::priority_queue<RouteEntry, std::vector<RouteEntry>, RouteEntryGreater> pq;
    pq.push({0.0, start_idx});
    bool reached = start_idx == target_idx;

    while (!pq.empty()) {
        const RouteEntry entry = pq.top();
        pq.pop();
        if (entry.cost > best_cost[static_cast<std::size_t>(entry.cell_idx)]) continue;  // stale
        if (entry.cell_idx == target_idx) {
            reached = true;
            break;
        }

        const int cx = entry.cell_idx % W;
        const int cy = entry.cell_idx / W;
        for (int k = 0; k < 8; ++k) {
            const int nx = cx + DX[k];
            const int ny = cy + DY[k];
            if (!in_bounds(nx, ny)) continue;
            const int nidx = ny * W + nx;

            double step_cost = std::hypot(DX[k] != 0 ? cell_w : 0.0, DY[k] != 0 ? cell_h : 0.0);
            const double elev_diff =
                std::abs(static_cast<double>(elevation.at(nx, ny)) - static_cast<double>(elevation.at(cx, cy)));
            if (elev_diff > steep_slope_threshold_m) step_cost *= steep_slope_cost_multiplier;
            if (is_river[static_cast<std::size_t>(nidx)] != 0) step_cost *= river_crossing_cost_multiplier;

            const double new_cost = entry.cost + step_cost;
            if (new_cost < best_cost[static_cast<std::size_t>(nidx)]) {
                best_cost[static_cast<std::size_t>(nidx)]  = new_cost;
                came_from[static_cast<std::size_t>(nidx)]  = entry.cell_idx;
                pq.push({new_cost, nidx});
            }
        }
    }

    if (!reached) return {};

    std::vector<int> cell_chain;
    for (int cur = target_idx; cur != start_idx; cur = came_from[static_cast<std::size_t>(cur)])
        cell_chain.push_back(cur);
    cell_chain.push_back(start_idx);
    std::reverse(cell_chain.begin(), cell_chain.end());

    std::vector<std::array<double, 2>> path;
    path.push_back({from_x, from_z});
    for (int idx : cell_chain) path.push_back({world_x(idx % W), world_z(idx / W)});
    path.push_back({to_x, to_z});
    return path;
}

// M145 -- checks one path segment (p0->p1) against a horizontal boundary
// line z=zc spanning x in [x_lo, x_hi]; appends a Road EdgeCrossing at
// `edge_idx` if the segment crosses it inside that span. `position` is
// measured west-to-east, matching EdgeCrossing::position's own convention.
void check_horizontal_boundary(int edge_idx, double zc, double x_lo, double x_hi,
                                const std::array<double, 2>& p0, const std::array<double, 2>& p1,
                                std::array<TileEdge, 4>& edges) {
    const double d0 = p0[1] - zc;
    const double d1 = p1[1] - zc;
    if ((d0 >= 0.0) == (d1 >= 0.0)) return;  // same side (or collinear): no crossing
    const double t    = d0 / (d0 - d1);
    const double x_at = p0[0] + t * (p1[0] - p0[0]);
    if (x_at < x_lo || x_at > x_hi) return;  // crosses the line, but off this tile's own edge span
    const double position = (x_at - x_lo) / (x_hi - x_lo);
    edges[static_cast<std::size_t>(edge_idx)].crossings.push_back(
        {EdgeCrossingType::Road, static_cast<float>(position)});
}

// Same as above but for a vertical boundary line x=xc spanning z in
// [z_lo, z_hi]; `position` measured north-to-south.
void check_vertical_boundary(int edge_idx, double xc, double z_lo, double z_hi,
                              const std::array<double, 2>& p0, const std::array<double, 2>& p1,
                              std::array<TileEdge, 4>& edges) {
    const double d0 = p0[0] - xc;
    const double d1 = p1[0] - xc;
    if ((d0 >= 0.0) == (d1 >= 0.0)) return;
    const double t    = d0 / (d0 - d1);
    const double z_at = p0[1] + t * (p1[1] - p0[1]);
    if (z_at < z_lo || z_at > z_hi) return;
    const double position = (z_at - z_lo) / (z_hi - z_lo);
    edges[static_cast<std::size_t>(edge_idx)].crossings.push_back(
        {EdgeCrossingType::Road, static_cast<float>(position)});
}

} // namespace

RoadNetwork Roads::build(const SettlementNetwork& settlements, SettlementTier max_tier,
                          int extra_redundant_links, const FieldGrid* elevation,
                          double world_x0, double world_z0, double world_x1, double world_z1,
                          const HydrologyNetwork* hydrology, double steep_slope_threshold_m,
                          double steep_slope_cost_multiplier,
                          double river_crossing_cost_multiplier) {
    RoadNetwork net;

    for (const Settlement& s : settlements.settlements)
        if (static_cast<int>(s.tier) <= static_cast<int>(max_tier)) net.nodes.push_back({s.x, s.z});

    const std::size_t n = net.nodes.size();
    if (n < 2) return net;

    std::vector<CandidateEdge> candidates;
    candidates.reserve(n * (n - 1) / 2);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i + 1; j < n; ++j) {
            const double dx = net.nodes[i].x - net.nodes[j].x;
            const double dz = net.nodes[i].z - net.nodes[j].z;
            candidates.push_back({std::hypot(dx, dz), static_cast<int>(i), static_cast<int>(j)});
        }
    }
    // Ties broken by (a, b) index rather than left to std::sort's
    // implementation-defined behavior on equal-weight edges, so the result
    // is deterministic across compilers/standard library implementations.
    std::sort(candidates.begin(), candidates.end(), [](const CandidateEdge& p, const CandidateEdge& q) {
        if (p.dist != q.dist) return p.dist < q.dist;
        if (p.a != q.a) return p.a < q.a;
        return p.b < q.b;
    });

    const auto add_edge = [&](int a, int b) {
        RoadEdge e;
        e.from = a;
        e.to   = b;
        e.path = {{net.nodes[static_cast<std::size_t>(a)].x, net.nodes[static_cast<std::size_t>(a)].z},
                  {net.nodes[static_cast<std::size_t>(b)].x, net.nodes[static_cast<std::size_t>(b)].z}};
        net.edges.push_back(std::move(e));
    };

    UnionFind uf(n);
    int       redundant_added = 0;
    for (const CandidateEdge& c : candidates) {
        if (uf.unite(c.a, c.b)) {
            add_edge(c.a, c.b);  // spanning-tree edge
        } else if (redundant_added < extra_redundant_links) {
            add_edge(c.a, c.b);  // a-few-redundant-links edge
            ++redundant_added;
        }
    }

    // M144 -- `elevation == nullptr` (the default) skips routing entirely,
    // leaving M143's straight two-point `path` on every edge untouched.
    if (elevation == nullptr || elevation->empty()) return net;

    const int    W      = elevation->w;
    const int    H      = elevation->h;
    const double cell_w = (world_x1 - world_x0) / static_cast<double>(W);
    const double cell_h = (world_z1 - world_z0) / static_cast<double>(H);
    if (cell_w <= 0.0 || cell_h <= 0.0) return net;  // malformed bounds -- keep M143's paths

    const std::vector<char> is_river =
        mark_river_cells(*elevation, world_x0, world_z0, cell_w, cell_h, hydrology);

    for (RoadEdge& e : net.edges) {
        const RoadNode& a = net.nodes[static_cast<std::size_t>(e.from)];
        const RoadNode& b = net.nodes[static_cast<std::size_t>(e.to)];
        std::vector<std::array<double, 2>> routed =
            route_between(*elevation, world_x0, world_z0, cell_w, cell_h, is_river,
                          steep_slope_threshold_m, steep_slope_cost_multiplier,
                          river_crossing_cost_multiplier, a.x, a.z, b.x, b.z);
        if (!routed.empty()) e.path = std::move(routed);  // else: keep M143's straight line
    }

    return net;
}

void Roads::exportCrossings(const RoadNetwork& net, double world_x0, double world_z0,
                             double world_x1, double world_z1, std::array<TileEdge, 4>& edges) {
    if (world_x1 <= world_x0 || world_z1 <= world_z0) return;  // degenerate bounds

    for (const RoadEdge& e : net.edges) {
        for (std::size_t i = 0; i + 1 < e.path.size(); ++i) {
            const std::array<double, 2>& p0 = e.path[i];
            const std::array<double, 2>& p1 = e.path[i + 1];
            check_horizontal_boundary(0, world_z0, world_x0, world_x1, p0, p1, edges);  // N
            check_horizontal_boundary(2, world_z1, world_x0, world_x1, p0, p1, edges);  // S
            check_vertical_boundary(3, world_x0, world_z0, world_z1, p0, p1, edges);    // W
            check_vertical_boundary(1, world_x1, world_z0, world_z1, p0, p1, edges);    // E
        }
    }
}

} // namespace MeshWorld::Map
