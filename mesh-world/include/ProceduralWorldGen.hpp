// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include <cstdint>
#include "ZoneType.hpp"

namespace MeshWorld {

// Assigns a ZoneType to each chunk deterministically from the world seed.
//
// The world is divided into coarse cells of `cell_size` chunks each.
// Every cell hashes to a ZoneType; all chunks inside the cell share it.
// This produces natural contiguous zone blobs without external noise libs.
class ProceduralWorldGen {
public:
    // cell_size: how many chunks wide/tall each procedural zone cell is.
    ProceduralWorldGen(uint64_t world_seed, int cell_size = 4);

    ZoneType zone_at(int chunk_x, int chunk_y) const;

private:
    uint64_t seed_;
    int      cell_size_;
};

} // namespace MeshWorld
