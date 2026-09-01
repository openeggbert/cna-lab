// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cstdint>

#include "Map/MapTilePayload.hpp"
#include "Map/TileCoord.hpp"

namespace MeshWorld::Map {

// Abstract interface for a map-tile generator (MAP4). Given a tile, its parent
// payload, and the tile's entropy, it produces a fully-populated MapTilePayload.
//
//   tile    — which quadtree tile to generate.
//   parent  — the already-generated parent tile's payload, used to constrain the
//             child (edge continuity, inherited features). NULL at level 0: the
//             planet root has no parent (TileCoord::parent() of level 0 is itself).
//   entropy — the tile's entropy (typically tile_entropy(world_entropy, tile)).
//
// Generators must be pure with respect to these inputs: the same
// (tile, parent, entropy) must yield the same payload, so a regenerated tile
// matches a persisted one (determinism — see M070). Mirrors ChunkGenerator for
// the chunk layer, but in the planetary map domain.
class MapGenerator {
public:
    virtual ~MapGenerator() = default;

    virtual MapTilePayload generate(const TileCoord&      tile,
                                    const MapTilePayload* parent,
                                    std::uint64_t         entropy) const = 0;
};

} // namespace MeshWorld::Map
