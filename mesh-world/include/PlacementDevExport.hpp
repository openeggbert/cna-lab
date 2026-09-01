// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <string>
#include <vector>

#include "ChunkGenerator.hpp"
#include "ModelPlacement.hpp"

namespace MeshWorld {

// M179 (MAP11) -- dev-only export: renders one chunk's ModelPlacements as
// MC3 <instance> elements, for visual/manual debugging. NOT used at
// runtime -- the real path is ModelPlacementStore::query_box() plus a real
// renderer (M175); this exists purely because SQLite blobs aren't
// hand-editable/inspectable the way a .mc3.xml file is (map.md section
// 10.1's own "keep a dev-mode export" trade-off note).
//
// `placements` are assumed to already belong to `ctx.coord` (the caller's
// responsibility, e.g. a single ModelPlacementStore::query_box() result
// filtered to one chunk) -- this function does not validate chunk
// membership, matching its dev-tool, trust-the-caller scope.
std::string export_chunk_placements_to_mc3(const ChunkContext& ctx,
                                            const std::vector<ModelPlacement>& placements);

} // namespace MeshWorld
