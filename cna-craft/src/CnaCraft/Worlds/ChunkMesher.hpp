#pragma once

#include "MeshData.hpp"

namespace CnaCraft::Worlds {

class World;

class ChunkMesher {
public:
    // originX/Y/Z are the chunk's world-space block-coordinate origin
    // (cx*CHUNK_SIZE etc.). Emitted vertex positions are chunk-local
    // ([0, CHUNK_SIZE]); the renderer places the chunk via its World matrix.
    //
    // Only faces exposed to a non-opaque neighbor are emitted (naive per-face
    // culling, not greedy meshing — see plan.md §4/§9), inspired by Craft's
    // "only exposed faces are rendered" (THIRD_PARTY_NOTICES.md). Neighbor
    // lookups go through World::IsOpaque, which transparently reads across
    // chunk boundaries, so faces at chunk edges cull correctly too. Opaque
    // and transparent (e.g. Glass) blocks are emitted into two separate
    // meshes — see ChunkMeshData (plan.md §11.2 "Transparency for glass").
    static ChunkMeshData Build(const World& world, int originX, int originY, int originZ);

    // Direct port of Craft's occlusion() (main.c:879-921), zero-light
    // variant (cna-craft has no torch-light propagation -- item 27's glow
    // pass substitutes for it). `neighbors`/`shades` describe the 3x3x3
    // neighborhood around one block, indexed by NeighborIndex(dx,dy,dz):
    // `neighbors` is 0/1 occupancy (World::IsOpaque semantics -- transparent
    // blocks and plants do NOT occlude, matching Craft's !is_transparent),
    // `shades` the per-cell column shade (1 - 0.125*oy for the nearest
    // opaque cell at or above within 8, else 0). Output ao[face][corner] is
    // Craft's occlusion amount in [0,1] (0 = unoccluded/bright, 1 = fully
    // occluded/dark), in THIS mesher's face/corner order (not Craft's --
    // the lookup cells are derived geometrically from the face definitions
    // rather than transcribed from Craft's lookup3/lookup4 tables, since
    // both orderings differ). Public so tests can pin known values.
    static void ComputeOcclusion(const std::uint8_t neighbors[27], const float shades[27], float ao[6][4]);

    // 3x3x3 neighborhood flattening for ComputeOcclusion's inputs: dx
    // outermost, dz innermost, each in {-1,0,1} (Craft's own fill order in
    // compute_chunk, main.c:1077-1098).
    static constexpr int NeighborIndex(int dx, int dy, int dz) { return (dx + 1) * 9 + (dy + 1) * 3 + (dz + 1); }
};

}
