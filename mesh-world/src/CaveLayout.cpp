// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "CaveLayout.hpp"

#include "Map/Noise.hpp"

namespace MeshWorld {

namespace {

// M332 — target per-edge tunnel-opening probability. ~40%: high enough
// that most cave chunks end up connected to at least one neighbor (a real
// system, not scattered isolated rooms), low enough that dead ends and
// isolated chambers still commonly occur too — real cave systems are
// sparse and irregular, not a solid lattice where every wall is a doorway.
constexpr double kOpeningProbability = 0.40;

// Decorrelates this hash channel from every other Map::noise::hash2i(...)
// caller in this codebase (river/lake/mountain-range naming use small
// integer axis constants 601-603, see FeatureNaming.cpp; this just needs
// to be distinctly outside that numbering scheme, not part of it — cave
// layout is a legacy-chunk-system concept, unrelated to that Map::-side
// naming machinery).
constexpr std::uint64_t kCaveLayoutSalt = 0xCA4E000000000000ULL;

// Canonical per-edge hash: symmetric in (a, b) because it always hashes
// the two coordinates in the SAME order regardless of which one is passed
// first (ChunkCoord::operator< sorts them) — composed as two chained
// hash2i() calls (not a single call over packed/combined coordinates) so
// there's no bit-packing collision risk at any realistic world size.
bool edge_has_opening(std::uint64_t world_seed, const ChunkCoord& a, const ChunkCoord& b) {
    const ChunkCoord lo = (a < b) ? a : b;
    const ChunkCoord hi = (a < b) ? b : a;
    const std::uint64_t h1 = Map::noise::hash2i(lo.x, lo.y, world_seed ^ kCaveLayoutSalt);
    const std::uint64_t h2 = Map::noise::hash2i(hi.x, hi.y, h1);
    return Map::noise::to_unit_float(h2) < static_cast<float>(kOpeningProbability);
}

} // namespace

CaveOpenings CaveLayout::openings_for(std::uint64_t world_seed, const ChunkCoord& coord) {
    CaveOpenings o;
    o.north = edge_has_opening(world_seed, coord, coord.north());
    o.south = edge_has_opening(world_seed, coord, coord.south());
    o.east  = edge_has_opening(world_seed, coord, coord.east());
    o.west  = edge_has_opening(world_seed, coord, coord.west());
    return o;
}

} // namespace MeshWorld
