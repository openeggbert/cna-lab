#include "SkyDome.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColorTexture.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace CnaCraft::Render {

namespace {
// Full sphere now (was a hemisphere in the gradient version): rings run
// nadir -> zenith so looking below the horizon shows Craft's real
// below-horizon sky band instead of falling through to the clear color.
constexpr int kRings = 12;    // nadir (ring 0) to zenith (ring kRings)
constexpr int kSegments = 16; // around the Y axis
constexpr float kRadius = 400.0f; // comfortably inside the 500-unit far clip plane
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;
}

void SkyDome::Update(GraphicsDevice& device, float timeOfDay) {
    std::vector<VertexPositionColorTexture> vertices;
    vertices.reserve(static_cast<std::size_t>(kRings + 1) * (kSegments + 1));

    const Color white(255, 255, 255, 255);
    for (int r = 0; r <= kRings; ++r) {
        // latitude from -pi/2 (nadir) to +pi/2 (zenith); the V coordinate is
        // Craft's own sphere texcoord `t = 1 - acos(y)/pi`, which for
        // y = sin(latitude) simplifies to 0.5 + latitude/pi: 0 at the
        // nadir, 0.5 at the horizon, 1 at the zenith.
        const float latitude = -kPi / 2.0f + (static_cast<float>(r) / static_cast<float>(kRings)) * kPi;
        const float y = std::sin(latitude);
        const float ringRadius = std::cos(latitude);
        const float v = 0.5f + latitude / kPi;

        for (int s = 0; s <= kSegments; ++s) {
            const float theta = (static_cast<float>(s) / static_cast<float>(kSegments)) * kTwoPi;
            const float x = ringRadius * std::cos(theta);
            const float z = ringRadius * std::sin(theta);
            // U = the frame's time of day, identical for every vertex --
            // Craft passes this as the `timer` shader uniform; BasicEffect
            // has no such slot, so it's baked per-vertex and re-uploaded
            // (see the class comment).
            vertices.emplace_back(Vector3(x, y, z), white, Vector2(timeOfDay, v));
        }
    }

    if (!vb_) {
        std::vector<std::uint16_t> indices;
        indices.reserve(static_cast<std::size_t>(kRings) * kSegments * 6);
        for (int r = 0; r < kRings; ++r) {
            for (int s = 0; s < kSegments; ++s) {
                const auto v00 = static_cast<std::uint16_t>(r * (kSegments + 1) + s);
                const auto v01 = static_cast<std::uint16_t>(r * (kSegments + 1) + s + 1);
                const auto v10 = static_cast<std::uint16_t>((r + 1) * (kSegments + 1) + s);
                const auto v11 = static_cast<std::uint16_t>((r + 1) * (kSegments + 1) + s + 1);
                // Wound so the visible face points inward (the camera sits
                // at the dome's center, looking outward at the interior
                // surface) -- opposite winding from an ordinary outward-
                // facing sphere. CNA's default RasterizerState culls
                // CullCounterClockwiseFace (RasterizerState.cpp), so the
                // *visible* winding here must render as CW in screen space
                // -- verified empirically via a real EasyGL build back when
                // this was the gradient hemisphere (a naive "CCW from
                // outside" guess rendered nothing at all); the ring/segment
                // parametrization (y increasing with r) is unchanged, so the
                // winding carries over to the full sphere as-is.
                indices.push_back(v00);
                indices.push_back(v10);
                indices.push_back(v11);
                indices.push_back(v00);
                indices.push_back(v11);
                indices.push_back(v01);
            }
        }
        vb_ = std::make_unique<VertexBuffer>(device, static_cast<int>(vertices.size()));
        ib_ = std::make_unique<IndexBuffer>(device, IndexElementSize::SixteenBits,
                                             static_cast<int>(indices.size()), BufferUsage::None);
        ib_->SetData(indices.data(), static_cast<int>(indices.size()));
        primitiveCount_ = static_cast<int>(indices.size() / 3);
    }
    vb_->SetData(vertices.data(), static_cast<int>(vertices.size()));
}

void SkyDome::Draw(GraphicsDevice& device, BasicEffect& effect, const Vector3& cameraPosition) {
    if (!vb_ || !ib_) return;

    effect.World = Matrix::CreateScale(kRadius) * Matrix::CreateTranslation(cameraPosition);
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

}
