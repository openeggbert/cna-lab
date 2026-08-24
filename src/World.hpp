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
            int repeaterWeapons = 0;
            int heavyWeapons = 0;
        };

        struct AttackResult
        {
            bool hit = false;
            int score = 0;

            operator bool() const { return hit; }
        };

        struct EnemyAudioEvents
        {
            int guardAlerts = 0;
            int houndAlerts = 0;
            int houndAttacks = 0;
        };

        struct CompletionStats
        {
            int defeatedEnemies = 0;
            int totalEnemies = 0;
            int collectedGold = 0;
            int totalGold = 0;
            int foundSecrets = 0;
            int totalSecrets = 0;
        };

        explicit World(const LevelDefinition& level);

        [[nodiscard]] int Update(
            float elapsedSeconds,
            const Microsoft::Xna::Framework::Vector3& playerPosition,
            float damageMultiplier = 1.0f);
        void Upload(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

        void Draw(
            Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
            Microsoft::Xna::Framework::Graphics::BasicEffect& effect,
            const Microsoft::Xna::Framework::Matrix& view,
            const Microsoft::Xna::Framework::Matrix& projection,
            Microsoft::Xna::Framework::Graphics::Texture2D& atlas,
            Microsoft::Xna::Framework::Graphics::Texture2D& guardSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& houndSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& rapidTrooperSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& heavyUnitSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& ammoPickupSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& healthPickupSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& goldBarsSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& goldenGobletSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& peaceMedallionSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& bloodDecal,
            Microsoft::Xna::Framework::Graphics::Texture2D& paintingTexture,
            Microsoft::Xna::Framework::Graphics::Texture2D& peaceBannerTexture,
            Microsoft::Xna::Framework::Graphics::Texture2D& ceilingLampTexture,
            Microsoft::Xna::Framework::Graphics::Texture2D& lampLightTexture,
            const Microsoft::Xna::Framework::Vector3& cameraPosition);

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
        [[nodiscard]] CompletionStats GetCompletionStats() const;
        [[nodiscard]] int ConsumeGuardShotCount();
        [[nodiscard]] EnemyAudioEvents ConsumeEnemyAudioEvents();
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
            enum class Type { Guard, Hound, RapidTrooper, HeavyUnit };

            Microsoft::Xna::Framework::Vector3 position;
            Type type = Type::Guard;
            EnemyState state = EnemyState::Idle;
            int health = 3;
            int scoreValue = 100;
            int attackDamage = 8;
            float moveSpeed = 0.8f;
            float attackRange = 6.0f;
            float attackInterval = 1.35f;
            float projectileSpeed = 4.5f;
            bool melee = false;
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
            int damage = 8;
        };

        enum class PickupType
        {
            Health,
            Ammo,
            GoldBars,
            GoldenGoblet,
            PeaceMedallion,
            AccessCard,
            RepeaterWeapon,
            HeavyWeapon
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

        struct Decoration
        {
            enum class Type { Painting, PeaceBanner, CeilingLamp };

            Microsoft::Xna::Framework::Vector3 position;
            Type type = Type::Painting;
            float rotationY = 0.0f;
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
        int defeatedEnemies_ = 0;
        std::vector<EnemyProjectile> enemyProjectiles_;
        int pendingGuardShotCount_ = 0;
        EnemyAudioEvents pendingEnemyAudioEvents_;
        std::vector<Pickup> pickups_;
        int collectedGold_ = 0;
        int totalGold_ = 0;
        int foundSecrets_ = 0;
        int totalSecrets_ = 0;
        std::vector<Terminal> terminals_;
        std::vector<Microsoft::Xna::Framework::Vector3> exits_;
        std::vector<Decoration> decorations_;
        std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionTexture> enemyVertices_;
        std::vector<std::uint16_t> enemyIndices_;
        std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionTexture> billboardVertices_;
        std::vector<std::uint16_t> billboardIndices_;
        std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionTexture> bloodPoolVertices_;
        std::vector<std::uint16_t> bloodPoolIndices_;

        std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> vertexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> indexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> doorVertexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> doorIndexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> impactVertexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> impactIndexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> enemyVertexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> enemyIndexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> billboardVertexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> billboardIndexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> bloodPoolVertexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> bloodPoolIndexBuffer_;

        [[nodiscard]] bool IsStaticWallCell(int x, int z) const;
        [[nodiscard]] bool IsBlockedCell(int x, int z) const;
        [[nodiscard]] bool HasDeadEnemyInDoorway(const Door& door) const;
        [[nodiscard]] bool HasPlayerInDoorway(
            const Door& door,
            const Microsoft::Xna::Framework::Vector3& playerPosition) const;
        void BuildMesh();
        void BuildDoors();
        void RebuildDoorGeometry();
        void BuildImpactGeometry();
        void BuildEnemies();
        void BuildPickups();
        void BuildTerminals();
        void BuildExits();
        void BuildDecorations();
        void BuildEnemyGeometry();
        void BuildBillboardGeometry();
        void BuildBloodPoolGeometry();
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
