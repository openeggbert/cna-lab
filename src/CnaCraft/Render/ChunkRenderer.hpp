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

    void Rebuild(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                 const CnaCraft::Worlds::World& world);

    void DrawOpaque(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                     Microsoft::Xna::Framework::Graphics::BasicEffect& effect);
    void DrawTransparent(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                          Microsoft::Xna::Framework::Graphics::BasicEffect& effect);

    // World-space AABB of this chunk, for frustum culling (plan.md §11.2) —
    // the caller (CnaCraftGame::Draw) decides whether to call Draw() at all.
    [[nodiscard]] Microsoft::Xna::Framework::BoundingBox Bounds() const;

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

    int originX_;
    int originY_;
    int originZ_;
    MeshBuffers opaque_;
    MeshBuffers transparent_;
};

}
