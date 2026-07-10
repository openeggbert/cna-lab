#include "SignBillboard.hpp"

#include <cstdint>
#include <vector>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionNormalTexture.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "BitmapFont.hpp"
#include "../Worlds/Sign.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace CnaCraft::Render {

namespace {
constexpr float kEps = 0.02f;  // sits just outside the block's own face, avoids z-fighting
constexpr float kHalf = 0.4f;  // quad half-size -- 80% of the block face

struct FaceQuad {
    Vector3 corners[4]; // wound (0,1,2,3) around the perimeter
    Vector3 normal;
};

// Face-local quad corners (CRAFT_PARITY.md §4.3, see Sign.hpp for the
// face-index convention: 0=+X,1=-X,2=+Y/top,3=-Y/bottom,4=+Z,5=-Z). Block
// occupies the local unit cube [0,1]^3; corners sit on that face's plane,
// centered, offset outward by kEps.
const FaceQuad kFaceQuads[6] = {
    // +X
    {{Vector3(1 + kEps, 0.5f - kHalf, 0.5f - kHalf), Vector3(1 + kEps, 0.5f - kHalf, 0.5f + kHalf),
      Vector3(1 + kEps, 0.5f + kHalf, 0.5f + kHalf), Vector3(1 + kEps, 0.5f + kHalf, 0.5f - kHalf)},
     Vector3(1, 0, 0)},
    // -X
    {{Vector3(-kEps, 0.5f - kHalf, 0.5f + kHalf), Vector3(-kEps, 0.5f - kHalf, 0.5f - kHalf),
      Vector3(-kEps, 0.5f + kHalf, 0.5f - kHalf), Vector3(-kEps, 0.5f + kHalf, 0.5f + kHalf)},
     Vector3(-1, 0, 0)},
    // +Y (top)
    {{Vector3(0.5f - kHalf, 1 + kEps, 0.5f - kHalf), Vector3(0.5f + kHalf, 1 + kEps, 0.5f - kHalf),
      Vector3(0.5f + kHalf, 1 + kEps, 0.5f + kHalf), Vector3(0.5f - kHalf, 1 + kEps, 0.5f + kHalf)},
     Vector3(0, 1, 0)},
    // -Y (bottom)
    {{Vector3(0.5f - kHalf, -kEps, 0.5f + kHalf), Vector3(0.5f + kHalf, -kEps, 0.5f + kHalf),
      Vector3(0.5f + kHalf, -kEps, 0.5f - kHalf), Vector3(0.5f - kHalf, -kEps, 0.5f - kHalf)},
     Vector3(0, -1, 0)},
    // +Z
    {{Vector3(0.5f - kHalf, 0.5f - kHalf, 1 + kEps), Vector3(0.5f + kHalf, 0.5f - kHalf, 1 + kEps),
      Vector3(0.5f + kHalf, 0.5f + kHalf, 1 + kEps), Vector3(0.5f - kHalf, 0.5f + kHalf, 1 + kEps)},
     Vector3(0, 0, 1)},
    // -Z
    {{Vector3(0.5f + kHalf, 0.5f - kHalf, -kEps), Vector3(0.5f - kHalf, 0.5f - kHalf, -kEps),
      Vector3(0.5f - kHalf, 0.5f + kHalf, -kEps), Vector3(0.5f + kHalf, 0.5f + kHalf, -kEps)},
     Vector3(0, 0, -1)},
};

constexpr int kSignTexWidth = 128;
constexpr int kSignTexHeight = 32;

Texture2D BuildSignTexture(GraphicsDevice& device, const std::string& text) {
    std::vector<std::uint8_t> px(static_cast<std::size_t>(kSignTexWidth) * kSignTexHeight * 4);
    // Wood-plank-ish backdrop so the sign reads clearly against any terrain.
    for (int i = 0; i < kSignTexWidth * kSignTexHeight; ++i) {
        px[static_cast<std::size_t>(i) * 4 + 0] = 130;
        px[static_cast<std::size_t>(i) * 4 + 1] = 95;
        px[static_cast<std::size_t>(i) * 4 + 2] = 55;
        px[static_cast<std::size_t>(i) * 4 + 3] = 235;
    }
    FontDrawText(px, kSignTexWidth, kSignTexHeight, 3, 4, text, 255, 245, 220, 255, 1);

    Texture2D texture(device, kSignTexWidth, kSignTexHeight);
    texture.SetDataRGBA(px.data(), kSignTexWidth * kSignTexHeight);
    return texture;
}
}

void SignBillboard::Rebuild(GraphicsDevice& device, const std::vector<Worlds::Sign>& signs) {
    quads_.clear();
    quads_.reserve(signs.size());

    for (const Worlds::Sign& sign : signs) {
        if (sign.face < 0 || sign.face > 5) continue;
        const FaceQuad& face = kFaceQuads[sign.face];

        VertexPositionNormalTexture verts[4] = {
            {Vector3(static_cast<float>(sign.x) + face.corners[0].X, static_cast<float>(sign.y) + face.corners[0].Y,
                     static_cast<float>(sign.z) + face.corners[0].Z),
             face.normal, Vector2(0, 1)},
            {Vector3(static_cast<float>(sign.x) + face.corners[1].X, static_cast<float>(sign.y) + face.corners[1].Y,
                     static_cast<float>(sign.z) + face.corners[1].Z),
             face.normal, Vector2(1, 1)},
            {Vector3(static_cast<float>(sign.x) + face.corners[2].X, static_cast<float>(sign.y) + face.corners[2].Y,
                     static_cast<float>(sign.z) + face.corners[2].Z),
             face.normal, Vector2(1, 0)},
            {Vector3(static_cast<float>(sign.x) + face.corners[3].X, static_cast<float>(sign.y) + face.corners[3].Y,
                     static_cast<float>(sign.z) + face.corners[3].Z),
             face.normal, Vector2(0, 0)},
        };
        // Both windings, so the sign is visible from either side (see the
        // class comment in SignBillboard.hpp for why).
        const std::uint16_t indices[12] = {0, 1, 2, 0, 2, 3, 0, 2, 1, 0, 3, 2};

        SignQuad quad;
        quad.vb = std::make_unique<VertexBuffer>(device, 4);
        quad.vb->SetData(verts, 4);
        quad.ib = std::make_unique<IndexBuffer>(device, IndexElementSize::SixteenBits, 12, BufferUsage::None);
        quad.ib->SetData(indices, 12);
        quad.texture = std::make_unique<Texture2D>(BuildSignTexture(device, sign.text));
        quads_.push_back(std::move(quad));
    }
}

void SignBillboard::Draw(GraphicsDevice& device, BasicEffect& effect) {
    if (quads_.empty()) return;

    effect.World = Matrix::getIdentityProperty();
    for (auto& quad : quads_) {
        effect.setTextureProperty(quad.texture.get());
        for (auto& pass : effect.getCurrentTechniqueProperty()->getPassesProperty()) {
            pass.Apply();
            device.SetVertexBuffer(quad.vb.get());
            device.Indices(quad.ib.get());
            device.DrawIndexedPrimitives(
                PrimitiveType::TriangleList,
                /*baseVertex=*/0,
                /*minVertexIndex=*/0,
                /*numVertices=*/4,
                /*startIndex=*/0,
                /*primitiveCount=*/4);
        }
    }
}

}
