#pragma once

#include <memory>

#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"

namespace Microsoft::Xna::Framework::Graphics {
class GraphicsDevice;
class BasicEffect;
}

namespace CnaCraft::Worlds {
class World;
struct MeshData;
struct GlowMeshData;
struct ChunkMeshData;
}

namespace CnaCraft::Render {

// Owns two VertexBuffer + IndexBuffer pairs for a single chunk's mesh — one
// for opaque blocks, one for transparent ones like Glass (plan.md §11.2
// "Transparency for glass"). Rebuild() re-runs ChunkMesher and re-uploads
// both; the caller (CnaCraftGame) only calls it when the chunk's dirty flag
// is set (plan.md §5). DrawOpaque()/DrawTransparent() are separate calls so
// the caller can draw all chunks' opaque geometry first, then all chunks'
// transparent geometry last with blending on (mirrors house3d_demo.cpp's
// solid-then-glass draw order).
class ChunkRenderer {
public:
    ChunkRenderer(int chunkOriginX, int chunkOriginY, int chunkOriginZ);

    // Synchronous: computes the mesh (ChunkMesher::Build) and uploads it in
    // one call. Used only where synchronous meshing is actually wanted
    // (CnaCraftGame's spawn-area force-load in Initialize(), mirroring
    // Craft's own synchronous force_chunks) -- the steady-state per-frame
    // path backgrounds mesh computation instead (plan.md §12.1 item 19
    // phase 4) and calls ApplyMesh below with an already-computed result.
    void Rebuild(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                 const CnaCraft::Worlds::World& world);

    // Uploads an already-computed mesh (e.g. produced by a background
    // ChunkMesher::Build call) — the GPU-upload half of Rebuild, split out
    // so mesh computation (safe off the main thread, given a data
    // snapshot) and GPU upload (main-thread-only) can happen on different
    // threads.
    void ApplyMesh(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                    const CnaCraft::Worlds::ChunkMeshData& mesh);

    void DrawOpaque(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                     Microsoft::Xna::Framework::Graphics::BasicEffect& effect);
    void DrawTransparent(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                          Microsoft::Xna::Framework::Graphics::BasicEffect& effect);

    // Light-toggle glow pass (plan.md §12.1 item 17 follow-up) — drawn with
    // `effect`'s VertexColorEnabled+TextureEnabled, LightingEnabled=false
    // (the caller, CnaCraftGame::Draw, is responsible for that effect
    // flip/restore, same "temporarily reconfigure the shared BasicEffect"
    // pattern already used for SkyDome/SelectionOutline). Empty/no-op if
    // this chunk has no light-source blocks.
    void DrawGlow(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                  Microsoft::Xna::Framework::Graphics::BasicEffect& effect);

    // World-space AABB of this chunk, for frustum culling (plan.md §11.2) —
    // the caller (CnaCraftGame::Draw) decides whether to call Draw() at all.
    [[nodiscard]] Microsoft::Xna::Framework::BoundingBox Bounds() const;

    // True once this chunk has at least one light-source block's glow faces
    // uploaded (plan.md §12.1 item 27 follow-up). Lets the caller skip the
    // entire glow render pass -- map iteration and per-renderer frustum
    // tests included, not just the (already free-when-empty) draw call --
    // whenever no loaded chunk anywhere has any lit block, which is the
    // overwhelmingly common case (nobody has toggled a light yet).
    [[nodiscard]] bool HasGlow() const { return glow_.vb != nullptr; }

private:
    struct MeshBuffers {
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> vb;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> ib;
        int primitiveCount = 0;
    };

    void Draw(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
              Microsoft::Xna::Framework::Graphics::BasicEffect& effect, const MeshBuffers& buffers);
    static void UploadMesh(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                           const CnaCraft::Worlds::MeshData& mesh, MeshBuffers& buffers);
    static void UploadGlowMesh(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                                const CnaCraft::Worlds::GlowMeshData& mesh, MeshBuffers& buffers);

    int originX_;
    int originY_;
    int originZ_;
    MeshBuffers opaque_;
    MeshBuffers transparent_;
    MeshBuffers glow_;
};

}
