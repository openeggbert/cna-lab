// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "WorldMap.hpp"
#include "ProceduralWorldGen.hpp"

#include <algorithm>

namespace MeshWorld {
namespace {

void set_road(EdgeExits& exits, Edge edge, bool value = true) {
    switch (edge) {
        case Edge::North: exits.north_road = value; break;
        case Edge::South: exits.south_road = value; break;
        case Edge::East:  exits.east_road  = value; break;
        case Edge::West:  exits.west_road  = value; break;
    }
}

Edge opposite(Edge edge) {
    switch (edge) {
        case Edge::North: return Edge::South;
        case Edge::South: return Edge::North;
        case Edge::East:  return Edge::West;
        case Edge::West:  return Edge::East;
    }
    return Edge::North;
}

const char* edge_name(Edge edge) {
    switch (edge) {
        case Edge::North: return "north";
        case Edge::South: return "south";
        case Edge::East:  return "east";
        case Edge::West:  return "west";
    }
    return "unknown";
}

bool terminus_matches(const RoadTerminus& terminus, int x, int y, Edge edge) {
    return terminus.chunk_x == x && terminus.chunk_y == y && terminus.edge == edge_name(edge);
}

} // namespace

WorldMap::WorldMap(const WorldConfig& cfg) : cfg_(cfg) { build(); }

void WorldMap::build() {
    const int total = cfg_.grid_w * cfg_.grid_h;
    grid_.assign(total, ChunkInfo{cfg_.zone_default, cfg_.region_default, {}});

    // Procedural fill: overrides the default zone/region for every chunk.
    // JSON zone overrides are still applied afterwards.
    if (cfg_.procedural) {
        ProceduralWorldGen gen(cfg_.seed, cfg_.procedural_cell_size);
        for (int y = 0; y < cfg_.grid_h; ++y) {
            for (int x = 0; x < cfg_.grid_w; ++x) {
                auto& ci  = grid_[y * cfg_.grid_w + x];
                ci.zone   = gen.zone_at(x, y);
                ci.region = RegionType::open;
            }
        }
    }

    // Apply zone overrides in order; later entries take priority.
    for (const auto& zo : cfg_.zones) {
        for (int y = zo.y_min; y <= zo.y_max; ++y) {
            for (int x = zo.x_min; x <= zo.x_max; ++x) {
                if (!in_bounds(x, y)) continue;
                auto& ci  = grid_[y * cfg_.grid_w + x];
                ci.zone   = zo.type;
                ci.region = zo.region_default;
            }
        }
        // Apply region overrides within this zone.
        for (const auto& ro : zo.regions) {
            for (int y = ro.y_min; y <= ro.y_max; ++y) {
                for (int x = ro.x_min; x <= ro.x_max; ++x) {
                    if (!in_bounds(x, y)) continue;
                    grid_[y * cfg_.grid_w + x].region = ro.type;
                }
            }
        }
    }

    authored_exits_.assign(static_cast<std::size_t>(total), EdgeExits{});
    rebuild_road_topology();
}

ChunkInfo WorldMap::info(int x, int y) const {
    if (!in_bounds(x, y)) return {};
    return grid_[y * cfg_.grid_w + x];
}

void WorldMap::set_info(int x, int y, const ChunkInfo& info) {
    if (!in_bounds(x, y)) return;
    const std::size_t index = static_cast<std::size_t>(y * cfg_.grid_w + x);
    grid_[index] = info;
    // `ChunkInfo::exits` remains the backwards-compatible authored input to
    // set_info(). Keep it separately: rebuild_road_topology() publishes
    // computed frontage through grid_.exits, so it must not later mistake
    // that derived result for an authored crossing.
    authored_exits_[index] = info.exits;
    rebuild_road_topology();
}

EdgeExits WorldMap::road_connections(int x, int y) const {
    if (!in_bounds(x, y)) return {};
    return road_connections_[static_cast<std::size_t>(y * cfg_.grid_w + x)];
}

EdgeExits WorldMap::road_frontage(int x, int y) const {
    if (!in_bounds(x, y)) return {};
    return road_frontage_[static_cast<std::size_t>(y * cfg_.grid_w + x)];
}

void WorldMap::rebuild_road_topology() {
    const std::size_t total = grid_.size();
    road_connections_.assign(total, EdgeExits{});
    road_frontage_.assign(total, EdgeExits{});

    const auto index = [this](int x, int y) {
        return static_cast<std::size_t>(y * cfg_.grid_w + x);
    };
    const auto connect_pair = [&](int ax, int ay, Edge a_edge, int bx, int by) {
        const std::size_t a = index(ax, ay);
        const std::size_t b = index(bx, by);
        const Edge b_edge = opposite(a_edge);
        const bool regions_connect = is_road_region(grid_[a].region) && is_road_region(grid_[b].region);
        const bool authored_connect = authored_exits_[a].has_road(a_edge) ||
                                      authored_exits_[b].has_road(b_edge);
        if (regions_connect || authored_connect) {
            set_road(road_connections_[a], a_edge);
            set_road(road_connections_[b], b_edge);
        }
        // Frontage is intentionally not a physical crossing graph: it tells
        // a parcel which of its sides borders a road chunk. An explicit edge
        // also counts as frontage so callers that authored an edge before
        // R134 retain useful behaviour.
        if (is_road_region(grid_[b].region) || authored_connect)
            set_road(road_frontage_[a], a_edge);
        if (is_road_region(grid_[a].region) || authored_connect)
            set_road(road_frontage_[b], b_edge);
    };

    for (int y = 0; y < cfg_.grid_h; ++y) {
        for (int x = 0; x < cfg_.grid_w; ++x) {
            if (x + 1 < cfg_.grid_w) connect_pair(x, y, Edge::East, x + 1, y);
            if (y + 1 < cfg_.grid_h) connect_pair(x, y, Edge::South, x, y + 1);
        }
    }

    // Keep WorldMap::exits()/ChunkInfo::exits' long-standing meaning for
    // non-road callers: adjacent road frontage. Physical road consumers use
    // road_connections() explicitly, avoiding the old overloaded contract.
    for (std::size_t i = 0; i < total; ++i)
        grid_[i].exits = road_frontage_[i];
}

std::vector<RoadEdgeIssue> WorldMap::validate_road_network() const {
    std::vector<RoadEdgeIssue> issues;
    const auto has_matching_terminus = [&](int x, int y, Edge edge) {
        return std::any_of(cfg_.road_termini.begin(), cfg_.road_termini.end(),
                           [&](const RoadTerminus& t) { return terminus_matches(t, x, y, edge); });
    };

    for (const RoadTerminus& terminus : cfg_.road_termini) {
        Edge edge = Edge::North;
        if (terminus.edge == "south") edge = Edge::South;
        else if (terminus.edge == "east") edge = Edge::East;
        else if (terminus.edge == "west") edge = Edge::West;
        if (!in_bounds(terminus.chunk_x, terminus.chunk_y)) {
            issues.push_back({terminus.chunk_x, terminus.chunk_y, "road terminus is outside the world grid"});
            continue;
        }
        if (!is_road_region(region(terminus.chunk_x, terminus.chunk_y))) {
            issues.push_back({terminus.chunk_x, terminus.chunk_y, "road terminus is not on a road/crossroad chunk"});
            continue;
        }
        const int nx = terminus.chunk_x + (edge == Edge::East ? 1 : edge == Edge::West ? -1 : 0);
        const int ny = terminus.chunk_y + (edge == Edge::South ? 1 : edge == Edge::North ? -1 : 0);
        if (terminus.kind == RoadTerminusKind::boundary && in_bounds(nx, ny))
            issues.push_back({terminus.chunk_x, terminus.chunk_y, "boundary road terminus does not face the world boundary"});
        if (road_connections(terminus.chunk_x, terminus.chunk_y).has_road(edge))
            issues.push_back({terminus.chunk_x, terminus.chunk_y, "road terminus points at a connected road edge"});
    }

    constexpr Edge all_edges[] = {Edge::North, Edge::South, Edge::East, Edge::West};
    for (int y = 0; y < cfg_.grid_h; ++y) {
        for (int x = 0; x < cfg_.grid_w; ++x) {
            if (!is_road_region(region(x, y))) continue;
            const EdgeExits connections = road_connections(x, y);
            int degree = 0;
            for (const Edge edge : all_edges)
                degree += connections.has_road(edge) ? 1 : 0;
            if (degree == 0) {
                issues.push_back({x, y, "isolated road/crossroad chunk has no canonical road connection"});
                continue;
            }
            if (degree != 1) continue;
            bool approved = false;
            for (const Edge edge : all_edges)
                if (!connections.has_road(edge) && has_matching_terminus(x, y, edge)) approved = true;
            if (!approved)
                issues.push_back({x, y, "degree-one road/crossroad chunk needs an explicit road_termini entry"});
        }
    }
    return issues;
}

std::array<float,3> WorldMap::zone_color(ZoneType z) {
    // Grown 12->52, M235 (MAP16, 2026-07-10) -- same palette as
    // PlanetMapLogic.cpp's zone_rgb_color(), normalized to [0,1] float
    // (÷255) instead of RGB bytes; keep the two in sync if either changes.
    switch (z) {
        case ZoneType::city:     return {0.60f, 0.60f, 0.60f};
        case ZoneType::jungle:   return {0.10f, 0.50f, 0.10f};
        case ZoneType::desert:   return {0.90f, 0.80f, 0.35f};
        case ZoneType::forest:   return {0.20f, 0.65f, 0.20f};
        case ZoneType::ocean:    return {0.10f, 0.35f, 0.80f};
        case ZoneType::mountain: return {0.45f, 0.40f, 0.35f};
        case ZoneType::tundra:   return {0.90f, 0.93f, 0.97f};
        case ZoneType::swamp:    return {0.25f, 0.38f, 0.20f};
        case ZoneType::cave:     return {0.25f, 0.18f, 0.12f};
        case ZoneType::meadow:   return {0.55f, 0.85f, 0.35f};
        case ZoneType::beach:    return {0.95f, 0.90f, 0.65f};

        case ZoneType::savanna:               return {0.78f, 0.75f, 0.39f};
        case ZoneType::steppe:                return {0.71f, 0.67f, 0.43f};
        case ZoneType::prairie:               return {0.67f, 0.76f, 0.39f};
        case ZoneType::chaparral:             return {0.63f, 0.59f, 0.31f};
        case ZoneType::shrubland:             return {0.59f, 0.55f, 0.35f};

        case ZoneType::taiga:                 return {0.08f, 0.31f, 0.24f};
        case ZoneType::temperate_rainforest:  return {0.06f, 0.35f, 0.20f};
        case ZoneType::mixed_forest:          return {0.18f, 0.43f, 0.18f};
        case ZoneType::cloud_forest:          return {0.16f, 0.51f, 0.35f};
        case ZoneType::mangrove:              return {0.12f, 0.39f, 0.31f};
        case ZoneType::bamboo_forest:         return {0.35f, 0.59f, 0.20f};
        case ZoneType::riparian_forest:       return {0.20f, 0.51f, 0.27f};
        case ZoneType::tropical_dry_forest:   return {0.43f, 0.51f, 0.20f};

        case ZoneType::marsh:                 return {0.39f, 0.43f, 0.27f};
        case ZoneType::floodplain:            return {0.51f, 0.55f, 0.35f};
        case ZoneType::bog:                   return {0.27f, 0.31f, 0.22f};
        case ZoneType::muskeg:                return {0.24f, 0.33f, 0.27f};

        case ZoneType::dunes:                 return {0.88f, 0.76f, 0.47f};
        case ZoneType::rocky_desert:          return {0.67f, 0.55f, 0.39f};
        case ZoneType::cold_desert:           return {0.71f, 0.69f, 0.59f};
        case ZoneType::salt_flat:             return {0.92f, 0.90f, 0.86f};
        case ZoneType::badlands:              return {0.63f, 0.43f, 0.31f};
        case ZoneType::mesa:                  return {0.71f, 0.47f, 0.27f};
        case ZoneType::canyon:                return {0.59f, 0.37f, 0.25f};
        case ZoneType::oasis:                 return {0.35f, 0.71f, 0.47f};

        case ZoneType::glacier:               return {0.88f, 0.94f, 0.98f};
        case ZoneType::permafrost:            return {0.63f, 0.65f, 0.61f};
        case ZoneType::alpine_meadow:         return {0.67f, 0.78f, 0.55f};
        case ZoneType::ice_cap:               return {0.96f, 0.98f, 1.00f};

        case ZoneType::volcanic:              return {0.31f, 0.16f, 0.12f};
        case ZoneType::geothermal:            return {0.78f, 0.39f, 0.24f};
        case ZoneType::ash_plain:             return {0.39f, 0.35f, 0.33f};
        case ZoneType::volcanic_island:       return {0.24f, 0.27f, 0.22f};

        case ZoneType::coral_reef:            return {0.24f, 0.71f, 0.75f};
        case ZoneType::kelp_forest:           return {0.16f, 0.43f, 0.43f};
        case ZoneType::deep_ocean:            return {0.04f, 0.16f, 0.43f};
        case ZoneType::lagoon:                return {0.27f, 0.75f, 0.78f};
        case ZoneType::fjord:                 return {0.20f, 0.35f, 0.51f};
        case ZoneType::tidal_flat:            return {0.63f, 0.67f, 0.55f};
        case ZoneType::sea_cliff:             return {0.43f, 0.39f, 0.37f};

        case ZoneType::empty:    return {0.05f, 0.05f, 0.05f};
    }
    return {0.f, 0.f, 0.f};
}

} // namespace MeshWorld
