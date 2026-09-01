// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <array>
#include <string>
#include <vector>
#include "ZoneType.hpp"
#include "RegionType.hpp"
#include "ChunkCoord.hpp"
#include "WorldConfig.hpp"

namespace MeshWorld {

enum class Edge { North, South, East, West };

struct EdgeExits {
    bool north_road{false}, south_road{false};
    bool east_road{false},  west_road{false};
    bool north_path{false}, south_path{false};
    bool east_path{false},  west_path{false};

    bool has_road(Edge e) const {
        switch (e) {
            case Edge::North: return north_road;
            case Edge::South: return south_road;
            case Edge::East:  return east_road;
            case Edge::West:  return west_road;
        }
        return false;
    }
};

struct ChunkInfo {
    ZoneType   zone{ZoneType::empty};
    RegionType region{RegionType::empty};
    EdgeExits  exits;
};

// A configuration-level error found by the multi-chunk road validator. It is
// intentionally independent from MC3Validator: the latter validates one MC3
// document, while this describes the relationship between chunk documents.
struct RoadEdgeIssue {
    int         x{0};
    int         y{0};
    std::string message;
};

// WorldMap assigns a (ZoneType, RegionType) and EdgeExits to every chunk.
// Deterministic: same WorldConfig always produces the same map.
class WorldMap {
public:
    explicit WorldMap(const WorldConfig& cfg);

    ChunkInfo  info(int x, int y) const;
    ChunkInfo  info(const ChunkCoord& c) const { return info(c.x, c.y); }

    ZoneType   zone  (int x, int y) const { return info(x, y).zone;   }
    RegionType region(int x, int y) const { return info(x, y).region; }
    EdgeExits  exits (int x, int y) const { return info(x, y).exits;  }

    ZoneType   zone  (const ChunkCoord& c) const { return info(c).zone;   }
    RegionType region(const ChunkCoord& c) const { return info(c).region; }
    EdgeExits  exits (const ChunkCoord& c) const { return info(c).exits;  }

    // R134 -- canonical physical road crossings, guaranteed symmetric across
    // every shared chunk edge. Road/Crossroad generators consume these.
    EdgeExits road_connections(int x, int y) const;
    EdgeExits road_connections(const ChunkCoord& c) const { return road_connections(c.x, c.y); }

    // A non-road parcel's neighbouring road sides. This deliberately remains
    // directional (a house beside a road has frontage, while the road does not
    // gain a spur into that house) and is what BuildingComposer consumes.
    EdgeExits road_frontage(int x, int y) const;
    EdgeExits road_frontage(const ChunkCoord& c) const { return road_frontage(c.x, c.y); }

    // R134's multi-chunk validation pass. Callers such as MeshWorldExport
    // report these before generating a misleading city full of road stubs.
    std::vector<RoadEdgeIssue> validate_road_network() const;

    static std::array<float,3> zone_color(ZoneType z);

    bool in_bounds(int x, int y) const {
        return x >= 0 && x < cfg_.grid_w && y >= 0 && y < cfg_.grid_h;
    }

    // Override a single cell (used by PersistentWorldMap to populate on demand).
    // No-op if (x,y) is out of bounds.
    void set_info(int x, int y, const ChunkInfo& info);

    const WorldConfig& config() const { return cfg_; }

private:
    void build();
    void rebuild_road_topology();

    WorldConfig             cfg_;
    std::vector<ChunkInfo>  grid_;   // row-major [y * grid_w + x]
    std::vector<EdgeExits>  authored_exits_;
    std::vector<EdgeExits>  road_connections_;
    std::vector<EdgeExits>  road_frontage_;
};

} // namespace MeshWorld
