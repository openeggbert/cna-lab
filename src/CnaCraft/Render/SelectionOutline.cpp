#include "SelectionOutline.hpp"

#include <cstdint>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace CnaCraft::Render {

namespace {
// Craft's own render_wireframe uses n=0.53 (main.c) -- slightly larger than
// the block's true 0.5 half-size, so the outline sits just outside the
// block's own faces instead of z-fighting with them.
constexpr float kHalfSize = 0.53f;
}

void SelectionOutline::Update(GraphicsDevice& device, int bx, int by, int bz) {
    const float cx = static_cast<float>(bx) + 0.5f;
    const float cy = static_cast<float>(by) + 0.5f;
    const float cz = static_cast<float>(bz) + 0.5f;
    const float x0 = cx - kHalfSize, x1 = cx + kHalfSize;
    const float y0 = cy - kHalfSize, y1 = cy + kHalfSize;
    const float z0 = cz - kHalfSize, z1 = cz + kHalfSize;

    const Color edgeColor(0, 0, 0, 255);
    const VertexPositionColor verts[8] = {
        {Vector3(x0, y0, z0), edgeColor}, {Vector3(x1, y0, z0), edgeColor},
        {Vector3(x1, y0, z1), edgeColor}, {Vector3(x0, y0, z1), edgeColor},
        {Vector3(x0, y1, z0), edgeColor}, {Vector3(x1, y1, z0), edgeColor},
        {Vector3(x1, y1, z1), edgeColor}, {Vector3(x0, y1, z1), edgeColor},
    };

    if (!vb_) {
        // 12 edges of a box => 24 indices (line list); built once, only the
        // vertex positions change frame to frame as the target block moves.
        static constexpr std::uint16_t kIndices[24] = {
            0, 1, 1, 2, 2, 3, 3, 0, // bottom
            4, 5, 5, 6, 6, 7, 7, 4, // top
            0, 4, 1, 5, 2, 6, 3, 7, // verticals
        };
        vb_ = std::make_unique<VertexBuffer>(device, 8);
        ib_ = std::make_unique<IndexBuffer>(device, IndexElementSize::SixteenBits, 24, BufferUsage::None);
        ib_->SetData(kIndices, 24);
    }
    vb_->SetData(verts, 8);
}

void SelectionOutline::Draw(GraphicsDevice& device, BasicEffect& effect) {
    if (!vb_ || !ib_) return;

    effect.World = Matrix::getIdentityProperty();
    for (auto& pass : effect.getCurrentTechniqueProperty()->getPassesProperty()) {
        pass.Apply();
        device.SetVertexBuffer(vb_.get());
        device.Indices(ib_.get());
        device.DrawIndexedPrimitives(
            PrimitiveType::LineList,
            /*baseVertex=*/0,
            /*minVertexIndex=*/0,
            /*numVertices=*/8,
            /*startIndex=*/0,
            /*primitiveCount=*/12);
    }
}

}
