#pragma once

#include <memory>

#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"

namespace Microsoft::Xna::Framework::Graphics {
class GraphicsDevice;
class BasicEffect;
}

namespace CnaCraft::Worlds {
class World;
}

namespace CnaCraft::Render {

// Owns one VertexBuffer + IndexBuffer for a single chunk's mesh. Rebuild()
// re-runs ChunkMesher and re-uploads both buffers; the caller (CnaCraftGame)
// only calls it when the chunk's dirty flag is set (plan.md §5).
class ChunkRenderer {
public:
    ChunkRenderer(int chunkOriginX, int chunkOriginY, int chunkOriginZ);

    void Rebuild(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                 const CnaCraft::Worlds::World& world);

    void Draw(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
              Microsoft::Xna::Framework::Graphics::BasicEffect& effect);

private:
    int originX_;
    int originY_;
    int originZ_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> vb_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> ib_;
    int primitiveCount_ = 0;
};

}
