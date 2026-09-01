// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include "Map/MapTilePayload.hpp"
#include "ValidationResult.hpp"

namespace MeshWorld {

// Structural checks on a single Map::MapTilePayload before it is persisted
// (MAP6, M098/M099). Mirrors MC3Validator's role for chunk XML — reuses the
// same ValidationResult type rather than inventing a parallel one.
//
// Checks (see validate()'s implementation for the authoritative list):
//   - elevation/temperature/moisture/biome fields are present and internally
//     consistent (matching w/h, data.size() == w*h).
//   - biome ordinals are within ZoneType's valid range.
//   - every feature's points lie within the tile's world bounds
//     (Map::TileCoord::world_bounds()).
//   - edge descriptors have the length implied by the elevation grid shape
//     (N/S = w samples, E/W = h samples).
//
// Deliberately scoped narrower than "edges match parent" might suggest
// (plan.md M099): this only checks a payload's *own* internal edge-length
// consistency, not cross-tile bilinear agreement with an actual parent
// payload — that boundary-continuity property is already covered by
// dedicated tests (tests/MapPipelineTests.cpp's M117 coverage) and would
// require re-deriving generator math here, which a structural validator
// should not do.
class MapValidator {
public:
    ValidationResult validate(const Map::MapTilePayload& payload) const;
};

} // namespace MeshWorld
