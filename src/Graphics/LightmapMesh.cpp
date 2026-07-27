#include "IronShadows/Graphics/LightmapMesh.hpp"

#include "IronShadows/Graphics/SunLight.hpp"

#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexElementSize.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace IronShadows
{
    using namespace Microsoft::Xna::Framework;
    using namespace Microsoft::Xna::Framework::Graphics;

    void LightmapMeshBuilder::AddFace(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3,
                                      const Vector3& normal, const Color& color)
    {
        if (vertices_.size() + 4U > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()))
        {
            throw std::runtime_error("Lightmap city mesh exceeded the 16-bit index limit");
        }

        const int tileIndex = static_cast<int>(tileBrightness_.size());
        tileBrightness_.push_back(ComputeBrightnessForNormal(normal));

        const std::uint16_t base = static_cast<std::uint16_t>(vertices_.size());
        // UVs are placeholders here (0,0) -- Finalize() resolves the real ones once the total
        // tile count (and therefore the atlas height) is known.
        vertices_.insert(vertices_.end(), {
            {p0, color, Vector2(0.0F, 0.0F)},
            {p1, color, Vector2(0.0F, 0.0F)},
            {p2, color, Vector2(0.0F, 0.0F)},
            {p3, color, Vector2(0.0F, 0.0F)},
        });
        vertexTileIndex_.insert(vertexTileIndex_.end(), {tileIndex, tileIndex, tileIndex, tileIndex});

        // RasterizerState::CullNone is set for this renderer (PrototypeRenderer::Initialize()), so
        // triangle winding never affects visibility -- a single consistent (0,1,2)/(0,2,3)
        // triangulation is enough for every face regardless of which way it faces.
        indices_.insert(indices_.end(), {
            static_cast<std::uint16_t>(base + 0), static_cast<std::uint16_t>(base + 1),
            static_cast<std::uint16_t>(base + 2),
            static_cast<std::uint16_t>(base + 0), static_cast<std::uint16_t>(base + 2),
            static_cast<std::uint16_t>(base + 3),
        });
    }

    void LightmapMeshBuilder::AddBox(const Vector3& center, const Vector3& size, const Color& color)
    {
        const float hx = size.X * 0.5F;
        const float hy = size.Y * 0.5F;
        const float hz = size.Z * 0.5F;

        const Vector3 p0(center.X - hx, center.Y - hy, center.Z - hz);
        const Vector3 p1(center.X + hx, center.Y - hy, center.Z - hz);
        const Vector3 p2(center.X + hx, center.Y + hy, center.Z - hz);
        const Vector3 p3(center.X - hx, center.Y + hy, center.Z - hz);
        const Vector3 p4(center.X - hx, center.Y - hy, center.Z + hz);
        const Vector3 p5(center.X + hx, center.Y - hy, center.Z + hz);
        const Vector3 p6(center.X + hx, center.Y + hy, center.Z + hz);
        const Vector3 p7(center.X - hx, center.Y + hy, center.Z + hz);

        AddFace(p0, p1, p2, p3, Vector3(0.0F, 0.0F, -1.0F), color); // front (-Z)
        AddFace(p5, p4, p7, p6, Vector3(0.0F, 0.0F, 1.0F), color);  // back (+Z)
        AddFace(p4, p0, p3, p7, Vector3(-1.0F, 0.0F, 0.0F), color); // left (-X)
        AddFace(p1, p5, p6, p2, Vector3(1.0F, 0.0F, 0.0F), color);  // right (+X)
        AddFace(p4, p5, p1, p0, Vector3(0.0F, -1.0F, 0.0F), color); // bottom (-Y)
        AddFace(p3, p2, p6, p7, Vector3(0.0F, 1.0F, 0.0F), color);  // top (+Y)
    }

    void LightmapMeshBuilder::Finalize()
    {
        const int tileCount = static_cast<int>(tileBrightness_.size());
        const int rows = std::max(1, (tileCount + kLightmapAtlasColumns - 1) / kLightmapAtlasColumns);
        atlasHeight_ = rows * kLightmapTileSize;
        const int atlasWidth = GetAtlasWidth();

        atlasPixels_.assign(static_cast<std::size_t>(atlasWidth) * static_cast<std::size_t>(atlasHeight_),
                            Color(255, 255, 255, 255));
        for (int tileIndex = 0; tileIndex < tileCount; ++tileIndex)
        {
            const int column = tileIndex % kLightmapAtlasColumns;
            const int row = tileIndex / kLightmapAtlasColumns;
            const auto level = static_cast<std::uint8_t>(
                std::clamp(tileBrightness_[static_cast<std::size_t>(tileIndex)], 0.0F, 1.0F) * 255.0F);
            constexpr std::uint8_t kOpaque = 255;
            const Color tileColor(level, level, level, kOpaque);
            for (int y = 0; y < kLightmapTileSize; ++y)
            {
                for (int x = 0; x < kLightmapTileSize; ++x)
                {
                    const std::size_t pixelIndex =
                        static_cast<std::size_t>(row * kLightmapTileSize + y) * static_cast<std::size_t>(atlasWidth) +
                        static_cast<std::size_t>(column * kLightmapTileSize + x);
                    atlasPixels_[pixelIndex] = tileColor;
                }
            }
        }

        for (std::size_t i = 0; i < vertices_.size(); ++i)
        {
            const int tileIndex = vertexTileIndex_[i];
            const int column = tileIndex % kLightmapAtlasColumns;
            const int row = tileIndex / kLightmapAtlasColumns;
            // Sample the exact center of the tile so bilinear filtering can never bleed into a
            // neighboring tile, regardless of tile size.
            const float u = (static_cast<float>(column) * kLightmapTileSize + kLightmapTileSize * 0.5F) /
                            static_cast<float>(atlasWidth);
            const float v = (static_cast<float>(row) * kLightmapTileSize + kLightmapTileSize * 0.5F) /
                            static_cast<float>(atlasHeight_);
            vertices_[i].TextureCoordinate = Vector2(u, v);
        }
    }

    void LightmapPrimitiveMesh::Upload(GraphicsDevice& device, const LightmapMeshBuilder& builder)
    {
        const auto& vertices = builder.GetVertices();
        const auto& indices = builder.GetIndices();
        if (vertices.empty() || indices.empty())
        {
            throw std::runtime_error("Cannot upload an empty lightmap primitive mesh");
        }

        vertexBuffer_ = std::make_unique<VertexBuffer>(
            device,
            VertexPositionColorTexture::getVertexDeclarationStatic(),
            static_cast<int>(vertices.size()),
            BufferUsage::None);
        vertexBuffer_->SetData(vertices.data(), static_cast<int>(vertices.size()));

        indexBuffer_ = std::make_unique<IndexBuffer>(
            device,
            IndexElementSize::SixteenBits,
            static_cast<int>(indices.size()),
            BufferUsage::None);
        indexBuffer_->SetData(indices.data(), static_cast<int>(indices.size()));
        primitiveCount_ = static_cast<int>(indices.size() / 3U);
    }

    void LightmapPrimitiveMesh::Draw(GraphicsDevice& device) const
    {
        if (!IsReady())
        {
            return;
        }
        device.SetVertexBuffer(vertexBuffer_.get());
        device.Indices(indexBuffer_.get());
        device.DrawIndexedPrimitives(
            PrimitiveType::TriangleList,
            0,
            0,
            vertexBuffer_->getVertexCountProperty(),
            0,
            primitiveCount_);
    }
}
