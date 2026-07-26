#include "IronShadows/Graphics/PrimitiveMesh.hpp"

#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexElementSize.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"

#include <limits>
#include <stdexcept>

namespace IronShadows
{
    using namespace Microsoft::Xna::Framework;
    using namespace Microsoft::Xna::Framework::Graphics;

    void MeshBuilder::AddBox(const Vector3& center, const Vector3& size, const Color& color)
    {
        if (vertices_.size() + 8U > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()))
        {
            throw std::runtime_error("Prototype mesh exceeded the 16-bit index limit");
        }

        const float hx = size.X * 0.5F;
        const float hy = size.Y * 0.5F;
        const float hz = size.Z * 0.5F;
        const std::uint16_t base = static_cast<std::uint16_t>(vertices_.size());

        vertices_.insert(vertices_.end(), {
            {{center.X - hx, center.Y - hy, center.Z - hz}, color},
            {{center.X + hx, center.Y - hy, center.Z - hz}, color},
            {{center.X + hx, center.Y + hy, center.Z - hz}, color},
            {{center.X - hx, center.Y + hy, center.Z - hz}, color},
            {{center.X - hx, center.Y - hy, center.Z + hz}, color},
            {{center.X + hx, center.Y - hy, center.Z + hz}, color},
            {{center.X + hx, center.Y + hy, center.Z + hz}, color},
            {{center.X - hx, center.Y + hy, center.Z + hz}, color}
        });

        constexpr std::uint16_t localIndices[] = {
            0, 2, 1, 0, 3, 2,
            4, 5, 6, 4, 6, 7,
            0, 4, 7, 0, 7, 3,
            1, 2, 6, 1, 6, 5,
            0, 1, 5, 0, 5, 4,
            3, 7, 6, 3, 6, 2
        };
        for (const std::uint16_t index : localIndices)
        {
            indices_.push_back(static_cast<std::uint16_t>(base + index));
        }
    }

    void PrimitiveMesh::Upload(GraphicsDevice& device, const MeshBuilder& builder)
    {
        const auto& vertices = builder.GetVertices();
        const auto& indices = builder.GetIndices();
        if (vertices.empty() || indices.empty())
        {
            throw std::runtime_error("Cannot upload an empty primitive mesh");
        }

        vertexBuffer_ = std::make_unique<VertexBuffer>(
            device,
            VertexPositionColor::getVertexDeclarationStatic(),
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

    void PrimitiveMesh::Draw(GraphicsDevice& device) const
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
