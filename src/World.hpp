#pragma once

#include <cstdint>
#include <memory>
#include <optional>
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
#include "Difficulty.hpp"

namespace WolfCna
{
    class World final
    {
    public:
        static constexpr int MaterialPanelCount = 9;

        enum class InteractionResult
        {
            None,
            DoorOpened,
            DoorLocked,
            TerminalActivated,
            RelayActivated,
            SecretRevealed,
            ExitActivated
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
            int projectileImpacts = 0;
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

        struct ObjectiveStatus
        {
            int activatedRelays = 0;
            int totalRelays = 0;
            int activatedTerminals = 0;
            int totalTerminals = 0;
        };

        struct ExitApproach
        {
            Microsoft::Xna::Framework::Vector3 position;
            Microsoft::Xna::Framework::Vector3 lookDirection;
        };

        struct DifficultyBalance
        {
            int activeEnemies = 0;
            int totalEnemyHealth = 0;
            int fixedAmmunition = 0;
            int potentialDroppedAmmunition = 0;
        };

        explicit World(
            const LevelDefinition& level,
            Difficulty difficulty = Difficulty::Operative);

        [[nodiscard]] int Update(
            float elapsedSeconds,
            const Microsoft::Xna::Framework::Vector3& playerPosition);
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
            Microsoft::Xna::Framework::Graphics::Texture2D& guardAttackSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& houndAttackSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& rapidTrooperAttackSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& heavyUnitAttackSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& guardPainSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& houndPainSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& rapidTrooperPainSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& heavyUnitPainSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& defeatedGuardSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& defeatedHoundSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& defeatedRapidTrooperSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& defeatedHeavyUnitSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& ammoPickupSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& healthPickupSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& goldBarsSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& goldenGobletSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& peaceMedallionSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& accessCardSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& repeaterPickupSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& heavyWeaponPickupSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& terminalSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& relaySprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& exitSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& enemyProjectileSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& enemyImpactSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& bloodDecal,
            Microsoft::Xna::Framework::Graphics::Texture2D& paintingTexture,
            Microsoft::Xna::Framework::Graphics::Texture2D& peaceBannerTexture,
            Microsoft::Xna::Framework::Graphics::Texture2D& ceilingLampTexture,
            Microsoft::Xna::Framework::Graphics::Texture2D& lampLightTexture,
            Microsoft::Xna::Framework::Graphics::Texture2D& plantSprite,
            const Microsoft::Xna::Framework::Vector3& cameraPosition);

        [[nodiscard]] Microsoft::Xna::Framework::Vector3 PlayerStart() const;
        [[nodiscard]] bool Collides(float worldX, float worldZ, float radius) const;
        [[nodiscard]] AttackResult FireHitscan(
            const Microsoft::Xna::Framework::Vector3& playerPosition,
            const Microsoft::Xna::Framework::Vector3& lookDirection,
            float range = 12.0f);
        [[nodiscard]] PickupResult CollectPickups(
            const Microsoft::Xna::Framework::Vector3& playerPosition,
            int currentHealth = 0);
        [[nodiscard]] bool ReachedExit(
            const Microsoft::Xna::Framework::Vector3& playerPosition) const;
        [[nodiscard]] bool AreObjectivesComplete() const;
        [[nodiscard]] std::optional<ExitApproach> GetExitApproach() const;
        [[nodiscard]] ObjectiveStatus GetObjectiveStatus() const;
        [[nodiscard]] CompletionStats GetCompletionStats() const;
        [[nodiscard]] DifficultyBalance GetDifficultyBalance() const;
        [[nodiscard]] int ConsumeGuardShotCount();
        [[nodiscard]] EnemyAudioEvents ConsumeEnemyAudioEvents();
        [[nodiscard]] int ActiveEnemyImpactCount() const;
        [[nodiscard]] InteractionResult TryActivate(
            const Microsoft::Xna::Framework::Vector3& playerPosition,
            const Microsoft::Xna::Framework::Vector3& lookDirection,
            bool hasSecurityCard);

