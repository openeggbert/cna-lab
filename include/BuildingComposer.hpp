// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <optional>
#include <string>

#include "ChunkGenerator.hpp"

namespace MeshWorld {

// R113 (docs/world-composer-design.md) -- the actual "world composer",
// deliberately named BuildingComposer for v1: it composes buildings onto
// parcels (small_house_block/apartment_block/shop_street, via Parcel's
// derive_parcels()) plus, since R129, one whole-chunk civic composition
// for RegionType::square (its own compose_square() path, no parcels
// involved) -- not mesh_world_revival.md §11's full eventual scope
// (roads too). See the design doc for why.
class BuildingComposer {
public:
    // Attempts to compose full mc3.xml content for `ctx` from real
    // registered assets (AssetRegistry). Returns std::nullopt if no
    // parcels apply to this chunk's region (or, for `square`, no
    // street_furniture asset is registered), or no asset satisfies any
    // parcel's category/style requirements -- caller (ChunkPipeline) must
    // fall through to the existing Lua/C++ chain unchanged in that case,
    // never treat nullopt as an error.
    std::optional<std::string> compose_chunk(const ChunkContext& ctx) const;
};

} // namespace MeshWorld
