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

// Light-toggle glow vertex (CRAFT_PARITY.md §2.7/§4.3, plan.md §12.1 item 17
// follow-up) — deliberately no normal: this mesh is drawn unlit (fixed
// bright vertex-color tint, day/night-independent), matching the one
// vertex-color+texture combo CNA's backends actually support without a
// custom shader (see ChunkMeshData's own doc comment below for why this
// couldn't just reuse `opaque`/MeshVertex's own lit-textured path).
struct GlowVertex {
    float px = 0.0f, py = 0.0f, pz = 0.0f;
    float u = 0.0f, v = 0.0f;
    int tileIndex = 0;
};

struct GlowMeshData {
    std::vector<GlowVertex> vertices;
    std::vector<std::uint32_t> indices;
};

// ChunkMesher::Build emits three independent meshes:
// - `opaque` drawn normally; `transparent` drawn last with alpha blending
//   on and depth writes off, so blocks like Glass show the scene behind
//   them instead of occluding it (plan.md §11.2 "Transparency for glass").
// - `glow`: an ADDITIVE third mesh (plan.md §12.1 item 17 follow-up,
//   "light toggle") -- a light-source block's exposed faces are emitted
//   here IN ADDITION TO their normal opaque/transparent emission (the
//   block still meshes completely normally), so the toggled block reads
//   as "lit" via a separate unlit bright-tinted draw pass layered on top.
//   Real per-neighbor light propagation (Craft's own flood-fill) isn't
//   possible here without new engine-level rendering work -- see
//   CRAFT_PARITY.md §2.7 for the full reasoning; this glow pass is a
//   deliberate, documented, self-contained substitute.
struct ChunkMeshData {
    MeshData opaque;
    MeshData transparent;
    GlowMeshData glow;
};

}
