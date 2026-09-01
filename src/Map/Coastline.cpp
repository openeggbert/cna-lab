// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Map/Coastline.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <unordered_map>
#include <utility>

namespace MeshWorld::Map {

namespace {

// Matches Countries.cpp's own kBorderEdgeEpsM exactly, same reason: tile
// bounds are half-open [world_x0,world_x1)x[world_z0,world_z1) --
// MapValidator rejects a point sitting exactly on the max edge.
constexpr double kCoastlineEdgeEpsM = 1e-6;

// Chaikin's corner-cutting: each round replaces every edge (P0,P1) with two
// points at 1/4 and 3/4 along it, cutting the corner at their shared
// vertex. `closed` polylines are treated cyclically (first==last on entry
// and exit); open polylines keep their two true endpoints fixed across
// every round (the standard open-curve variant) since those endpoints are
// this tile's own honest trace boundary, not an artifact to smooth away.
std::vector<std::array<double, 2>> chaikin_smooth(
    std::vector<std::array<double, 2>> pts, bool closed, int iterations) {
    for (int it = 0; it < iterations; ++it) {
        if (pts.size() < 3) break;
        std::vector<std::array<double, 2>> next;
        if (closed) {
            const std::size_t n = pts.size() - 1;  // pts.front() == pts.back()
            next.reserve(n * 2 + 1);
            for (std::size_t i = 0; i < n; ++i) {
                const auto& p0 = pts[i];
                const auto& p1 = pts[(i + 1) % n];
                next.push_back({p0[0] * 0.75 + p1[0] * 0.25, p0[1] * 0.75 + p1[1] * 0.25});
                next.push_back({p0[0] * 0.25 + p1[0] * 0.75, p0[1] * 0.25 + p1[1] * 0.75});
            }
            next.push_back(next.front());
        } else {
            next.reserve((pts.size() - 1) * 2);
            next.push_back(pts.front());
            for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
                const auto& p0 = pts[i];
                const auto& p1 = pts[i + 1];
                next.push_back({p0[0] * 0.75 + p1[0] * 0.25, p0[1] * 0.75 + p1[1] * 0.25});
                next.push_back({p0[0] * 0.25 + p1[0] * 0.75, p0[1] * 0.25 + p1[1] * 0.75});
            }
            next.push_back(pts.back());
        }
        pts = std::move(next);
    }
    return pts;
}

} // namespace

