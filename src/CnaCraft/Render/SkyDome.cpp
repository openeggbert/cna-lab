#include "SkyDome.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace CnaCraft::Render {

namespace {
constexpr int kRings = 6;     // horizon (ring 0) to zenith (ring kRings)
constexpr int kSegments = 16; // around the Y axis
constexpr float kRadius = 400.0f; // comfortably inside the 500-unit far clip plane
constexpr float kHalfPi = 1.57079632679489661923f;
constexpr float kTwoPi = 6.28318530717958647692f;
}

void SkyDome::Update(GraphicsDevice& device, const Color& horizonColor, const Color& zenithColor) {
    std::vector<VertexPositionColor> vertices;
    vertices.reserve(static_cast<std::size_t>(kRings + 1) * (kSegments + 1));

    for (int r = 0; r <= kRings; ++r) {
        const float t = static_cast<float>(r) / static_cast<float>(kRings); // 0=horizon, 1=zenith
        const float angle = t * kHalfPi;
        const float y = std::sin(angle);
        const float ringRadius = std::cos(angle);

        const auto lerpByte = [t](std::uint8_t from, std::uint8_t to) {
            return static_cast<std::uint8_t>(static_cast<float>(from) + (static_cast<float>(to) - static_cast<float>(from)) * t);
        };
        const Color ringColor(lerpByte(horizonColor.getRProperty(), zenithColor.getRProperty()),
                               lerpByte(horizonColor.getGProperty(), zenithColor.getGProperty()),
                               lerpByte(horizonColor.getBProperty(), zenithColor.getBProperty()),
                               std::uint8_t{255});

        for (int s = 0; s <= kSegments; ++s) {
            const float theta = (static_cast<float>(s) / static_cast<float>(kSegments)) * kTwoPi;
            const float x = ringRadius * std::cos(theta);
            const float z = ringRadius * std::sin(theta);
            vertices.emplace_back(Vector3(x, y, z), ringColor);
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
                // -- verified empirically via a real EasyGL build (a naive
                // "CCW from outside" guess rendered nothing at all).
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
