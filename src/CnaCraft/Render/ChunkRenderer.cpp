#include "ChunkRenderer.hpp"

#include <vector>

#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionNormalTexture.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "TextureAtlas.hpp"
#include "../Worlds/Chunk.hpp"
#include "../Worlds/ChunkMesher.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace CnaCraft::Render {

ChunkRenderer::ChunkRenderer(int chunkOriginX, int chunkOriginY, int chunkOriginZ)
    : originX_(chunkOriginX), originY_(chunkOriginY), originZ_(chunkOriginZ) {}

void ChunkRenderer::Rebuild(GraphicsDevice& device, const CnaCraft::Worlds::World& world) {
    const CnaCraft::Worlds::MeshData mesh =
        CnaCraft::Worlds::ChunkMesher::Build(world, originX_, originY_, originZ_);

    if (mesh.vertices.empty()) {
        vb_.reset();
        ib_.reset();
        primitiveCount_ = 0;
        return;
    }

    std::vector<VertexPositionNormalTexture> vertices;
    vertices.reserve(mesh.vertices.size());
    for (const auto& v : mesh.vertices) {
        float u = 0.0f, w = 0.0f;
        MapAtlasUv(v.tileIndex, v.u, v.v, u, w);
        vertices.emplace_back(Vector3(v.px, v.py, v.pz), Vector3(v.nx, v.ny, v.nz), Vector2(u, w));
    }

    vb_ = std::make_unique<VertexBuffer>(device, static_cast<int>(vertices.size()));
    vb_->SetData(vertices.data(), static_cast<int>(vertices.size()));

    ib_ = std::make_unique<IndexBuffer>(
        device, IndexElementSize::ThirtyTwoBits, static_cast<int>(mesh.indices.size()), BufferUsage::None);
    ib_->SetData(mesh.indices.data(), static_cast<int>(mesh.indices.size()));

    primitiveCount_ = static_cast<int>(mesh.indices.size() / 3);
}

void ChunkRenderer::Draw(GraphicsDevice& device, BasicEffect& effect) {
    if (!vb_ || !ib_) return;

    effect.World = Matrix::CreateTranslation(
        static_cast<float>(originX_), static_cast<float>(originY_), static_cast<float>(originZ_));

    for (auto& pass : effect.getCurrentTechniqueProperty()->getPassesProperty()) {
        pass.Apply();
        device.SetVertexBuffer(vb_.get());
        device.Indices(ib_.get());
        device.DrawIndexedPrimitives(
            PrimitiveType::TriangleList,
            /*baseVertex=*/0,
            /*minVertexIndex=*/0,
            /*numVertices=*/vb_->getVertexCountProperty(),
            /*startIndex=*/0,
            /*primitiveCount=*/primitiveCount_);
    }
}

BoundingBox ChunkRenderer::Bounds() const {
    const auto size = static_cast<float>(CnaCraft::Worlds::CHUNK_SIZE);
    return BoundingBox(
        Vector3(static_cast<float>(originX_), static_cast<float>(originY_), static_cast<float>(originZ_)),
        Vector3(static_cast<float>(originX_) + size, static_cast<float>(originY_) + size,
                static_cast<float>(originZ_) + size));
}

}
