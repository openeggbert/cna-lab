// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "Map/Countries.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <map>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Map/Noise.hpp"
#include "NameGenerator.hpp"
#include "Naming.hpp"

namespace MeshWorld::Map {

namespace {
// Arbitrary, distinct hash2i axis so country-naming seeds never collide
// with unrelated hash2i(...) callers (FeatureNaming.cpp uses 601-603,
// MountainRanges.cpp uses 401/900+j).
constexpr std::int64_t kCountryNameAxis = 801;

constexpr int DX[8] = {0, 0, -1, 1, -1, 1, -1, 1};
constexpr int DY[8] = {-1, 1, 0, 0, -1, -1, 1, 1};

struct QueueEntry {
    double cost;
    int    cell_idx;
    int    owner;  // index into CountryNetwork::countries
};

struct QueueEntryGreater {
    bool operator()(const QueueEntry& a, const QueueEntry& b) const { return a.cost > b.cost; }
};

// True if any cell within radius_cells (Chebyshev) is ocean -- same check
// as BiomeRefinement::applyCoastalBeach() (M128), reused here (that
// function's own implementation is private to BiomeRefinement.cpp, so this
// mirrors its formula rather than sharing code, the same way
// Settlements.cpp already does for its own coastal/flatness checks).
bool near_water(const FieldGrid& elevation, double sea_level_m, int gx, int gy, int radius_cells) {
    const int W = elevation.w;
    const int H = elevation.h;
    for (int dy = -radius_cells; dy <= radius_cells; ++dy) {
        const int ny = gy + dy;
        if (ny < 0 || ny >= H) continue;
        for (int dx = -radius_cells; dx <= radius_cells; ++dx) {
            const int nx = gx + dx;
            if (nx < 0 || nx >= W) continue;
            if (elevation.at(nx, ny) <= static_cast<float>(sea_level_m)) return true;
        }
    }
    return false;
}

// M140 — marks every grid cell that counts as a "natural feature" (river,
// ridge, or coastline) for Countries::grow()'s cost model. `hydrology`/
// `mountains` may be null (meaning that source contributes nothing, not an
// error -- callers that don't have one yet, or don't want it considered,
// simply omit it).
std::vector<char> build_natural_feature_grid(const FieldGrid& elevation, double sea_level_m,
                                              double world_x0, double world_z0,
                                              double cell_w, double cell_h,
                                              const HydrologyNetwork* hydrology,
                                              const MountainRangeNetwork* mountains,
                                              int coastal_radius_cells) {
    const int W = elevation.w;
    const int H = elevation.h;
    std::vector<char> is_feature(static_cast<std::size_t>(W) * static_cast<std::size_t>(H), 0);

    const auto mark_world_point = [&](double x, double z) {
        const int gx = static_cast<int>((x - world_x0) / cell_w);
        const int gy = static_cast<int>((z - world_z0) / cell_h);
        if (gx < 0 || gx >= W || gy < 0 || gy >= H) return;  // outside this grid; not our concern
        is_feature[static_cast<std::size_t>(gy) * static_cast<std::size_t>(W) + static_cast<std::size_t>(gx)] = 1;
    };

    if (hydrology != nullptr)
        for (const RiverSegment& seg : hydrology->rivers)
            for (const RiverPoint& p : seg.points) mark_world_point(p.x, p.z);

    if (mountains != nullptr)
        for (const MountainRange& range : mountains->ranges)
            for (const RidgePoint& p : range.ridge) mark_world_point(p.x, p.z);

    if (coastal_radius_cells > 0) {
        for (int gy = 0; gy < H; ++gy) {
            for (int gx = 0; gx < W; ++gx) {
                if (elevation.at(gx, gy) <= static_cast<float>(sea_level_m)) continue;  // ocean itself isn't "coastline"
                if (near_water(elevation, sea_level_m, gx, gy, coastal_radius_cells))
                    is_feature[static_cast<std::size_t>(gy) * static_cast<std::size_t>(W) + static_cast<std::size_t>(gx)] = 1;
            }
        }
    }

    return is_feature;
}

// Tile bounds are half-open [world_x0,world_x1)x[world_z0,world_z1) --
// MapValidator rejects a point at exactly the max edge (same bug class §5
// #18/#20/#21 fixed elsewhere this session). A traced corner that lands on
// grid column W or row H sits exactly on that max edge; nudge it 1e-6 m
// inside instead, matching country.lua's own established epsilon.
constexpr double kBorderEdgeEpsM = 1e-6;

// M139 follow-up -- traces owner's territory boundary from the claimed
// raster (`claimed[gy*W+gx]`, values as grow() defines them: an owner
// index, -1 unclaimed, or -2 ocean; any coordinate outside the grid counts
// as unowned too) into a single closed polygon loop of world-space
// grid-corner points, welded from individual boundary edges. Cell (cx,cy)'s
// 4 corners are grid points (cx,cy),(cx+1,cy),(cx,cy+1),(cx+1,cy+1); an edge
// between an owned cell and a non-owned neighbor is a boundary edge.
// Multiple disjoint loops are possible (e.g. a territory with a hole, or a
// pathological disconnected shape) -- only the loop with the most points is
// kept, the documented V1 simplification (see Countries.hpp's own doc
// comment on Country::border).
std::vector<std::array<double, 2>> trace_owner_border(
    const std::vector<int>& claimed, int W, int H, int owner,
    double world_x0, double world_z0, double cell_w, double cell_h) {
    const auto owner_at = [&](int cx, int cy) -> int {
        if (cx < 0 || cx >= W || cy < 0 || cy >= H) return -3;  // off-grid: unowned
        return claimed[static_cast<std::size_t>(cy) * static_cast<std::size_t>(W) +
                        static_cast<std::size_t>(cx)];
    };
    const auto corner_key = [W](int gx, int gy) -> std::int64_t {
        return static_cast<std::int64_t>(gy) * (W + 1) + gx;
    };

    std::unordered_map<std::int64_t, std::vector<std::int64_t>> adj;
    std::map<std::pair<std::int64_t, std::int64_t>, int>        edge_remaining;
    const auto add_edge = [&](std::int64_t a, std::int64_t b) {
        adj[a].push_back(b);
        adj[b].push_back(a);
        ++edge_remaining[std::minmax(a, b)];
    };

    for (int cy = 0; cy < H; ++cy) {
        for (int cx = 0; cx < W; ++cx) {
            if (owner_at(cx, cy) != owner) continue;
            if (owner_at(cx, cy - 1) != owner) add_edge(corner_key(cx, cy), corner_key(cx + 1, cy));      // N
            if (owner_at(cx, cy + 1) != owner) add_edge(corner_key(cx, cy + 1), corner_key(cx + 1, cy + 1)); // S
            if (owner_at(cx - 1, cy) != owner) add_edge(corner_key(cx, cy), corner_key(cx, cy + 1));      // W
            if (owner_at(cx + 1, cy) != owner) add_edge(corner_key(cx + 1, cy), corner_key(cx + 1, cy + 1)); // E
        }
    }
    if (edge_remaining.empty()) return {};

    const auto has_edge = [&](std::int64_t a, std::int64_t b) {
        auto it = edge_remaining.find(std::minmax(a, b));
        return it != edge_remaining.end() && it->second > 0;
    };
    const auto use_edge = [&](std::int64_t a, std::int64_t b) { --edge_remaining[std::minmax(a, b)]; };

    std::vector<std::int64_t> best_loop;
    for (const auto& [key, count] : edge_remaining) {
        while (edge_remaining[key] > 0) {
            const std::int64_t start = key.first;
            std::int64_t       cur   = key.first;
            std::int64_t       nxt   = key.second;
            std::vector<std::int64_t> loop{cur};
            use_edge(cur, nxt);
            cur = nxt;
            while (cur != start) {
                loop.push_back(cur);
                std::int64_t next = -1;
                for (std::int64_t cand : adj[cur]) {
                    if (has_edge(cur, cand)) { next = cand; break; }
                }
                if (next < 0) break;  // dead end -- shouldn't happen for a rasterized region's own boundary
                use_edge(cur, next);
                cur = next;
            }
            if (cur == start && loop.size() > best_loop.size()) best_loop = std::move(loop);
        }
    }
    if (best_loop.size() < 3) return {};

    std::vector<std::array<double, 2>> polygon;
    polygon.reserve(best_loop.size() + 1);
    for (std::int64_t key : best_loop) {
        const int gx = static_cast<int>(key % (W + 1));
        const int gy = static_cast<int>(key / (W + 1));
        const double x = (gx >= W) ? (world_x0 + W * cell_w - kBorderEdgeEpsM) : (world_x0 + gx * cell_w);
        const double z = (gy >= H) ? (world_z0 + H * cell_h - kBorderEdgeEpsM) : (world_z0 + gy * cell_h);
        polygon.push_back({x, z});
    }
    polygon.push_back(polygon.front());  // close the loop
    return polygon;
}

} // namespace

CountryNetwork Countries::grow(const SettlementNetwork& settlements,
                                const FieldGrid& elevation, double sea_level_m,
                                double world_x0, double world_z0,
                                double world_x1, double world_z1,
                                const HydrologyNetwork* hydrology,
                                const MountainRangeNetwork* mountains,
                                double natural_feature_cost_multiplier,
                                int coastal_radius_cells) {
    CountryNetwork net;
    if (elevation.empty()) return net;

    std::vector<std::size_t> capital_indices;
    for (std::size_t i = 0; i < settlements.settlements.size(); ++i)
        if (settlements.settlements[i].tier == SettlementTier::Capital) capital_indices.push_back(i);
    if (capital_indices.empty()) return net;

    const int    W      = elevation.w;
    const int    H      = elevation.h;
    const double cell_w = (world_x1 - world_x0) / static_cast<double>(W);
    const double cell_h = (world_z1 - world_z0) / static_cast<double>(H);
    if (cell_w <= 0.0 || cell_h <= 0.0) return net;

    const auto in_bounds = [&](int x, int y) { return x >= 0 && x < W && y >= 0 && y < H; };
    const auto world_x   = [&](int gx) { return world_x0 + (gx + 0.5) * cell_w; };
    const auto world_z   = [&](int gy) { return world_z0 + (gy + 0.5) * cell_h; };

    net.countries.resize(capital_indices.size());
    for (std::size_t i = 0; i < capital_indices.size(); ++i) {
        const Settlement& cap = settlements.settlements[capital_indices[i]];
        net.countries[i].capital_x = cap.x;
        net.countries[i].capital_z = cap.z;
    }

    // M140 — nullptr hydrology/mountains and coastal_radius_cells<=0 (via
    // the guard inside build_natural_feature_grid) leave every cell
    // unmarked, so the multiplier below never applies and this reduces to
    // exactly M139's original uniform-cost behavior.
    const std::vector<char> is_feature = build_natural_feature_grid(
        elevation, sea_level_m, world_x0, world_z0, cell_w, cell_h, hydrology, mountains,
        coastal_radius_cells);

    // -1 = unclaimed, -2 = ocean (absorbed, belongs to no country).
    std::vector<int> claimed(static_cast<std::size_t>(W) * static_cast<std::size_t>(H), -1);

    std::priority_queue<QueueEntry, std::vector<QueueEntry>, QueueEntryGreater> pq;
    for (std::size_t i = 0; i < capital_indices.size(); ++i) {
        const Settlement& cap = settlements.settlements[capital_indices[i]];
        const int gx = std::clamp(static_cast<int>((cap.x - world_x0) / cell_w), 0, W - 1);
        const int gy = std::clamp(static_cast<int>((cap.z - world_z0) / cell_h), 0, H - 1);
        pq.push({0.0, gy * W + gx, static_cast<int>(i)});
    }

    while (!pq.empty()) {
        const QueueEntry entry = pq.top();
        pq.pop();
        if (claimed[static_cast<std::size_t>(entry.cell_idx)] != -1) continue;  // already settled, cheaper claim first

        const int cx = entry.cell_idx % W;
        const int cy = entry.cell_idx / W;
        if (elevation.at(cx, cy) <= static_cast<float>(sea_level_m)) {
            claimed[static_cast<std::size_t>(entry.cell_idx)] = -2;  // ocean: absorbed, growth stops here
            continue;
        }

        claimed[static_cast<std::size_t>(entry.cell_idx)] = entry.owner;
        net.countries[static_cast<std::size_t>(entry.owner)].territory.push_back(
            {world_x(cx), world_z(cy)});

        for (int k = 0; k < 8; ++k) {
            const int nx = cx + DX[k];
            const int ny = cy + DY[k];
            if (!in_bounds(nx, ny)) continue;
            const int nidx = ny * W + nx;
            if (claimed[static_cast<std::size_t>(nidx)] != -1) continue;

            double step_cost = (DX[k] != 0 && DY[k] != 0) ? 1.4142135623730951 : 1.0;
            // M140 — entering a river/ridge/coastline cell costs more,
            // regardless of which direction it's entered from: this
            // discourages pushing territory *past* a natural feature more
            // than it discourages reaching the feature itself, so borders
            // tend to settle on or near one rather than cutting through
            // open land beyond it.
            if (is_feature[static_cast<std::size_t>(nidx)] != 0)
                step_cost *= natural_feature_cost_multiplier;
            pq.push({entry.cost + step_cost, nidx, entry.owner});
        }
    }

    for (std::size_t i = 0; i < net.countries.size(); ++i)
        net.countries[i].border = trace_owner_border(claimed, W, H, static_cast<int>(i),
                                                      world_x0, world_z0, cell_w, cell_h);

    return net;
}

namespace {

// M341 (MAP22) -- countries whose capitals sit close together get a
// softened chance of borrowing their nearest neighbor's naming style for
// their generated NAME string only. Deliberately narrow in scope, per an
// explicit user decision after this task was flagged as potentially
// conflicting with map.md's own closed M228 resolution ("Borders do not
// blend between cultures; a region's culture is inherited from its parent
// tile"): Country::culture itself, and MapTilePayload::culture's own
// tile-hierarchy inheritance, are both left completely untouched by this --
// only the assembled NAME can read as a hybrid, the same way a real
// border town's name sometimes carries a neighboring language's influence
// even while the town politically/culturally identifies with its own side.
// Beyond kCultureBlendMaxDistanceM, or when the nearest neighbor happens to
// share the same culture already, blend_t is exactly 0.0 -- byte-identical
// to the pre-M341, unblended `Naming::country(...)` call.
constexpr double kCultureBlendMaxDistanceM = 500000.0;
// Even at zero distance, a country's OWN culture stays favored more often
// than not (blend_t never reaches 0.5) -- this softens a hard border, it
// doesn't erase identity.
constexpr double kCultureBlendMaxT = 0.35;

double capital_distance(const Country& a, const Country& b) {
    const double dx = a.capital_x - b.capital_x;
    const double dz = a.capital_z - b.capital_z;
    return std::sqrt(dx * dx + dz * dz);
}

} // namespace

// M340/M341 (MAP22) -- naming happens in two passes. Pass 1 assigns every
// country's own culture exactly as before M341 (unaffected by this change,
// still a pure function of that country's own seed) -- done first, in its
// own loop, so pass 2's nearest-neighbor blend lookup below can safely read
// ANY other country's FINAL culture regardless of iteration order (a single
// combined loop would see garbage/default culture strings for every
// not-yet-visited higher index). Pass 2 dedupes within this network
// (M340, same reasoning as Settlements::name()'s own M340 change: two
// countries sharing an identical generated name, possible even across
// different cultures since dedupe() only compares the final assembled
// strings, previously went uncaught) while also applying M341's proximity
// blend. The generate lambda wraps Naming::country() (unblended path) or
// NameGenerator::blend() (blended path) -- either way preserving Naming::'s
// own real-registry/hardcoded-fallback duality, since blending is skipped
// entirely (own_c/neighbor_c stay nullptr) whenever the registry itself
// isn't available to resolve real NameCulture objects from.
void Countries::name(CountryNetwork& net, std::uint64_t entropy) {
    for (std::size_t i = 0; i < net.countries.size(); ++i) {
        const std::uint64_t seed = noise::hash2i(static_cast<std::int64_t>(i), kCountryNameAxis, entropy);
        net.countries[i].culture = MeshWorld::Naming::culture(seed);
    }

    std::unordered_set<std::string> used_names;
    for (std::size_t i = 0; i < net.countries.size(); ++i) {
        const std::uint64_t seed = noise::hash2i(static_cast<std::int64_t>(i), kCountryNameAxis, entropy);
        Country&            country = net.countries[i];

        double      nearest_d = -1.0;
        std::size_t nearest_j = i;
        for (std::size_t j = 0; j < net.countries.size(); ++j) {
            if (j == i) continue;
            const double d = capital_distance(country, net.countries[j]);
            if (nearest_d < 0.0 || d < nearest_d) { nearest_d = d; nearest_j = j; }
        }

        const bool has_close_different_neighbor =
            nearest_d >= 0.0 && nearest_d < kCultureBlendMaxDistanceM
            && net.countries[nearest_j].culture != country.culture;
        const double blend_t = has_close_different_neighbor
            ? kCultureBlendMaxT * (1.0 - nearest_d / kCultureBlendMaxDistanceM)
            : 0.0;

        const NameCulture* own_c      = blend_t > 0.0 ? MeshWorld::Naming::try_resolve_culture(country.culture) : nullptr;
        const NameCulture* neighbor_c = blend_t > 0.0
            ? MeshWorld::Naming::try_resolve_culture(net.countries[nearest_j].culture) : nullptr;

        const auto generate = [&](std::uint64_t e) -> std::string {
            if (own_c != nullptr && neighbor_c != nullptr) {
                return MeshWorld::NameGenerator::blend(
                    *own_c, *neighbor_c, blend_t, e,
                    [](const NameCulture& c, std::uint64_t ee) { return MeshWorld::NameGenerator::country(c, ee); });
            }
            return MeshWorld::Naming::country(country.culture, e);
        };
        country.name = MeshWorld::NameGenerator::dedupe(generate, seed, used_names);
        used_names.insert(country.name);
    }
}

} // namespace MeshWorld::Map
