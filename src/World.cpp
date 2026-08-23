#include "World.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexElementSize.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"

namespace WolfCna
{
    using namespace Microsoft::Xna::Framework;
    using namespace Microsoft::Xna::Framework::Graphics;

    namespace
    {
        constexpr float WallHeight = 1.0f;
    }

    World::World(const LevelDefinition& level)
        : map_(level.Rows())
        , playerStart_(
            static_cast<float>(level.PlayerStartX()) + 0.5f,
            0.62f,
            static_cast<float>(level.PlayerStartZ()) + 0.5f)
    {

        BuildMesh();
    }

    Vector3 World::PlayerStart() const
    {
        return playerStart_;
    }

    bool World::IsWallCell(int x, int z) const
    {
        if (z < 0 || z >= static_cast<int>(map_.size()))
            return true;
        if (x < 0 || x >= static_cast<int>(map_[z].size()))
            return true;
        return map_[z][x] == '#';
    }

    bool World::Collides(float worldX, float worldZ, float radius) const
    {
        const std::array<Vector2, 8> probes = {
            Vector2(-radius, -radius),
            Vector2( radius, -radius),
            Vector2(-radius,  radius),
            Vector2( radius,  radius),
            Vector2(-radius, 0.0f),
            Vector2( radius, 0.0f),
            Vector2(0.0f, -radius),
            Vector2(0.0f,  radius)
        };

        for (const Vector2& p : probes)
        {
            const int cellX = static_cast<int>(std::floor(worldX + p.X));
            const int cellZ = static_cast<int>(std::floor(worldZ + p.Y));
            if (IsWallCell(cellX, cellZ))
                return true;
        }

        return false;
    }

    void World::AddQuad(
        const Vector3& a,
        const Vector3& b,
        const Vector3& c,
        const Vector3& d,
        Material material)
    {
        if (vertices_.size() > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max() - 4))
            throw std::runtime_error("Starter world exceeded the 16-bit vertex index limit.");

        const auto base = static_cast<std::uint16_t>(vertices_.size());

        constexpr float panelWidth = 1.0f / 3.0f;
        const float u0 = panelWidth * static_cast<int>(material);
        const float u1 = u0 + panelWidth;

        vertices_.emplace_back(a, Vector2(u0, 1.0f));
        vertices_.emplace_back(b, Vector2(u1, 1.0f));
        vertices_.emplace_back(c, Vector2(u1, 0.0f));
        vertices_.emplace_back(d, Vector2(u0, 0.0f));

        indices_.push_back(base + 0);
        indices_.push_back(base + 1);
        indices_.push_back(base + 2);
        indices_.push_back(base + 0);
        indices_.push_back(base + 2);
        indices_.push_back(base + 3);
    }

    void World::BuildMesh()
    {
        for (int z = 0; z < static_cast<int>(map_.size()); ++z)
        {
            for (int x = 0; x < static_cast<int>(map_[z].size()); ++x)
            {
                if (!IsWallCell(x, z))
                {
                    const float x0 = static_cast<float>(x);
                    const float x1 = x0 + 1.0f;
                    const float z0 = static_cast<float>(z);
                    const float z1 = z0 + 1.0f;

                    // Floor.
                    AddQuad(
                        Vector3(x0, 0.0f, z1),
                        Vector3(x1, 0.0f, z1),
                        Vector3(x1, 0.0f, z0),
                        Vector3(x0, 0.0f, z0),
                        Material::Floor);

                    // Ceiling. CullNone is used in the starter, so winding is deliberately
                    // not relied upon yet.
                    AddQuad(
                        Vector3(x0, WallHeight, z0),
                        Vector3(x1, WallHeight, z0),
                        Vector3(x1, WallHeight, z1),
                        Vector3(x0, WallHeight, z1),
                        Material::Ceiling);

                    continue;
                }

                const float x0 = static_cast<float>(x);
                const float x1 = x0 + 1.0f;
                const float z0 = static_cast<float>(z);
                const float z1 = z0 + 1.0f;

                // Emit only wall faces that border open space.
                if (!IsWallCell(x, z - 1))
                {
                    AddQuad(
                        Vector3(x1, 0.0f, z0),
                        Vector3(x0, 0.0f, z0),
                        Vector3(x0, WallHeight, z0),
                        Vector3(x1, WallHeight, z0),
                        Material::Wall);
                }

                if (!IsWallCell(x, z + 1))
                {
                    AddQuad(
                        Vector3(x0, 0.0f, z1),
                        Vector3(x1, 0.0f, z1),
                        Vector3(x1, WallHeight, z1),
                        Vector3(x0, WallHeight, z1),
                        Material::Wall);
                }

                if (!IsWallCell(x - 1, z))
                {
                    AddQuad(
                        Vector3(x0, 0.0f, z0),
                        Vector3(x0, 0.0f, z1),
                        Vector3(x0, WallHeight, z1),
                        Vector3(x0, WallHeight, z0),
                        Material::Wall);
                }

                if (!IsWallCell(x + 1, z))
                {
                    AddQuad(
                        Vector3(x1, 0.0f, z1),
                        Vector3(x1, 0.0f, z0),
                        Vector3(x1, WallHeight, z0),
                        Vector3(x1, WallHeight, z1),
                        Material::Wall);
                }
            }
        }
    }

    void World::Upload(GraphicsDevice& device)
    {
        if (vertexBuffer_ || indexBuffer_)
            return;

        vertexBuffer_ = std::make_unique<VertexBuffer>(
            device,
            VertexPositionTexture::getVertexDeclarationStatic(),
            static_cast<int>(vertices_.size()),
            BufferUsage::None);

        vertexBuffer_->SetData(vertices_.data(), static_cast<int>(vertices_.size()));

        indexBuffer_ = std::make_unique<IndexBuffer>(
            device,
            IndexElementSize::SixteenBits,
            static_cast<int>(indices_.size()),
            BufferUsage::None);

        indexBuffer_->SetData(indices_.data(), static_cast<int>(indices_.size()));
    }

    void World::Draw(
        GraphicsDevice& device,
        BasicEffect& effect,
        const Matrix& view,
        const Matrix& projection,
        Texture2D& atlas)
    {
        if (!vertexBuffer_ || !indexBuffer_ || indices_.empty())
            return;

        effect.setWorldProperty(Matrix::getIdentityProperty());
        effect.setViewProperty(view);
        effect.setProjectionProperty(projection);
        effect.setLightingEnabledProperty(false);
        effect.setTextureEnabledProperty(true);
        effect.setTextureProperty(&atlas);
        effect.setDiffuseColorProperty(Vector3(1.0f, 1.0f, 1.0f));
        effect.setAlphaProperty(1.0f);

        device.SetVertexBuffer(vertexBuffer_.get());
        device.setIndicesProperty(indexBuffer_.get());

        for (auto& pass : effect.getCurrentTechniqueProperty()->getPassesProperty())
        {
            pass.Apply();

            device.DrawIndexedPrimitives(
                PrimitiveType::TriangleList,
                0,
                0,
                static_cast<int>(vertices_.size()),
                0,
                static_cast<int>(indices_.size() / 3));
        }
    }
}