std::vector<std::vector<std::array<double, 2>>> Coastline::trace(
    const FieldGrid& elevation, double sea_level_m,
    double world_x0, double world_z0, double world_x1, double world_z1,
    int smoothing_iterations) {
    std::vector<std::vector<std::array<double, 2>>> result;
    if (elevation.empty()) return result;

    const int    W      = elevation.w;
    const int    H      = elevation.h;
    const double cell_w = (world_x1 - world_x0) / static_cast<double>(W);
    const double cell_h = (world_z1 - world_z0) / static_cast<double>(H);
    if (cell_w <= 0.0 || cell_h <= 0.0) return result;

    const auto is_ocean = [&](int gx, int gy) {
        return elevation.at(gx, gy) < static_cast<float>(sea_level_m);
    };
    const auto corner_key = [W](int gx, int gy) -> std::int64_t {
        return static_cast<std::int64_t>(gy) * (W + 1) + gx;
    };

    // Boundary edges between an ocean cell and a land cell, one entry per
    // INTERNAL cell-pair only (deliberately no off-grid comparison at
    // all -- unlike Countries.cpp's own trace_owner_border(), which treats
    // off-grid as "unowned" so a territory reaching the tile edge still
    // closes into a loop there. That convention is right for a country's
    // OWN territory view, but wrong here: assuming "off-grid = ocean"
    // would draw a bogus coastline ringing the ENTIRE perimeter of a tile
    // that's genuinely all-land (a real, visible artifact), and assuming
    // "off-grid = land" would do the same for an all-ocean tile. Checking
    // only South and East from every cell covers every internal cell-pair
    // exactly once, with no double-counting from the neighbor's own
    // North/West check of the same pair.
    std::unordered_map<std::int64_t, std::vector<std::int64_t>> adj;
    std::map<std::pair<std::int64_t, std::int64_t>, int>        edge_remaining;
    const auto add_edge = [&](std::int64_t a, std::int64_t b) {
        adj[a].push_back(b);
        adj[b].push_back(a);
        ++edge_remaining[std::minmax(a, b)];
    };
    for (int cy = 0; cy < H; ++cy) {
        for (int cx = 0; cx < W; ++cx) {
            const bool ocean_here = is_ocean(cx, cy);
            if (cy + 1 < H && is_ocean(cx, cy + 1) != ocean_here)
                add_edge(corner_key(cx, cy + 1), corner_key(cx + 1, cy + 1));
            if (cx + 1 < W && is_ocean(cx + 1, cy) != ocean_here)
                add_edge(corner_key(cx + 1, cy), corner_key(cx + 1, cy + 1));
        }
    }
    if (edge_remaining.empty()) return result;

    const auto has_edge = [&](std::int64_t a, std::int64_t b) {
        auto it = edge_remaining.find(std::minmax(a, b));
        return it != edge_remaining.end() && it->second > 0;
    };
    const auto use_edge = [&](std::int64_t a, std::int64_t b) { --edge_remaining[std::minmax(a, b)]; };
    const auto remaining_degree = [&](std::int64_t v) {
        int d = 0;
        for (std::int64_t n : adj[v])
            if (has_edge(v, n)) ++d;
        return d;
    };
    const auto walk_from = [&](std::int64_t start) {
        std::vector<std::int64_t> path{start};
        std::int64_t cur = start;
        while (true) {
            std::int64_t next = -1;
            for (std::int64_t cand : adj[cur]) {
                if (has_edge(cur, cand)) { next = cand; break; }
            }
            if (next < 0) break;
            use_edge(cur, next);
            path.push_back(next);
            cur = next;
            if (cur == start) break;  // closed back on itself
        }
        return path;
    };

    // Pass 1: open paths -- a coastline segment that runs off the tile
    // edge leaves exactly two corners with an odd remaining degree (the
    // handshake lemma guarantees an even total count of them), so walking
    // from each one, repeatedly, drains every such segment cleanly before
    // pass 2 ever runs.
    std::vector<std::pair<std::vector<std::int64_t>, bool>> raw_paths;  // (corner keys, is_closed)
    std::vector<std::int64_t> odd_corners;
    for (const auto& [corner, neighbors] : adj) {
        (void)neighbors;
        if (remaining_degree(corner) % 2 != 0) odd_corners.push_back(corner);
    }
    std::sort(odd_corners.begin(), odd_corners.end());  // deterministic order
    for (std::int64_t start : odd_corners) {
        while (remaining_degree(start) > 0) {
            auto path = walk_from(start);
            if (path.size() >= 2) raw_paths.emplace_back(std::move(path), false);
        }
    }

    // Pass 2: everything left now has purely even degree -- closed loops
    // only (islands, or any coastline fully enclosed within this tile).
    for (const auto& [key, count] : edge_remaining) {
        (void)count;
        while (edge_remaining[key] > 0) {
            auto path = walk_from(key.first);
            if (path.size() >= 4 && path.front() == path.back())
                raw_paths.emplace_back(std::move(path), true);
            else
                break;  // malformed/degenerate remainder; don't loop forever
        }
    }

    const auto to_world = [&](std::int64_t key) -> std::array<double, 2> {
        const int    gx = static_cast<int>(key % (W + 1));
        const int    gy = static_cast<int>(key / (W + 1));
        const double x  = (gx >= W) ? (world_x0 + W * cell_w - kCoastlineEdgeEpsM) : (world_x0 + gx * cell_w);
        const double z  = (gy >= H) ? (world_z0 + H * cell_h - kCoastlineEdgeEpsM) : (world_z0 + gy * cell_h);
        return {x, z};
    };

    result.reserve(raw_paths.size());
    for (const auto& [keys, closed] : raw_paths) {
        std::vector<std::array<double, 2>> pts;
        pts.reserve(keys.size());
        for (std::int64_t k : keys) pts.push_back(to_world(k));
        result.push_back(chaikin_smooth(std::move(pts), closed, smoothing_iterations));
    }
    return result;
}

} // namespace MeshWorld::Map
