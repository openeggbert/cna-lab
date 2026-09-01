// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "ModelPlacementWriter.hpp"

#include "ModelPlacementStore.hpp"
#include "RegionShard.hpp"

namespace MeshWorld {

void write_chunk_placements(const std::string& world_dir, const ChunkCoord& chunk,
                             const std::vector<ModelPlacement>& placements) {
    if (placements.empty()) return;

    const RegionId region = region_for_chunk(chunk);
    ModelPlacementStore store(world_dir, region);
    store.insert_batch(chunk, placements);
}

} // namespace MeshWorld
