// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include "Map/MapGenerator.hpp"
#include "generators/map/PlanetGenerator.hpp"  // for PlanetParams

namespace MeshWorld::Map {

// M108/M109 — generates a child tile constrained by its parent payload.
//
// Elevation: bilinear interpolation of the parent's elevation grid at the
// child's quadrant, plus high-frequency fBm detail. The fBm is attenuated to
// zero at cell boundaries (sin²-fade) so siblings always agree at their shared
// edges (M112): both map to the SAME parent column/row, yielding identical
// bilinear values with zero fBm contribution.
//
// Mapping: child cell (gx, gy) → parent position (cx*32 + gx*32/63, cy*32 + gy*32/63)
// where (cx, cy) = (tile.x%2, tile.y%2). This places shared sibling boundaries
// exactly at parent columns/rows 0, 32, 64 — each pair of siblings reads the
// same parent value there.
class ChildGenerator : public MapGenerator {
public:
    explicit ChildGenerator(PlanetParams params);

    MapTilePayload generate(const TileCoord&      tile,
                            const MapTilePayload* parent,
                            std::uint64_t         entropy) const override;

private:
    PlanetParams params_;
};

} // namespace MeshWorld::Map
