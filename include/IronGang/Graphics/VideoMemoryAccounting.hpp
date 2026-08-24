#pragma once

#include <cstddef>
#include <unordered_set>

namespace Microsoft::Xna::Framework::Graphics
{
    class Effect;
    class EffectParameter;
    class IndexBuffer;
    class Model;
    class Texture2D;
    class Texture3D;
    class TextureCube;
    class VertexBuffer;
    enum class SurfaceFormat;
}

namespace IronGang
{
    struct VideoMemoryBreakdown
    {
        std::size_t bufferBytes{0};
        std::size_t textureBytes{0};

        [[nodiscard]] std::size_t TotalBytes() const noexcept { return bufferBytes + textureBytes; }
    };

    // Exact logical storage for a texture including its mip chain. faces is 6 for a cube map and
    // 1 otherwise; depth is halved at each level for a 3D texture. Backend padding is deliberately
    // outside this public-API calculation and remains part of the report's incomplete coverage.
    [[nodiscard]] std::size_t CalculateTextureStorageBytes(
        int width,
        int height,
        int depth,
        int faces,
        int levelCount,
        Microsoft::Xna::Framework::Graphics::SurfaceFormat format);

    // Traverses public CNA Model resources and deduplicates shared buffers, effects, and textures
    // by object identity. One accumulator can consume every loaded model before returning a total.
    class VideoMemoryAccumulator final
    {
    public:
        void AddModel(const Microsoft::Xna::Framework::Graphics::Model& model);
        [[nodiscard]] const VideoMemoryBreakdown& GetBreakdown() const noexcept { return breakdown_; }

    private:
        void AddVertexBuffer(const Microsoft::Xna::Framework::Graphics::VertexBuffer* buffer);
        void AddIndexBuffer(const Microsoft::Xna::Framework::Graphics::IndexBuffer* buffer);
        void AddEffect(const Microsoft::Xna::Framework::Graphics::Effect* effect);
        void AddEffectParameter(const Microsoft::Xna::Framework::Graphics::EffectParameter& parameter);
        void AddTexture(const Microsoft::Xna::Framework::Graphics::Texture2D* texture);
        void AddTexture(const Microsoft::Xna::Framework::Graphics::Texture3D* texture);
        void AddTexture(const Microsoft::Xna::Framework::Graphics::TextureCube* texture);

        VideoMemoryBreakdown breakdown_;
        std::unordered_set<const Microsoft::Xna::Framework::Graphics::VertexBuffer*> vertexBuffers_;
        std::unordered_set<const Microsoft::Xna::Framework::Graphics::IndexBuffer*> indexBuffers_;
        std::unordered_set<const Microsoft::Xna::Framework::Graphics::Effect*> effects_;
        std::unordered_set<const Microsoft::Xna::Framework::Graphics::Texture2D*> textures2D_;
        std::unordered_set<const Microsoft::Xna::Framework::Graphics::Texture3D*> textures3D_;
        std::unordered_set<const Microsoft::Xna::Framework::Graphics::TextureCube*> textureCubes_;
    };
}
