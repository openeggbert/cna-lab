#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionTexture.hpp"

#include "LevelDefinition.hpp"

namespace WolfCna
{
    class World final
    {
    public:
        explicit World(const LevelDefinition& level);

        void Update(float elapsedSeconds);
        void Upload(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

        void Draw(
            Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
            Microsoft::Xna::Framework::Graphics::BasicEffect& effect,
            const Microsoft::Xna::Framework::Matrix& view,
            const Microsoft::Xna::Framework::Matrix& projection,
            Microsoft::Xna::Framework::Graphics::Texture2D& atlas);

        [[nodiscard]] Microsoft::Xna::Framework::Vector3 PlayerStart() const;
        [[nodiscard]] bool Collides(float worldX, float worldZ, float radius) const;
        [[nodiscard]] bool FireHitscan(
            const Microsoft::Xna::Framework::Vector3& playerPosition,
            const Microsoft::Xna::Framework::Vector3& lookDirection);
        void TryActivate(
            const Microsoft::Xna::Framework::Vector3& playerPosition,
            const Microsoft::Xna::Framework::Vector3& lookDirection);

    private:
        enum class Material : int
        {
            Wall = 0,
            Floor = 1,
            Ceiling = 2
        };

        struct Door
        {
            int x = 0;
            int z = 0;
            bool blocksAlongX = true;
            bool opening = false;
            float openAmount = 0.0f;
        };

        struct Impact
        {
            Microsoft::Xna::Framework::Vector3 position;
            Microsoft::Xna::Framework::Vector3 normal;
        };

        static constexpr std::size_t MaxImpactCount = 24;

        std::vector<std::string> map_;
        Microsoft::Xna::Framework::Vector3 playerStart_;

        std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionTexture> vertices_;
        std::vector<std::uint16_t> indices_;
        std::vector<Door> doors_;
        std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionTexture> doorVertices_;
        std::vector<std::uint16_t> doorIndices_;
        std::vector<Impact> impacts_;
        std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionTexture> impactVertices_;
        std::vector<std::uint16_t> impactIndices_;

        std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> vertexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> indexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> doorVertexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> doorIndexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> impactVertexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> impactIndexBuffer_;

        [[nodiscard]] bool IsStaticWallCell(int x, int z) const;
        [[nodiscard]] bool IsBlockedCell(int x, int z) const;
        void BuildMesh();
        void BuildDoors();
        void RebuildDoorGeometry();
        void BuildImpactGeometry();

        void AddQuad(
            const Microsoft::Xna::Framework::Vector3& a,
            const Microsoft::Xna::Framework::Vector3& b,
            const Microsoft::Xna::Framework::Vector3& c,
            const Microsoft::Xna::Framework::Vector3& d,
            Material material);
        void AddDoorQuad(
            const Microsoft::Xna::Framework::Vector3& a,
            const Microsoft::Xna::Framework::Vector3& b,
            const Microsoft::Xna::Framework::Vector3& c,
            const Microsoft::Xna::Framework::Vector3& d);
    };
}
