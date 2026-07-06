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

// ChunkMesher::Build emits two independent meshes (plan.md §11.2
// "Transparency for glass"): `opaque` is drawn normally; `transparent` is
// drawn last, with alpha blending on and depth writes off, so blocks like
// Glass show the scene behind them instead of occluding it.
struct ChunkMeshData {
    MeshData opaque;
    MeshData transparent;
};

}
