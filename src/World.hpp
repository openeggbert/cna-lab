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
        static constexpr int MaterialPanelCount = 10;
        static constexpr int CyanAccess = 1;
        static constexpr int AmberAccess = 2;
        static constexpr int AllAccess = CyanAccess | AmberAccess;

        enum class InteractionResult
        {
            None,
            DoorOpened,
            DoorClosing,
            DoorCloseBlocked,
            DoorLocked,
            AmberDoorLocked,
            TerminalActivated,
            RelayActivated,
            SecretRevealed,
            SecretBlocked,
            ExitActivated,
            SecretExitActivated,
            ExitSealed
        };

        enum class ExitRoute
        {
            Standard,
            Secret
        };

        struct PickupResult
        {
            int health = 0;
            int ammo = 0;
            int gold = 0;
            int accessMask = 0;
            int repeaterWeapons = 0;
            int heavyWeapons = 0;
            int extraLives = 0;
            std::vector<Microsoft::Xna::Framework::Vector3> ammunitionAudioPositions;
            std::vector<Microsoft::Xna::Framework::Vector3> pickupAudioPositions;
        };

        struct AttackResult
        {
            bool hit = false;
            int score = 0;
            int damage = 0;
            float distance = 0.0f;
            Microsoft::Xna::Framework::Vector3 position;
            bool defeatedHound = false;

            operator bool() const { return hit; }
        };

        enum class RangedEnemyAudioKind
        {
            Guard,
            RapidTrooper,
            HeavyUnit,
            Boss,
            Count
        };

        struct RangedEnemyAudioEvent
        {
            Microsoft::Xna::Framework::Vector3 position;
            RangedEnemyAudioKind kind = RangedEnemyAudioKind::Guard;
        };

        struct EnemyAudioEvents
        {
            int rangedAlerts = 0;
            int houndAlerts = 0;
            int houndBarks = 0;
            int houndAttacks = 0;
            int projectileImpacts = 0;
            int doorsOpened = 0;
            std::vector<RangedEnemyAudioEvent> rangedAlertSources;
            std::vector<Microsoft::Xna::Framework::Vector3> houndAlertPositions;
            std::vector<Microsoft::Xna::Framework::Vector3> houndBarkPositions;
            std::vector<Microsoft::Xna::Framework::Vector3> houndAttackPositions;
            std::vector<Microsoft::Xna::Framework::Vector3> projectileImpactPositions;
            std::vector<Microsoft::Xna::Framework::Vector3> doorPositions;
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

        struct EnemyBehaviorStats
        {
            int idleEnemies = 0;
            int patrollingEnemies = 0;
            int alertingEnemies = 0;
            int chasingEnemies = 0;
            int attackingEnemies = 0;
            int searchingEnemies = 0;
            int deadEnemies = 0;
            int ambushEnemies = 0;
            float totalTravelDistance = 0.0f;
        };

        struct BossStatus
        {
            bool present = false;
            bool defeated = false;
            int health = 0;
            int maximumHealth = 0;
        };

        struct DoorSaveState
        {
            bool opening = false;
            float openAmount = 0.0f;
            float closeDelay = 0.0f;
            int pushDirectionX = 0;
            int pushDirectionZ = 0;
            int pushDistanceCells = 0;
        };

        struct EnemySaveState
        {
            int state = 0;
            int health = 0;
            float positionX = 0.0f;
            float positionZ = 0.0f;
            float facingX = 0.0f;
            float facingZ = -1.0f;
            float lastKnownX = 0.0f;
            float lastKnownZ = 0.0f;
            float attackCooldown = 0.0f;
            float attackVisualSeconds = 0.0f;
            float painVisualSeconds = 0.0f;
            float visualAnimationSeconds = 0.0f;
            float reactionRemaining = 0.0f;
            float searchRemaining = 0.0f;
            float distanceTravelled = 0.0f;
        };

        struct PickupSaveState
        {
            float positionX = 0.0f;
            float positionZ = 0.0f;
            int type = 0;
            bool collected = false;
            int amount = 0;
        };

        struct ProjectileSaveState
        {
            float positionX = 0.0f;
            float positionY = 0.0f;
            float positionZ = 0.0f;
            float velocityX = 0.0f;
            float velocityY = 0.0f;
            float velocityZ = 0.0f;
            float remainingLifetime = 0.0f;
            int damage = 0;
        };

        struct SaveState
        {
            int defeatedEnemies = 0;
            int collectedGold = 0;
            int foundSecrets = 0;
            std::vector<DoorSaveState> doors;
            std::vector<EnemySaveState> enemies;
            std::vector<PickupSaveState> pickups;
            std::vector<bool> terminalsActivated;
            std::vector<bool> relaysActivated;
            std::vector<float> exitOpenAmounts;
            std::vector<ProjectileSaveState> projectiles;
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
            Microsoft::Xna::Framework::Graphics::Texture2D& bossSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& guardAttackSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& houndAttackSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& rapidTrooperAttackSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& heavyUnitAttackSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& bossAttackSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& guardPainSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& houndPainSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& rapidTrooperPainSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& heavyUnitPainSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& bossPainSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& defeatedGuardSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& defeatedHoundSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& defeatedRapidTrooperSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& defeatedHeavyUnitSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& defeatedBossSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& ammoPickupSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& healthPickupSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& fieldDressingSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& goldBarsSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& goldenGobletSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& peaceMedallionSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& peacePrismSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& accessCardSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& amberAccessCardSprite,
            Microsoft::Xna::Framework::Graphics::Texture2D& recoveryBeaconSprite,
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
            float range = 12.0f,
            bool emitsNoise = true,
            int nearDamage = 1,
            int farDamage = 1,
            float falloffStart = 12.0f);
        [[nodiscard]] PickupResult CollectPickups(
            const Microsoft::Xna::Framework::Vector3& playerPosition,
            int currentHealth = 0,
            int currentAmmunition = 0,
            int carriedWeaponTier = 1,
            int currentAccessMask = 0,
            bool hasRepeater = false,
            bool hasHeavyWeapon = false);
        [[nodiscard]] bool ReachedExit(
            const Microsoft::Xna::Framework::Vector3& playerPosition) const;
        [[nodiscard]] std::optional<ExitRoute> ReachedExitRoute(
            const Microsoft::Xna::Framework::Vector3& playerPosition) const;
        [[nodiscard]] bool AreObjectivesComplete() const;
        [[nodiscard]] std::optional<ExitApproach> GetExitApproach() const;
        [[nodiscard]] ObjectiveStatus GetObjectiveStatus() const;
        [[nodiscard]] CompletionStats GetCompletionStats() const;
        [[nodiscard]] DifficultyBalance GetDifficultyBalance() const;
        [[nodiscard]] EnemyBehaviorStats GetEnemyBehaviorStats() const;
        [[nodiscard]] BossStatus GetBossStatus() const;
        [[nodiscard]] SaveState CaptureSaveState() const;
        [[nodiscard]] bool RestoreSaveState(const SaveState& state);
        [[nodiscard]] int ConsumeRangedShotCount();
        [[nodiscard]] std::vector<RangedEnemyAudioEvent>
            ConsumeRangedShotAudioEvents();
        [[nodiscard]] EnemyAudioEvents ConsumeEnemyAudioEvents();
        [[nodiscard]] std::optional<Microsoft::Xna::Framework::Vector3>
            GetLastInteractionPosition() const;
        [[nodiscard]] int ActiveEnemyImpactCount() const;
        [[nodiscard]] bool IsPushWallAtCell(int x, int z) const;
        [[nodiscard]] bool IsActivatedPushWallSource(int x, int z) const;
        [[nodiscard]] InteractionResult TryActivate(
            const Microsoft::Xna::Framework::Vector3& playerPosition,
            const Microsoft::Xna::Framework::Vector3& lookDirection,
            int accessMask);

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
            Wood = 8,
            AmberSecurityDoor = 9
        };

        struct Door
        {
            int x = 0;
            int z = 0;
            bool blocksAlongX = true;
            int slideDirection = 1;
            Material material = Material::Door;
            bool isSecret = false;
            bool opening = false;
            float openAmount = 0.0f;
            float closeDelay = 0.0f;
            int requiredAccess = 0;
            int pushDirectionX = 0;
            int pushDirectionZ = 0;
            int pushDistanceCells = 0;
        };

        struct Impact
        {
            Microsoft::Xna::Framework::Vector3 position;
            Microsoft::Xna::Framework::Vector3 normal;
        };

        enum class EnemyState
        {
            Idle,
            Patrol,
            Alert,
            Chase,
            Attack,
            Search,
            Dead
        };

        struct Enemy
        {
            enum class Type { Guard, Hound, RapidTrooper, HeavyUnit, Boss };

            Microsoft::Xna::Framework::Vector3 position;
            Microsoft::Xna::Framework::Vector3 facing{0.0f, 0.0f, -1.0f};
            Microsoft::Xna::Framework::Vector3 lastKnownTarget;
            Type type = Type::Guard;
            EnemyState state = EnemyState::Idle;
            int health = 5;
            int scoreValue = 100;
            int attackDamage = 8;
            float moveSpeed = 0.8f;
            float attackRange = 6.0f;
            float attackInterval = 1.35f;
            float projectileSpeed = 4.5f;
            bool melee = false;
            bool ambush = false;
            bool hasPatrolRoute = false;
            int ammunitionDrop = 3;
            int projectileBurst = 1;
            int patrolDirectionX = 0;
            int patrolDirectionZ = 0;
            float reactionDuration = 0.35f;
            float reactionRemaining = 0.0f;
            float searchRemaining = 0.0f;
            float viewDotThreshold = 0.35f;
            float hearingRange = 12.0f;
            float distanceTravelled = 0.0f;
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

        enum class PickupType : int
        {
            HealthLarge = 0,
            Ammo = 1,
            GoldBars = 2,
            GoldenGoblet = 3,
            PeaceMedallion = 4,
            CyanAccessCard = 5,
            RepeaterWeapon = 6,
            HeavyWeapon = 7,
            HealthSmall = 8,
            PeacePrism = 9,
            AmberAccessCard = 10,
            RecoveryBeacon = 11
        };

        struct Pickup
        {
            Microsoft::Xna::Framework::Vector3 position;
            PickupType type = PickupType::HealthLarge;
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
            int slideDirection = 1;
            ExitRoute route = ExitRoute::Standard;
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
        int pendingRangedShotCount_ = 0;
        std::vector<RangedEnemyAudioEvent> pendingRangedShotAudioEvents_;
        EnemyAudioEvents pendingEnemyAudioEvents_;
        std::optional<Microsoft::Xna::Framework::Vector3> lastInteractionPosition_;
        std::optional<Microsoft::Xna::Framework::Vector3> pendingPlayerNoise_;
        std::vector<Pickup> pickups_;
        std::size_t basePickupCount_ = 0;
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
        [[nodiscard]] bool IsPushWallDestinationCell(int x, int z) const;
        [[nodiscard]] bool PushWallOccupiesCell(const Door& door, int x, int z) const;
        [[nodiscard]] bool PushWallIntersectsCircle(
            const Door& door,
            float openAmount,
            float worldX,
            float worldZ,
            float radius) const;
        [[nodiscard]] int PushWallDistanceForDirection(
            const Door& door,
            int directionX,
            int directionZ) const;
        [[nodiscard]] bool PushWallPathOccupied(
            const Door& door,
            int directionX,
            int directionZ,
            int distanceCells,
            const Microsoft::Xna::Framework::Vector3& playerPosition) const;
        [[nodiscard]] bool IsBlockedCell(int x, int z) const;
        [[nodiscard]] bool IsNavigationBlockedCell(
            int x,
            int z,
            bool allowOrdinaryDoors) const;
        [[nodiscard]] bool IsExitCell(int x, int z) const;
        [[nodiscard]] Material WallMaterialForCell(int x, int z) const;
        [[nodiscard]] bool HasEnemyInDoorway(const Door& door) const;
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
        [[nodiscard]] bool HasDirectionalSight(
            const Enemy& enemy,
            const Microsoft::Xna::Framework::Vector3& target) const;
        [[nodiscard]] bool CanHearNoise(
            const Enemy& enemy,
            const Microsoft::Xna::Framework::Vector3& noisePosition) const;
        void BeginEnemyAlert(
            Enemy& enemy,
            const Microsoft::Xna::Framework::Vector3& target);
        [[nodiscard]] bool TryOpenOrdinaryDoor(int x, int z);
        [[nodiscard]] std::vector<std::pair<int, int>> FindPath(
            int startX,
            int startZ,
            int goalX,
            int goalZ,
            bool allowOrdinaryDoors = false) const;
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
