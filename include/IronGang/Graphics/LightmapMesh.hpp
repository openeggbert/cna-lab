#pragma once

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColorTexture.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace Microsoft::Xna::Framework::Graphics
{
    class GraphicsDevice;
}

namespace IronGang
{
    inline constexpr int kLightmapTileSize = 4;
    inline constexpr int kLightmapAtlasColumns = 32;

    // Gate M10 (plan_39 IG-39-011's own locked research note): builds the static city mesh (box
    // geometry only -- ground/roads/sidewalks/buildings/lamps; MC3-sourced models have no lightmap
    // UV channel and stay out of scope for this pass) with ONE flat-shaded lightmap tile per box
    // face, baked in-process from the shared sun direction (SunLight.hpp) -- not per-vertex vertex
    // color, a real lightmap texture sampled via a second UV channel (CNA's DualTextureEffect).
    // Unlike MeshBuilder::AddBox (which shares 8 vertices across all 6 faces, since every face has
    // the same flat vertex color), each face here needs its OWN 4 vertices, since each face needs
    // its own lightmap UV pointing at its own tile.
    class LightmapMeshBuilder
    {
    public:
        void AddBox(const Microsoft::Xna::Framework::Vector3& center,
                    const Microsoft::Xna::Framework::Vector3& size,
                    const Microsoft::Xna::Framework::Color& color);

        // Must be called exactly once, after every AddBox() call and before GetVertices()/
        // GetAtlasPixels()/GetAtlasHeight() are read: final vertex UVs and the atlas pixel buffer
        // both depend on the TOTAL tile count, which isn't known until construction finishes.
        void Finalize();

        [[nodiscard]] const std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionColorTexture>&
        GetVertices() const noexcept { return vertices_; }
        [[nodiscard]] const std::vector<std::uint16_t>& GetIndices() const noexcept { return indices_; }
        [[nodiscard]] const std::vector<Microsoft::Xna::Framework::Color>& GetAtlasPixels() const noexcept
        {
            return atlasPixels_;
        }
        [[nodiscard]] int GetAtlasWidth() const noexcept { return kLightmapAtlasColumns * kLightmapTileSize; }
        [[nodiscard]] int GetAtlasHeight() const noexcept { return atlasHeight_; }

    private:
        void AddFace(const Microsoft::Xna::Framework::Vector3& p0,
                     const Microsoft::Xna::Framework::Vector3& p1,
                     const Microsoft::Xna::Framework::Vector3& p2,
                     const Microsoft::Xna::Framework::Vector3& p3,
                     const Microsoft::Xna::Framework::Vector3& normal,
                     const Microsoft::Xna::Framework::Color& color);

        std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionColorTexture> vertices_;
        std::vector<int> vertexTileIndex_; // parallel to vertices_; resolved to UVs in Finalize()
        std::vector<std::uint16_t> indices_;
        std::vector<float> tileBrightness_; // one entry per allocated tile, in allocation order
        std::vector<Microsoft::Xna::Framework::Color> atlasPixels_;
        int atlasHeight_{0};
    };

    // The VertexPositionColorTexture equivalent of PrimitiveMesh (which is VertexPositionColor-
    // only) -- kept as a separate small class rather than templatizing PrimitiveMesh, since every
    // other mesh in this renderer (vehicle/player/traffic/pedestrian/police/shadow-decal) has no
    // use for a second UV channel.
    class LightmapPrimitiveMesh final
    {
    public:
        void Upload(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                    const LightmapMeshBuilder& builder);
        void Draw(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device) const;
        [[nodiscard]] bool IsReady() const noexcept { return vertexBuffer_ != nullptr && indexBuffer_ != nullptr; }
        [[nodiscard]] std::size_t GetTrackedVideoMemoryBytes() const noexcept { return trackedVideoMemoryBytes_; }

    private:
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> vertexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> indexBuffer_;
        int primitiveCount_{0};
        std::size_t trackedVideoMemoryBytes_{0};
    };
}
