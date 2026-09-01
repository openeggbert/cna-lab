// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "Map/TileCoord.hpp"

namespace MeshWorld::Map {

// Everything a map generator produces for one tile and the store persists.
// Serialization (JSON / packed) is M023-M025; this is the in-memory shape.

// Row-major 2D scalar field (elevation in meters, temperature, moisture 0..1).
struct FieldGrid {
    int w{0}, h{0};
    std::vector<float> data;  // size w*h, row-major: index = gy*w + gx
    float at(int gx, int gy) const { return data[static_cast<std::size_t>(gy) * w + gx]; }
    bool empty() const { return data.empty(); }
};

// Row-major 2D categorical biome grid; values are ZoneType ordinals (decoupled
// here as uint8_t; mapped to ZoneType at the chunk hand-off, M160).
struct BiomeGrid {
    int w{0}, h{0};
    std::vector<std::uint8_t> data;
    std::uint8_t at(int gx, int gy) const { return data[static_cast<std::size_t>(gy) * w + gx]; }
    bool empty() const { return data.empty(); }
};

// New enumerators are appended (never inserted) so the codec's integer ordinals
// stay stable across versions.
enum class FeatureType { River, Lake, MountainRange, Coastline, Border, City, Town, Road, Other,
                         Continent, Street, Park };

// A vector map feature: river path, city center, country border polygon, etc.
// points holds a path / polygon / single center point depending on type.
struct MapFeature {
    FeatureType type{FeatureType::Other};
    std::string name;
    std::vector<std::array<double, 2>> points;
    std::map<std::string, double>      attributes;  // width, population_hint, ...
};

// New enumerators are appended (never inserted) so the codec's integer
// ordinals stay stable across versions (same rule as FeatureType above).
enum class EdgeCrossingType { River, Road };

// A single point where a river or road crosses a tile edge (M107). `position`
// is the fractional distance along the edge, 0..1: for N/S edges measured
// west-to-east, for E/W edges measured north-to-south (same direction as
// TileEdge::elevation's sample ordering).
struct EdgeCrossing {
    EdgeCrossingType type{EdgeCrossingType::River};
    float            position{0.0f};
};

// Boundary data this tile exports to its children for constraint propagation.
// Edge indices: 0=N,1=E,2=S,3=W. `biome` and `crossings` are M107 additions:
// structural only for now — empty until a future generator (M110/M111/M115/
// M116) populates them; no generator writes them yet.
struct TileEdge {
    std::vector<float>        elevation;  // elevation samples along the edge
    std::vector<std::uint8_t> biome;      // ZoneType ordinals, parallel to elevation
    std::vector<EdgeCrossing> crossings;  // river/road crossing points along the edge
};

// A display label for the map UI (separate from full features for cheap text draw).
struct PlaceLabel {
    std::string          name;
    std::array<double, 2> pos{{0.0, 0.0}};
    std::string          kind;  // "continent","country","city","river",...
};

struct MapTilePayload {
    TileCoord     tile;          // self-describing back-reference
    std::uint64_t entropy{0};    // tile_entropy used to generate it
    std::string   culture;       // naming culture for this region
    std::string   generator;     // generator id (metadata)

    FieldGrid elevation;         // meters
    FieldGrid temperature;       // generator units
    FieldGrid moisture;          // 0..1
    BiomeGrid biome;             // ZoneType ordinals
    // M156: ZoneCandidate ordinals, one per cell (see Map::ZoneCandidate),
    // empty until a generator calls MapBuilder::setZoneCandidates().
    // Mapped to an actual per-chunk RegionType at the hand-off (M157),
    // same "decoupled here, translated later" relationship biome above
    // has with ZoneType at M160.
    BiomeGrid zone_candidates;

    std::vector<MapFeature> features;
    std::vector<PlaceLabel> labels;
    std::array<TileEdge, 4> edges;  // N,E,S,W
};

} // namespace MeshWorld::Map
