// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cstddef>
#include <string>

#include "ChunkGenerator.hpp"

namespace MeshWorld {

struct AssetEntry;

// R143a -- registers the first reusable, metadata-tagged natural MC3 kits.
// Safe to call repeatedly, like register_composer_assets().
void register_nature_assets();

// Returns a deterministic category/tag-matching natural asset, or nullptr
// when a minimal tool/test deliberately has not populated the asset registry.
const AssetEntry* pick_nature_asset(const ChunkContext& ctx,
                                    const std::string& category,
                                    const std::string& biome_tag,
                                    std::size_t ordinal);

// Chooses an authored low-detail proxy when a caller supplies a coarse LOD.
const std::string& resolve_nature_asset_id(const AssetEntry& asset,
                                           const ChunkContext& ctx);

} // namespace MeshWorld
