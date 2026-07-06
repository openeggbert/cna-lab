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

void ChunkRenderer::UploadMesh(GraphicsDevice& device, const CnaCraft::Worlds::MeshData& mesh, MeshBuffers& buffers) {
    if (mesh.vertices.empty()) {
        buffers.vb.reset();
        buffers.ib.reset();
        buffers.primitiveCount = 0;
        return;
    }

    std::vector<VertexPositionNormalTexture> vertices;
    vertices.reserve(mesh.vertices.size());
    for (const auto& v : mesh.vertices) {
        float u = 0.0f, w = 0.0f;
        MapAtlasUv(v.tileIndex, v.u, v.v, u, w);
        vertices.emplace_back(Vector3(v.px, v.py, v.pz), Vector3(v.nx, v.ny, v.nz), Vector2(u, w));
    }

    buffers.vb = std::make_unique<VertexBuffer>(device, static_cast<int>(vertices.size()));
    buffers.vb->SetData(vertices.data(), static_cast<int>(vertices.size()));

    buffers.ib = std::make_unique<IndexBuffer>(
        device, IndexElementSize::ThirtyTwoBits, static_cast<int>(mesh.indices.size()), BufferUsage::None);
    buffers.ib->SetData(mesh.indices.data(), static_cast<int>(mesh.indices.size()));

    buffers.primitiveCount = static_cast<int>(mesh.indices.size() / 3);
}

void ChunkRenderer::Rebuild(GraphicsDevice& device, const CnaCraft::Worlds::World& world) {
    const CnaCraft::Worlds::ChunkMeshData mesh =
        CnaCraft::Worlds::ChunkMesher::Build(world, originX_, originY_, originZ_);
    UploadMesh(device, mesh.opaque, opaque_);
    UploadMesh(device, mesh.transparent, transparent_);
}

void ChunkRenderer::Draw(GraphicsDevice& device, BasicEffect& effect, const MeshBuffers& buffers) {
    if (!buffers.vb || !buffers.ib) return;

    effect.World = Matrix::CreateTranslation(
        static_cast<float>(originX_), static_cast<float>(originY_), static_cast<float>(originZ_));

    for (auto& pass : effect.getCurrentTechniqueProperty()->getPassesProperty()) {
        pass.Apply();
        device.SetVertexBuffer(buffers.vb.get());
        device.Indices(buffers.ib.get());
        device.DrawIndexedPrimitives(
            PrimitiveType::TriangleList,
            /*baseVertex=*/0,
            /*minVertexIndex=*/0,
            /*numVertices=*/buffers.vb->getVertexCountProperty(),
            /*startIndex=*/0,
            /*primitiveCount=*/buffers.primitiveCount);
    }
}

void ChunkRenderer::DrawOpaque(GraphicsDevice& device, BasicEffect& effect) {
    Draw(device, effect, opaque_);
}

void ChunkRenderer::DrawTransparent(GraphicsDevice& device, BasicEffect& effect) {
    Draw(device, effect, transparent_);
}

BoundingBox ChunkRenderer::Bounds() const {
    const auto size = static_cast<float>(CnaCraft::Worlds::CHUNK_SIZE);
    return BoundingBox(
        Vector3(static_cast<float>(originX_), static_cast<float>(originY_), static_cast<float>(originZ_)),
        Vector3(static_cast<float>(originX_) + size, static_cast<float>(originY_) + size,
                static_cast<float>(originZ_) + size));
}

}
