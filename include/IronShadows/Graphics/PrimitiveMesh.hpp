#pragma once

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace IronShadows
{
    class MeshBuilder final
    {
    public:
        void AddBox(const Microsoft::Xna::Framework::Vector3& center,
                    const Microsoft::Xna::Framework::Vector3& size,
                    const Microsoft::Xna::Framework::Color& color);

        [[nodiscard]] const std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionColor>&
        GetVertices() const noexcept { return vertices_; }
        [[nodiscard]] const std::vector<std::uint16_t>& GetIndices() const noexcept { return indices_; }

    private:
        std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionColor> vertices_;
        std::vector<std::uint16_t> indices_;
    };

    class PrimitiveMesh final
    {
    public:
        void Upload(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                    const MeshBuilder& builder);
        void Draw(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device) const;
        [[nodiscard]] bool IsReady() const noexcept { return vertexBuffer_ != nullptr && indexBuffer_ != nullptr; }

    private:
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> vertexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> indexBuffer_;
        int primitiveCount_{0};
    };
}
