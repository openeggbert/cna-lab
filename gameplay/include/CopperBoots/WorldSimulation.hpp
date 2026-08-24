#pragma once

#include "CopperBoots/Camera2D.hpp"
#include "CopperBoots/LevelDefinition.hpp"
#include "CopperBoots/TileMap.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace CopperBoots
{
    struct PlayerInput
    {
        float Move = 0.0F;
        bool Run = false;
        bool JumpPressed = false;
        bool JumpHeld = false;
        bool AttackPressed = false;
        int Aim = 0;
        bool InteractHeld = false;
        bool PausePressed = false;
    };

    enum class PlayerMotion
    {
        Standing,
        Walking,
        Running,
        Jumping,
        Falling,
        Dead,
        Transition,
    };

    struct PlayerState
    {
        static constexpr float Width = 12.0F;
        static constexpr float Height = 20.0F;

        float X = 0.0F;
        float Y = 0.0F;
        float VelocityX = 0.0F;
        float VelocityY = 0.0F;
        bool Grounded = false;
        bool FacingRight = true;
        bool Plated = false;
        bool ArcCapacitor = false;
        int PowerTransitionTicks = 0;
        int InvulnerabilityTicks = 0;
        bool Dead = false;
        int DeathTicksRemaining = 0;
        PlayerMotion Motion = PlayerMotion::Falling;
    };

    struct CogState
    {
        static constexpr float Size = 8.0F;

        float X = 0.0F;
        float Y = 0.0F;
        bool Collected = false;
    };

    struct WorldEvents
    {
        int CogsCollected = 0;
        int ScoreAdded = 0;
        int BlocksBumped = 0;
        int BlockContentsReleased = 0;
        int BlocksBroken = 0;
        int EnemiesDefeated = 0;
        int PlayerDamaged = 0;
        int PlayerDied = 0;
        int PlayerRespawned = 0;
        int PowerUpsReleased = 0;
        int PowerUpsCollected = 0;
        int CapacitorsReleased = 0;
        int CapacitorsCollected = 0;
        int ProjectilesFired = 0;
        int CheckpointsActivated = 0;
        int LevelCompleted = 0;
    };

    struct CheckpointState
    {
        int TileX = 0;
        int TileY = 0;
        bool Activated = false;
    };

    struct LevelResult
    {
        bool Completed = false;
        int Score = 0;
        int CollectedCogs = 0;
        std::uint64_t CompletionTick = 0;
    };

    struct PlatingPickupState
    {
        static constexpr float Width = 12.0F;
        static constexpr float Height = 12.0F;

        float X = 0.0F;
        float Y = 0.0F;
        float VelocityY = 0.0F;
        int Direction = 1;
        int EmergenceTicks = 0;
        bool Collected = false;
    };

    struct CapacitorPickupState
    {
        static constexpr float Size = 10.0F;

        float X = 0.0F;
        float Y = 0.0F;
        int EmergenceTicks = 0;
        bool Collected = false;
    };

    struct ProjectileState
    {
        static constexpr float Size = 4.0F;

        float X = 0.0F;
        float Y = 0.0F;
        float VelocityX = 0.0F;
        float VelocityY = 0.0F;
        bool Active = false;
    };

    struct MotionBounds
    {
        float PreviousX = 0.0F;
        float PreviousY = 0.0F;
        float CurrentX = 0.0F;
        float CurrentY = 0.0F;
        float Width = 0.0F;
        float Height = 0.0F;
    };

    enum class EnemyContactKind
    {
        None,
        Harmful,
        Stomp,
    };

    [[nodiscard]] EnemyContactKind ClassifyEnemyContact(
        const MotionBounds& player, const MotionBounds& enemy) noexcept;
    [[nodiscard]] EnemyContactKind HighestPriorityEnemyContact(
        std::span<const EnemyContactKind> contacts) noexcept;

    enum class CrawlerEdgePolicy
    {
        Turn,
        Fall,
    };

    struct CrawlerState
    {
        static constexpr float Width = 14.0F;
        static constexpr float Height = 12.0F;

        float X = 0.0F;
        float Y = 0.0F;
        float PreviousX = 0.0F;
        float PreviousY = 0.0F;
        float VelocityY = 0.0F;
        int Direction = -1;
        CrawlerEdgePolicy EdgePolicy = CrawlerEdgePolicy::Turn;
        bool Active = false;
        bool Defeated = false;
    };

    struct InteractiveBlockState
    {
        int TileX = 0;
        int TileY = 0;
        BlockContent Content = BlockContent::None;
        bool Used = false;
        int BumpTicksRemaining = 0;
    };

    class WorldSimulation
    {
    public:
        WorldSimulation();

        void LoadLevel(LevelDefinition level);
        void Update(const PlayerInput& input, float seconds);
        void ResetPlayer();
        void SetPlayerPlated(bool plated) noexcept { player_.Plated = plated; }
        void SetPlayerArcCapacitor(bool enabled) noexcept
        {
            player_.ArcCapacitor = enabled;
        }

        [[nodiscard]] const TileMap& Level() const noexcept { return level_; }
        [[nodiscard]] const PlayerState& Player() const noexcept { return player_; }
        [[nodiscard]] const Camera2D& Camera() const noexcept { return camera_; }
        [[nodiscard]] const std::array<float, 3>& ParallaxFactors() const noexcept
        {
            return parallaxFactors_;
        }
        [[nodiscard]] const std::vector<CogState>& Cogs() const noexcept
        {
            return cogs_;
        }
        [[nodiscard]] const std::vector<CrawlerState>& Crawlers() const noexcept
        {
            return crawlers_;
        }
        [[nodiscard]] const std::vector<PlatingPickupState>& PlatingPickups()
            const noexcept
        {
            return platingPickups_;
        }
        [[nodiscard]] const std::vector<CapacitorPickupState>& CapacitorPickups()
            const noexcept
        {
            return capacitorPickups_;
        }
        [[nodiscard]] const std::array<ProjectileState, 2>& Projectiles()
            const noexcept
        {
            return projectiles_;
        }
        [[nodiscard]] const std::vector<CheckpointState>& Checkpoints() const noexcept
        {
            return checkpoints_;
        }
        [[nodiscard]] const WorldEvents& LastEvents() const noexcept
        {
            return lastEvents_;
        }
        [[nodiscard]] int CollectedCogCount() const noexcept { return collectedCogs_; }
        [[nodiscard]] int Score() const noexcept { return score_; }
        [[nodiscard]] int Lives() const noexcept { return lives_; }
        [[nodiscard]] const std::string& LevelName() const noexcept
        {
            return levelName_;
        }
        [[nodiscard]] std::uint64_t TickCount() const noexcept { return tickCount_; }
        [[nodiscard]] const LevelResult& Result() const noexcept { return result_; }
        [[nodiscard]] int CompletionTicks() const noexcept { return completionTicks_; }
        [[nodiscard]] int BlockVisualOffset(int tileX, int tileY) const noexcept;

    private:
        [[nodiscard]] bool Collides(float x, float y, float width,
                                    float height) const noexcept;
        void MoveHorizontal(float amount);
        void MoveVertical(float amount);
        void UpdateMotion(const PlayerInput& input) noexcept;
        void CollectOverlappingCogs() noexcept;
        void UpdateBlockAnimations() noexcept;
        void HitBlock(int tileX, int tileY);
        void StartBlockBump(int tileX, int tileY);
        void UpdateCrawlers(float seconds);
        void UpdatePlatingPickups(float seconds);
        void CollectOverlappingPlatingPickups() noexcept;
        void UpdateCapacitorPickups() noexcept;
        void CollectOverlappingCapacitorPickups() noexcept;
        void TryFireProjectile(const PlayerInput& input) noexcept;
        void UpdateProjectiles(float seconds);
        void ActivateOverlappingCheckpoints() noexcept;
        void StartLevelCompletion() noexcept;
        void ResolvePlayerCrawlerContacts(float previousPlayerX,
                                           float previousPlayerY);
        void StartPlayerDeath() noexcept;
        void RespawnAtCheckpoint();
        [[nodiscard]] bool TouchesCollision(TileCollision collision) const noexcept;
        [[nodiscard]] bool SolidAabb(float x, float y, float width,
                                     float height) const noexcept;

        TileMap level_;
        std::string levelName_ = "Test Room";
        PlayerState player_;
        Camera2D camera_;
        float spawnX_;
        float spawnY_;
        float checkpointX_;
        float checkpointY_;
        std::array<float, 3> parallaxFactors_{0.10F, 0.25F, 0.50F};
        std::vector<CogState> cogs_;
        std::vector<CrawlerState> crawlers_;
        std::vector<EnemyContactKind> crawlerContacts_;
        std::vector<PlatingPickupState> platingPickups_;
        std::vector<CapacitorPickupState> capacitorPickups_;
        std::array<ProjectileState, 2> projectiles_{};
        std::vector<CheckpointState> checkpoints_;
        std::vector<InteractiveBlockState> interactiveBlocks_;
        WorldEvents lastEvents_;
        int collectedCogs_ = 0;
        int score_ = 0;
        int lives_ = 3;
        std::uint64_t tickCount_ = 0;
        LevelResult result_;
        int completionTicks_ = 0;
    };
}
