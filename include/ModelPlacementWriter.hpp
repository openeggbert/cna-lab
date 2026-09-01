// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <string>
#include <vector>

#include "ChunkCoord.hpp"
#include "ModelPlacement.hpp"

namespace MeshWorld {

// M169 (MAP11) — writes a chunk's ModelPlacements (ChunkGenerator::placements(),
// M168) into the region ModelPlacementStore they belong to: derives the
// region via region_for_chunk() (M026), opens/creates that region's store
// (M027), and calls insert_batch() (M030). A no-op if `placements` is empty
// -- no reason to open/create a region DB file for a chunk with nothing to
// write (most chunks today, since only ForestGenerator overrides
// ChunkGenerator::placements()).
//
// Deliberate simplification: this reopens the region's SQLite file on every
// call rather than caching a ModelPlacementStore instance across calls.
// Real caching (lazy-open per region, reused across many chunk writes)
// belongs to MAP3's own M045 ("PlanetWorld: lazy-open ... ModelPlacementStore
// per region"), not here -- this task is just the writer itself.
void write_chunk_placements(const std::string& world_dir, const ChunkCoord& chunk,
                             const std::vector<ModelPlacement>& placements);

} // namespace MeshWorld
