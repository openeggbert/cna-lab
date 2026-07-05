#pragma once

#include <cstdint>
#include <vector>

namespace CnaCraft::Worlds {

// Plain, engine-agnostic vertex: `u, v` are tile-local (0..1 within one atlas
// tile), `tileIndex` says which atlas tile. Folding tileIndex + local UV into
// a final atlas UV is left to the CNA-side TextureAtlas (plan.md §5) — this
// type never needs to know atlas layout.
struct MeshVertex {
    float px = 0.0f, py = 0.0f, pz = 0.0f;
    float nx = 0.0f, ny = 0.0f, nz = 0.0f;
    float u = 0.0f, v = 0.0f;
    int tileIndex = 0;
};

struct MeshData {
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
};

}
