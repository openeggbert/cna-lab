#include "PlayerCube.hpp"

#include <cmath>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColorTexture.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "TextureAtlas.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace CnaCraft::Render {

namespace {

// Craft's player cube half-size (make_player passes 0.4 to make_cube).
constexpr float kHalf = 0.4f;

// Same cube-face layout as ChunkMesher's kFaces, scaled to ±kHalf around
// the origin. UVs all map to kPlayerSkinTile; the flip below keeps the
// face's eyes upright on the side faces.
struct FaceDef {
    float corners[4][3];
};
constexpr FaceDef kFaces[6] = {
    {{{kHalf, -kHalf, -kHalf}, {kHalf, -kHalf, kHalf}, {kHalf, kHalf, kHalf}, {kHalf, kHalf, -kHalf}}},      // +X
    {{{-kHalf, -kHalf, kHalf}, {-kHalf, -kHalf, -kHalf}, {-kHalf, kHalf, -kHalf}, {-kHalf, kHalf, kHalf}}},  // -X
    {{{-kHalf, kHalf, -kHalf}, {kHalf, kHalf, -kHalf}, {kHalf, kHalf, kHalf}, {-kHalf, kHalf, kHalf}}},      // +Y
    {{{-kHalf, -kHalf, kHalf}, {kHalf, -kHalf, kHalf}, {kHalf, -kHalf, -kHalf}, {-kHalf, -kHalf, -kHalf}}},  // -Y
    {{{kHalf, -kHalf, kHalf}, {-kHalf, -kHalf, kHalf}, {-kHalf, kHalf, kHalf}, {kHalf, kHalf, kHalf}}},      // +Z
    {{{-kHalf, -kHalf, -kHalf}, {kHalf, -kHalf, -kHalf}, {kHalf, kHalf, -kHalf}, {-kHalf, kHalf, -kHalf}}},  // -Z
};
constexpr float kUv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};

}  // namespace

void PlayerCube::Update(GraphicsDevice& device, const std::vector<Instance>& players) {
    if (players.empty()) {
        vb_.reset();
        ib_.reset();
        primitiveCount_ = 0;
        return;
    }

    std::vector<VertexPositionColorTexture> vertices;
    std::vector<std::uint32_t> indices;
    vertices.reserve(players.size() * 24);
    indices.reserve(players.size() * 36);

    // Vertex color 127 ~= baked shade 1.0 under the terrain pass's
    // shade/2-then-DiffuseColor-doubles convention (ChunkRenderer) -- flat
    // daylight brightness, no AO, matching Craft's flat-lit player cubes.
    const Color kSkinShade(127, 127, 127, 255);

    for (const Instance& p : players) {
        // Craft's make_player rotation order: pitch (ry) around X, then
        // yaw (rx) around Y, then translate to the state position.
        const Matrix rotation =
            Matrix::CreateRotationX(p.pitch) * Matrix::CreateRotationY(-p.yaw);
        const Vector3 center(p.x, p.y, p.z);

        const auto base = static_cast<std::uint32_t>(vertices.size());
        for (const FaceDef& face : kFaces) {
            const auto faceBase = static_cast<std::uint32_t>(vertices.size());
            for (int c = 0; c < 4; ++c) {
                const Vector3 local(face.corners[c][0], face.corners[c][1], face.corners[c][2]);
                const Vector3 rotated = Vector3::Transform(local, rotation);
                float u = 0.0f, v = 0.0f;
                MapAtlasUv(kPlayerSkinTile, kUv[c][0], kUv[c][1], u, v);
                vertices.emplace_back(Vector3(center.X + rotated.X, center.Y + rotated.Y, center.Z + rotated.Z),
                                      kSkinShade, Vector2(u, v));
            }
            indices.push_back(faceBase + 0);
            indices.push_back(faceBase + 1);
            indices.push_back(faceBase + 2);
            indices.push_back(faceBase + 0);
            indices.push_back(faceBase + 2);
            indices.push_back(faceBase + 3);
        }
        (void)base;
    }

    vb_ = std::make_unique<VertexBuffer>(device, static_cast<int>(vertices.size()));
    vb_->SetData(vertices.data(), static_cast<int>(vertices.size()));
    ib_ = std::make_unique<IndexBuffer>(device, IndexElementSize::ThirtyTwoBits,
                                        static_cast<int>(indices.size()), BufferUsage::None);
    ib_->SetData(indices.data(), static_cast<int>(indices.size()));
    primitiveCount_ = static_cast<int>(indices.size() / 3);
}

void PlayerCube::Draw(GraphicsDevice& device, BasicEffect& effect) {
    if (!vb_ || !ib_ || primitiveCount_ == 0) return;
    effect.World = Matrix::getIdentityProperty();
    for (auto& pass : effect.getCurrentTechniqueProperty()->getPassesProperty()) {
        pass.Apply();
        device.SetVertexBuffer(vb_.get());
        device.Indices(ib_.get());
        device.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, vb_->getVertexCountProperty(), 0,
                                     primitiveCount_);
    }
}

}
