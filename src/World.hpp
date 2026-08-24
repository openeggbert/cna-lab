#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
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
        enum class InteractionResult
        {
            None,
            DoorOpened,
            DoorLocked,
            TerminalActivated,
            SecretRevealed
        };

        struct PickupResult
        {
            int health = 0;
            int ammo = 0;
            int gold = 0;
            int accessCards = 0;
        };

        struct AttackResult
        {
            bool hit = false;
            int score = 0;

            operator bool() const { return hit; }
        };

        explicit World(const LevelDefinition& level);

        [[nodiscard]] int Update(
            float elapsedSeconds,
            const Microsoft::Xna::Framework::Vector3& playerPosition);
        void Upload(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

        void Draw(
            Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
            Microsoft::Xna::Framework::Graphics::BasicEffect& effect,
            const Microsoft::Xna::Framework::Matrix& view,
            const Microsoft::Xna::Framework::Matrix& projection,
            Microsoft::Xna::Framework::Graphics::Texture2D& atlas);

        [[nodiscard]] Microsoft::Xna::Framework::Vector3 PlayerStart() const;
        [[nodiscard]] bool Collides(float worldX, float worldZ, float radius) const;
        [[nodiscard]] AttackResult FireHitscan(
            const Microsoft::Xna::Framework::Vector3& playerPosition,
            const Microsoft::Xna::Framework::Vector3& lookDirection,
            float range = 12.0f);
        [[nodiscard]] PickupResult CollectPickups(
            const Microsoft::Xna::Framework::Vector3& playerPosition);
        [[nodiscard]] bool ReachedExit(
            const Microsoft::Xna::Framework::Vector3& playerPosition) const;
        [[nodiscard]] bool IsExitUnlocked() const;
        [[nodiscard]] int ConsumeGuardShotCount();
        [[nodiscard]] InteractionResult TryActivate(
            const Microsoft::Xna::Framework::Vector3& playerPosition,
            const Microsoft::Xna::Framework::Vector3& lookDirection,
            bool hasSecurityCard);

    private:
        enum class Material : int
        {
            Wall = 0,
            Floor = 1,
            Ceiling = 2,
            Door = 3,
            SecurityDoor = 4
        };

        struct Door
        {
            int x = 0;
            int z = 0;
            bool blocksAlongX = true;
            Material material = Material::Door;
            bool isSecret = false;
            bool opening = false;
            float openAmount = 0.0f;
            float closeDelay = 0.0f;
        };

        struct Impact
        {
            Microsoft::Xna::Framework::Vector3 position;
            Microsoft::Xna::Framework::Vector3 normal;
        };

        enum class EnemyState
        {
            Idle,
            Chase,
            Attack,
            Dead
        };

        struct Enemy
        {
            enum class Type { Guard, Hound };

            Microsoft::Xna::Framework::Vector3 position;
            Type type = Type::Guard;
            EnemyState state = EnemyState::Idle;
            int health = 3;
            std::vector<std::pair<int, int>> path;
            std::size_t pathIndex = 0;
            float pathRefreshTime = 0.0f;
            float attackCooldown = 0.0f;
        };

        struct EnemyProjectile
        {
            Microsoft::Xna::Framework::Vector3 position;
            Microsoft::Xna::Framework::Vector3 velocity;
            float remainingLifetime = 0.0f;
        };

        enum class PickupType
        {
            Health,
            Ammo,
            Gold,
            AccessCard
        };

        struct Pickup
        {
            Microsoft::Xna::Framework::Vector3 position;
            PickupType type = PickupType::Health;
            bool collected = false;
        };

        struct Terminal
        {
            Microsoft::Xna::Framework::Vector3 position;
            bool activated = false;
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
        std::vector<Enemy> enemies_;
        std::vector<EnemyProjectile> enemyProjectiles_;
        int pendingGuardShotCount_ = 0;
        std::vector<Pickup> pickups_;
        std::vector<Terminal> terminals_;
        std::vector<Microsoft::Xna::Framework::Vector3> exits_;
        std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionTexture> enemyVertices_;
        std::vector<std::uint16_t> enemyIndices_;

        std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> vertexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> indexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> doorVertexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> doorIndexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> impactVertexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> impactIndexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> enemyVertexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> enemyIndexBuffer_;

        [[nodiscard]] bool IsStaticWallCell(int x, int z) const;
        [[nodiscard]] bool IsBlockedCell(int x, int z) const;
        [[nodiscard]] bool HasDeadEnemyInDoorway(const Door& door) const;
        void BuildMesh();
        void BuildDoors();
        void RebuildDoorGeometry();
        void BuildImpactGeometry();
        void BuildEnemies();
        void BuildPickups();
        void BuildTerminals();
        void BuildExits();
        void BuildEnemyGeometry();
        [[nodiscard]] bool HasLineOfSight(
            const Microsoft::Xna::Framework::Vector3& from,
            const Microsoft::Xna::Framework::Vector3& to) const;
        [[nodiscard]] std::vector<std::pair<int, int>> FindPath(
            int startX,
            int startZ,
            int goalX,
            int goalZ) const;

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
            const Microsoft::Xna::Framework::Vector3& d,
            Material material);
        void AddEnemyQuad(
            const Microsoft::Xna::Framework::Vector3& a,
            const Microsoft::Xna::Framework::Vector3& b,
            const Microsoft::Xna::Framework::Vector3& c,
            const Microsoft::Xna::Framework::Vector3& d);
    };
}
