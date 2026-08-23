#include "World.hpp"

#include <array>
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
        constexpr float DoorThickness = 0.12f;
        constexpr float DoorOpenSpeed = 1.8f;
        constexpr float DoorPassableAt = 0.78f;
        constexpr float ActivationRange = 1.5f;
        constexpr float ActivationDotThreshold = 0.5f;
    }

    World::World(const LevelDefinition& level)
        : map_(level.Rows())
        , playerStart_(
            static_cast<float>(level.PlayerStartX()) + 0.5f,
            0.62f,
            static_cast<float>(level.PlayerStartZ()) + 0.5f)
    {
        BuildDoors();
        BuildMesh();
        RebuildDoorGeometry();
    }

    Vector3 World::PlayerStart() const
    {
        return playerStart_;
    }

    bool World::IsStaticWallCell(int x, int z) const
    {
        if (z < 0 || z >= static_cast<int>(map_.size()))
            return true;
        if (x < 0 || x >= static_cast<int>(map_[z].size()))
            return true;
        return map_[z][x] == '#';
    }

    bool World::IsBlockedCell(int x, int z) const
    {
        if (IsStaticWallCell(x, z))
            return true;

        for (const Door& door : doors_)
        {
            if (door.x == x && door.z == z)
                return door.openAmount < DoorPassableAt;
        }

        return false;
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
            if (IsBlockedCell(cellX, cellZ))
                return true;
        }

        return false;
    }

    void World::TryActivate(const Vector3& playerPosition, const Vector3& lookDirection)
    {
        Door* target = nullptr;
        float closestDistanceSquared = ActivationRange * ActivationRange;

        for (Door& door : doors_)
        {
            if (door.opening || door.openAmount >= 1.0f)
                continue;

            const float offsetX = static_cast<float>(door.x) + 0.5f - playerPosition.X;
            const float offsetZ = static_cast<float>(door.z) + 0.5f - playerPosition.Z;
            const float distanceSquared = offsetX * offsetX + offsetZ * offsetZ;
            if (distanceSquared > closestDistanceSquared || distanceSquared <= 0.0f)
                continue;

            const float inverseDistance = 1.0f / std::sqrt(distanceSquared);
            const float facing = (offsetX * lookDirection.X + offsetZ * lookDirection.Z) * inverseDistance;
            if (facing < ActivationDotThreshold)
                continue;

            target = &door;
            closestDistanceSquared = distanceSquared;
        }

        if (target)
            target->opening = true;
    }

    void World::Update(float elapsedSeconds)
    {
        bool changed = false;

        for (Door& door : doors_)
        {
            if (!door.opening || door.openAmount >= 1.0f)
                continue;

            const float previousAmount = door.openAmount;
            door.openAmount = std::min(1.0f, door.openAmount + DoorOpenSpeed * elapsedSeconds);
            changed = changed || door.openAmount != previousAmount;
        }

        if (!changed)
            return;

        RebuildDoorGeometry();
        if (doorVertexBuffer_)
            doorVertexBuffer_->SetData(doorVertices_.data(), static_cast<int>(doorVertices_.size()));
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

    void World::AddDoorQuad(
        const Vector3& a,
        const Vector3& b,
        const Vector3& c,
        const Vector3& d)
    {
        if (doorVertices_.size() > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max() - 4))
            throw std::runtime_error("Door geometry exceeded the 16-bit vertex index limit.");

        const auto base = static_cast<std::uint16_t>(doorVertices_.size());
        constexpr float wallU0 = 0.0f;
        constexpr float wallU1 = 1.0f / 3.0f;

        doorVertices_.emplace_back(a, Vector2(wallU0, 1.0f));
        doorVertices_.emplace_back(b, Vector2(wallU1, 1.0f));
        doorVertices_.emplace_back(c, Vector2(wallU1, 0.0f));
        doorVertices_.emplace_back(d, Vector2(wallU0, 0.0f));

        doorIndices_.push_back(base + 0);
        doorIndices_.push_back(base + 1);
        doorIndices_.push_back(base + 2);
        doorIndices_.push_back(base + 0);
        doorIndices_.push_back(base + 2);
        doorIndices_.push_back(base + 3);
    }

    void World::BuildDoors()
    {
        for (int z = 0; z < static_cast<int>(map_.size()); ++z)
        {
            for (int x = 0; x < static_cast<int>(map_[z].size()); ++x)
            {
                if (map_[z][x] != 'D')
                    continue;

                doors_.push_back({
                    x,
                    z,
                    IsStaticWallCell(x, z - 1) && IsStaticWallCell(x, z + 1)});
            }
        }
    }

    void World::RebuildDoorGeometry()
    {
        doorVertices_.clear();
        doorIndices_.clear();

        for (const Door& door : doors_)
        {
            const float centerX = static_cast<float>(door.x) + 0.5f;
            const float centerZ = static_cast<float>(door.z) + 0.5f;
            const float halfThickness = DoorThickness * 0.5f;
            const float minimumX = door.blocksAlongX ? centerX - halfThickness : static_cast<float>(door.x);
            const float maximumX = door.blocksAlongX ? centerX + halfThickness : static_cast<float>(door.x) + 1.0f;
            const float minimumZ = door.blocksAlongX ? static_cast<float>(door.z) : centerZ - halfThickness;
            const float maximumZ = door.blocksAlongX ? static_cast<float>(door.z) + 1.0f : centerZ + halfThickness;
            const float minimumY = door.openAmount * WallHeight;
            const float maximumY = minimumY + WallHeight;

            const Vector3 lowerNorthWest(minimumX, minimumY, minimumZ);
            const Vector3 lowerNorthEast(maximumX, minimumY, minimumZ);
            const Vector3 lowerSouthWest(minimumX, minimumY, maximumZ);
            const Vector3 lowerSouthEast(maximumX, minimumY, maximumZ);
            const Vector3 upperNorthWest(minimumX, maximumY, minimumZ);
            const Vector3 upperNorthEast(maximumX, maximumY, minimumZ);
            const Vector3 upperSouthWest(minimumX, maximumY, maximumZ);
            const Vector3 upperSouthEast(maximumX, maximumY, maximumZ);

            AddDoorQuad(lowerNorthEast, lowerNorthWest, upperNorthWest, upperNorthEast);
            AddDoorQuad(lowerSouthWest, lowerSouthEast, upperSouthEast, upperSouthWest);
            AddDoorQuad(lowerNorthWest, lowerSouthWest, upperSouthWest, upperNorthWest);
            AddDoorQuad(lowerSouthEast, lowerNorthEast, upperNorthEast, upperSouthEast);
            AddDoorQuad(upperNorthWest, upperNorthEast, upperSouthEast, upperSouthWest);
            AddDoorQuad(lowerNorthWest, lowerSouthWest, lowerSouthEast, lowerNorthEast);
        }
    }

    void World::BuildMesh()
    {
        for (int z = 0; z < static_cast<int>(map_.size()); ++z)
        {
            for (int x = 0; x < static_cast<int>(map_[z].size()); ++x)
            {
                if (!IsStaticWallCell(x, z))
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
                if (!IsStaticWallCell(x, z - 1))
                {
                    AddQuad(
                        Vector3(x1, 0.0f, z0),
                        Vector3(x0, 0.0f, z0),
                        Vector3(x0, WallHeight, z0),
                        Vector3(x1, WallHeight, z0),
                        Material::Wall);
                }

                if (!IsStaticWallCell(x, z + 1))
                {
                    AddQuad(
                        Vector3(x0, 0.0f, z1),
                        Vector3(x1, 0.0f, z1),
                        Vector3(x1, WallHeight, z1),
                        Vector3(x0, WallHeight, z1),
                        Material::Wall);
                }

                if (!IsStaticWallCell(x - 1, z))
                {
                    AddQuad(
                        Vector3(x0, 0.0f, z0),
                        Vector3(x0, 0.0f, z1),
                        Vector3(x0, WallHeight, z1),
                        Vector3(x0, WallHeight, z0),
                        Material::Wall);
                }

                if (!IsStaticWallCell(x + 1, z))
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

        if (doorVertices_.empty())
            return;

        doorVertexBuffer_ = std::make_unique<VertexBuffer>(
            device,
            VertexPositionTexture::getVertexDeclarationStatic(),
            static_cast<int>(doorVertices_.size()),
            BufferUsage::None);
        doorVertexBuffer_->SetData(doorVertices_.data(), static_cast<int>(doorVertices_.size()));

        doorIndexBuffer_ = std::make_unique<IndexBuffer>(
            device,
            IndexElementSize::SixteenBits,
            static_cast<int>(doorIndices_.size()),
            BufferUsage::None);
        doorIndexBuffer_->SetData(doorIndices_.data(), static_cast<int>(doorIndices_.size()));
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

        if (!doorVertexBuffer_ || !doorIndexBuffer_ || doorIndices_.empty())
            return;

        device.SetVertexBuffer(doorVertexBuffer_.get());
        device.setIndicesProperty(doorIndexBuffer_.get());

        for (auto& pass : effect.getCurrentTechniqueProperty()->getPassesProperty())
        {
            pass.Apply();

            device.DrawIndexedPrimitives(
                PrimitiveType::TriangleList,
                0,
                0,
                static_cast<int>(doorVertices_.size()),
                0,
                static_cast<int>(doorIndices_.size() / 3));
        }
    }
}
