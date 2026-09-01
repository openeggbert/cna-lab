// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "ProceduralWorldGen.hpp"
#include "ChunkCoord.hpp"
#include <array>

namespace MeshWorld {

// Weighted zone table — city intentionally rare so natural biomes dominate.
static constexpr std::array<ZoneType, 20> kZoneTable = {{
    ZoneType::forest,   ZoneType::forest,   ZoneType::forest,
    ZoneType::meadow,   ZoneType::meadow,   ZoneType::meadow,
    ZoneType::jungle,   ZoneType::jungle,
    ZoneType::desert,   ZoneType::desert,
    ZoneType::mountain, ZoneType::mountain,
    ZoneType::swamp,    ZoneType::swamp,
    ZoneType::tundra,
    ZoneType::beach,
    ZoneType::ocean,
    ZoneType::cave,
    ZoneType::city,
    ZoneType::city,
}};

ProceduralWorldGen::ProceduralWorldGen(uint64_t world_seed, int cell_size)
    : seed_(world_seed), cell_size_(cell_size > 0 ? cell_size : 1) {}

ZoneType ProceduralWorldGen::zone_at(int chunk_x, int chunk_y) const {
    const int cx = chunk_x / cell_size_;
    const int cy = chunk_y / cell_size_;
    const uint64_t h = chunk_seed(seed_, ChunkCoord{cx, cy});
    return kZoneTable[h % kZoneTable.size()];
}

} // namespace MeshWorld
