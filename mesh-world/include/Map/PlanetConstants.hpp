// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

namespace MeshWorld::Map {

// Planet & LOD-pyramid constants (see map.md §4-§5).

// Side length of the (flat, square) planet in meters. Chosen so the area
// (side^2) is approximately Earth's surface area (~510,072,000 km²):
//   sqrt(510,072,000 km²) ≈ 22,585.4 km ≈ 22,585,000 m.
// double (not float) — float's ~7 significant digits cannot represent meter
// precision at this magnitude.
inline constexpr double PLANET_SIZE_M = 22'585'000.0;

// Deepest quadtree level. At level z the tile edge is PLANET_SIZE_M / 2^z;
// at level 18 that is ≈ 86.2 m — the hand-off scale to the 64 m chunk grid.
inline constexpr int MAX_LEVEL = 18;

// Vertical bucket size (meters) for altitude indexing of 3D model placements,
// matching the 64 m chunk edge. Used by alt_band(y) (M012).
inline constexpr double ALT_BAND_M = 64.0;

} // namespace MeshWorld::Map
