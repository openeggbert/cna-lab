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
};

}
