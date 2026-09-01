// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cstdint>

#include "ChunkCoord.hpp"

namespace MeshWorld {

// M332 (MAP21) — which of a cave chunk's 4 sides connect to a tunnel
// opening toward that neighbor. Lets CaveGenerator draw a real (if sparse
// and irregular) connected cave SYSTEM across chunks instead of every cave
// chunk being its own isolated sealed room (the pre-M332 behavior).
struct CaveOpenings {
    bool north{false}, south{false}, east{false}, west{false};

    int count() const {
        return (north ? 1 : 0) + (south ? 1 : 0) + (east ? 1 : 0) + (west ? 1 : 0);
    }
};

// Pure static; no state (mirrors Hydrology/MountainRanges/BiomeRefinement's
// style, MAP8).
class CaveLayout {
public:
    // Deterministic AND symmetric: chunk A's own opening toward its
    // neighbor B always agrees with chunk B's own opening toward A,
    // regardless of which chunk's own independent generate() call asks
    // first (see CaveLayout.cpp's edge_has_opening() for how). This
    // function has no visibility into whether a neighbor chunk is itself
    // cave-zoned -- a per-tile ChunkGenerator::generate(ctx) call only ever
    // sees its OWN ctx, never a neighbor's actual classified zone (same
    // "no lookahead into a neighbor's real content" limitation every other
    // generator already has, e.g. ForestGenerator doesn't know its
    // neighbor's zone either). An opening computed toward a neighbor that
    // turns out NOT to be cave-zoned simply means that tunnel mouth meets
    // solid rock/whatever that chunk draws instead -- harmless, no crash,
    // same class of "sibling coherence not fully cross-verified" tolerance
    // this codebase already accepts elsewhere (NEXT.md §5, known limitation
    // 3c). Cave-to-SURFACE entrance placement (a categorically different
    // problem -- where a cave should connect to the outside world at all)
    // is explicitly out of this function's scope, see M336.
    static CaveOpenings openings_for(std::uint64_t world_seed, const ChunkCoord& coord);
};

} // namespace MeshWorld