    private:
        enum class Material : int
        {
            WallStone = 0,
            WallBrick = 1,
            WallSteel = 2,
            WallLab = 3,
            Floor = 4,
            Ceiling = 5,
            Door = 6,
            SecurityDoor = 7,
            Wood = 8
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
            int ammunitionDrop = 3;
            std::vector<std::pair<int, int>> path;
            std::size_t pathIndex = 0;
            float pathRefreshTime = 0.0f;
            float attackCooldown = 0.0f;
            float attackVisualSeconds = 0.0f;
            float painVisualSeconds = 0.0f;
            float visualAnimationSeconds = 0.0f;
        };

        struct EnemyProjectile
        {
            Microsoft::Xna::Framework::Vector3 position;
            Microsoft::Xna::Framework::Vector3 velocity;
            float remainingLifetime = 0.0f;
            int damage = 8;
        };

        struct EnemyImpact
        {
            Microsoft::Xna::Framework::Vector3 position;
            float remainingSeconds = 0.0f;
            bool hitPlayer = false;
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
            int amount = 6;
        };

        struct Terminal
        {
            Microsoft::Xna::Framework::Vector3 position;
            bool activated = false;
        };

        struct Relay
        {
            Microsoft::Xna::Framework::Vector3 position;
            bool activated = false;
        };

        struct Exit
        {
            Microsoft::Xna::Framework::Vector3 position;
            int x = 0;
            int z = 0;
            int approachX = 0;
            int approachZ = 0;
            float openAmount = 0.0f;
        };

        struct Decoration
        {
            enum class Type { Painting, PeaceBanner, CeilingLamp, Plant };

            Microsoft::Xna::Framework::Vector3 position;
            Type type = Type::Painting;
            float rotationY = 0.0f;
        };

        static constexpr std::size_t MaxImpactCount = 24;

        std::vector<std::string> map_;
        Microsoft::Xna::Framework::Vector3 playerStart_;
        DifficultyProfile difficultyProfile_;

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
        std::vector<EnemyImpact> enemyImpacts_;
        int pendingGuardShotCount_ = 0;
        EnemyAudioEvents pendingEnemyAudioEvents_;
        std::vector<Pickup> pickups_;
        int collectedGold_ = 0;
        int totalGold_ = 0;
        int foundSecrets_ = 0;
        int totalSecrets_ = 0;
        int totalEnemyHealth_ = 0;
        int fixedAmmunition_ = 0;
        int potentialDroppedAmmunition_ = 0;
        std::vector<Terminal> terminals_;
        std::vector<Relay> relays_;
        std::vector<Exit> exits_;
        std::vector<Decoration> decorations_;
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
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> billboardVertexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> billboardIndexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> bloodPoolVertexBuffer_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> bloodPoolIndexBuffer_;

        [[nodiscard]] bool IsStaticWallCell(int x, int z) const;
        [[nodiscard]] bool IsBlockedCell(int x, int z) const;
        [[nodiscard]] bool IsExitCell(int x, int z) const;
        [[nodiscard]] Material WallMaterialForCell(int x, int z) const;
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
        void BuildRelays();
        void BuildExits();
        void BuildDecorations();
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
        void AddEnemyImpact(
            const Microsoft::Xna::Framework::Vector3& position,
            bool hitPlayer);

        void AddQuad(
            const Microsoft::Xna::Framework::Vector3& a,
            const Microsoft::Xna::Framework::Vector3& b,
            const Microsoft::Xna::Framework::Vector3& c,
            const Microsoft::Xna::Framework::Vector3& d,
            Material material);
        void AddBox(
            const Microsoft::Xna::Framework::Vector3& minimum,
            const Microsoft::Xna::Framework::Vector3& maximum,
            Material material);
        void AddDoorQuad(
            const Microsoft::Xna::Framework::Vector3& a,
            const Microsoft::Xna::Framework::Vector3& b,
            const Microsoft::Xna::Framework::Vector3& c,
            const Microsoft::Xna::Framework::Vector3& d,
            Material material);
        void AddDoorBox(
            const Microsoft::Xna::Framework::Vector3& minimum,
            const Microsoft::Xna::Framework::Vector3& maximum,
            Material material);
    };
}
