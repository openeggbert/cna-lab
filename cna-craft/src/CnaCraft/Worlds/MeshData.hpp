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
    // Baked static Craft light factor (CRAFT_PARITY.md §5.1, plan.md §12.1
    // item 12): (1 + df) * (0.3 + (1 - ao) * 0.7), in [0.3, 2.0] -- df is
    // Craft's fixed-direction per-face diffuse, ao the per-vertex occlusion
    // from ChunkMesher::ComputeOcclusion (cloud faces carry Craft's
    // contrast-compressed variant). With no torch-light propagation (the
    // item-27 glow-pass pivot), this is the ENTIRE time-independent half of
    // Craft's block_fragment.glsl lighting; the time-dependent half stays a
    // live per-frame uniform: the renderer uploads shade/2 as the 8-bit
    // vertex color and BasicEffect::DiffuseColor = 2*(daylight*0.3+0.2)
    // restores the *2 -- see ChunkRenderer::UploadMesh.
    float shade = 1.0f;
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
