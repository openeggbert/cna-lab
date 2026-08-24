#include "CopperBoots/WorldSimulation.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace CopperBoots
{
    namespace
    {
        constexpr float WalkSpeed = 72.0F;
        constexpr float RunSpeed = 128.0F;
        constexpr float GroundAcceleration = 720.0F;
        constexpr float AirAcceleration = 430.0F;
        constexpr float GroundBraking = 900.0F;
        constexpr float AirBraking = 90.0F;
        constexpr float Gravity = 1'200.0F;
        constexpr float JumpImpulse = 330.0F;
        constexpr float MaximumFallSpeed = 420.0F;
        constexpr float EarlyReleaseGravityMultiplier = 2.2F;
        constexpr float CollisionEpsilon = 0.001F;

        [[nodiscard]] float Approach(const float value, const float target,
                                     const float maximumDelta)
        {
            if (value < target)
                return std::min(value + maximumDelta, target);
            return std::max(value - maximumDelta, target);
        }
    }

    WorldSimulation::WorldSimulation()
        : level_(TileMap::CreateTestRoom()),
          camera_(320.0F, 180.0F),
          spawnX_(3.0F * static_cast<float>(TileMap::TileSize)),
          spawnY_(9.0F * static_cast<float>(TileMap::TileSize) - PlayerState::Height)
    {
        camera_.SetWorldBounds(static_cast<float>(level_.PixelWidth()),
                               static_cast<float>(level_.PixelHeight()));
        ResetPlayer();
    }

    void WorldSimulation::LoadLevel(LevelDefinition level)
    {
        cogs_.clear();
        cogs_.reserve(level.Cogs.size());
        for (const TileCoordinate& cog : level.Cogs) {
            cogs_.push_back({
                static_cast<float>(cog.X * TileMap::TileSize) +
                    (TileMap::TileSize - CogState::Size) * 0.5F,
                static_cast<float>(cog.Y * TileMap::TileSize) +
                    (TileMap::TileSize - CogState::Size) * 0.5F,
                false});
        }
        crawlers_.clear();
        crawlers_.reserve(level.Crawlers.size());
        for (const CrawlerDefinition& crawler : level.Crawlers) {
            crawlers_.push_back({
                static_cast<float>(crawler.Position.X * TileMap::TileSize) + 1.0F,
                static_cast<float>((crawler.Position.Y + 1) * TileMap::TileSize) -
                    CrawlerState::Height,
                0.0F, -1,
                crawler.FallsAtEdges
                    ? CrawlerEdgePolicy::Fall
                    : CrawlerEdgePolicy::Turn,
                false, false});
        }
        interactiveBlocks_.clear();
        interactiveBlocks_.reserve(level.InteractiveBlocks.size());
        for (const InteractiveBlockDefinition& block : level.InteractiveBlocks) {
            interactiveBlocks_.push_back({block.Position.X, block.Position.Y,
                                          block.Content, false, 0});
        }
        level_ = std::move(level.Map);
        spawnX_ = static_cast<float>(level.SpawnTileX * TileMap::TileSize);
        spawnY_ = static_cast<float>(level.SpawnFootTileY * TileMap::TileSize) -
                  PlayerState::Height;
        parallaxFactors_ = level.ParallaxFactors;
        collectedCogs_ = 0;
        score_ = 0;
        tickCount_ = 0;
        lastEvents_ = {};
        camera_.SetWorldBounds(static_cast<float>(level_.PixelWidth()),
                               static_cast<float>(level_.PixelHeight()));
        ResetPlayer();
    }

    void WorldSimulation::ResetPlayer()
    {
        player_ = {};
        player_.X = spawnX_;
        player_.Y = spawnY_;
        player_.Grounded = Collides(player_.X, player_.Y + 1.0F,
                                    PlayerState::Width, PlayerState::Height);
        player_.FacingRight = true;
        player_.Motion = PlayerMotion::Standing;
        camera_.SnapTo(player_.X + PlayerState::Width * 0.5F,
                       player_.Y + PlayerState::Height * 0.5F);
    }

    void WorldSimulation::Update(const PlayerInput& input, const float seconds)
    {
        lastEvents_ = {};
        UpdateBlockAnimations();
        if (player_.InvulnerabilityTicks > 0)
            --player_.InvulnerabilityTicks;
        const float previousPlayerBottom = player_.Y + PlayerState::Height;
        const float direction = std::clamp(input.Move, -1.0F, 1.0F);
        const float speedLimit = input.Run ? RunSpeed : WalkSpeed;
        const float acceleration = player_.Grounded
            ? GroundAcceleration
            : AirAcceleration;

        if (std::abs(direction) > 0.01F) {
            player_.VelocityX = Approach(player_.VelocityX,
                                         direction * speedLimit,
                                         acceleration * seconds);
            player_.FacingRight = direction > 0.0F;
        }
        else {
            player_.VelocityX = Approach(player_.VelocityX, 0.0F,
                                         (player_.Grounded
                                              ? GroundBraking
                                              : AirBraking) * seconds);
        }

        if (input.JumpPressed && player_.Grounded) {
            player_.VelocityY = -JumpImpulse;
            player_.Grounded = false;
        }

        float gravity = Gravity;
        if (!input.JumpHeld && player_.VelocityY < 0.0F)
            gravity *= EarlyReleaseGravityMultiplier;
        player_.VelocityY = std::min(player_.VelocityY + gravity * seconds,
                                     MaximumFallSpeed);

        MoveHorizontal(player_.VelocityX * seconds);
        player_.Grounded = false;
        MoveVertical(player_.VelocityY * seconds);

        if (player_.Y > static_cast<float>(level_.PixelHeight() + 64))
            ResetPlayer();

        UpdateCrawlers(seconds);
        ResolvePlayerCrawlerContacts(previousPlayerBottom);
        CollectOverlappingCogs();
        UpdateMotion(input);
        camera_.Update(player_.X + PlayerState::Width * 0.5F,
                       player_.Y + PlayerState::Height * 0.5F,
                       player_.VelocityX, seconds);
        ++tickCount_;
    }

    bool WorldSimulation::Collides(const float x, const float y,
                                   const float width, const float height) const noexcept
    {
        const int left = static_cast<int>(std::floor(x / TileMap::TileSize));
        const int right = static_cast<int>(std::floor(
            (x + width - CollisionEpsilon) / TileMap::TileSize));
        const int top = static_cast<int>(std::floor(y / TileMap::TileSize));
        const int bottom = static_cast<int>(std::floor(
            (y + height - CollisionEpsilon) / TileMap::TileSize));

        for (int tileY = top; tileY <= bottom; ++tileY) {
            for (int tileX = left; tileX <= right; ++tileX) {
                if (level_.IsSolid(tileX, tileY))
                    return true;
            }
        }
        return false;
    }

    void WorldSimulation::MoveHorizontal(const float amount)
    {
        if (amount == 0.0F)
            return;

        player_.X += amount;
        if (!Collides(player_.X, player_.Y,
                      PlayerState::Width, PlayerState::Height))
            return;

        if (amount > 0.0F) {
            const int tileX = static_cast<int>(std::floor(
                (player_.X + PlayerState::Width - CollisionEpsilon) /
                TileMap::TileSize));
            player_.X = static_cast<float>(tileX * TileMap::TileSize) -
                        PlayerState::Width;
        }
        else {
            const int tileX = static_cast<int>(std::floor(
                player_.X / TileMap::TileSize));
            player_.X = static_cast<float>((tileX + 1) * TileMap::TileSize);
        }
        player_.VelocityX = 0.0F;
    }

    void WorldSimulation::MoveVertical(const float amount)
    {
        if (amount == 0.0F)
            return;

        player_.Y += amount;
        if (!Collides(player_.X, player_.Y,
                      PlayerState::Width, PlayerState::Height))
            return;

        if (amount < 0.0F) {
            const int tileY = static_cast<int>(std::floor(
                player_.Y / TileMap::TileSize));
            const int left = static_cast<int>(std::floor(
                player_.X / TileMap::TileSize));
            const int right = static_cast<int>(std::floor(
                (player_.X + PlayerState::Width - CollisionEpsilon) /
                TileMap::TileSize));
            for (int tileX = left; tileX <= right; ++tileX) {
                if (level_.IsSolid(tileX, tileY))
                    HitBlock(tileX, tileY);
            }
            if (!Collides(player_.X, player_.Y,
                          PlayerState::Width, PlayerState::Height))
                return;
        }

        if (amount > 0.0F) {
            const int tileY = static_cast<int>(std::floor(
                (player_.Y + PlayerState::Height - CollisionEpsilon) /
                TileMap::TileSize));
            player_.Y = static_cast<float>(tileY * TileMap::TileSize) -
                        PlayerState::Height;
            player_.Grounded = true;
        }
        else {
            const int tileY = static_cast<int>(std::floor(
                player_.Y / TileMap::TileSize));
            player_.Y = static_cast<float>((tileY + 1) * TileMap::TileSize);
        }
        player_.VelocityY = 0.0F;
    }

    void WorldSimulation::UpdateMotion(const PlayerInput& input) noexcept
    {
        if (!player_.Grounded) {
            player_.Motion = player_.VelocityY < 0.0F
                ? PlayerMotion::Jumping
                : PlayerMotion::Falling;
        }
        else if (std::abs(player_.VelocityX) < 0.5F) {
            player_.Motion = PlayerMotion::Standing;
        }
        else {
            player_.Motion = input.Run
                ? PlayerMotion::Running
                : PlayerMotion::Walking;
        }
    }

    void WorldSimulation::CollectOverlappingCogs() noexcept
    {
        const float playerRight = player_.X + PlayerState::Width;
        const float playerBottom = player_.Y + PlayerState::Height;
        for (CogState& cog : cogs_) {
            if (cog.Collected)
                continue;
            const bool overlaps = player_.X < cog.X + CogState::Size &&
                                  playerRight > cog.X &&
                                  player_.Y < cog.Y + CogState::Size &&
                                  playerBottom > cog.Y;
            if (!overlaps)
                continue;

            cog.Collected = true;
            ++collectedCogs_;
            score_ += 100;
            ++lastEvents_.CogsCollected;
            lastEvents_.ScoreAdded += 100;
        }
    }

    void WorldSimulation::UpdateBlockAnimations() noexcept
    {
        for (InteractiveBlockState& block : interactiveBlocks_) {
            if (block.BumpTicksRemaining > 0)
                --block.BumpTicksRemaining;
        }
    }

    void WorldSimulation::HitBlock(const int tileX, const int tileY)
    {
        const Tile tile = level_.Get(tileX, tileY);
        if (tile.Visual == TileVisual::Breakable) {
            if (player_.Plated) {
                level_.Set(tileX, tileY, Tiles::Empty);
                ++lastEvents_.BlocksBroken;
            }
            else {
                ++lastEvents_.BlocksBumped;
            }
            return;
        }

        if (tile.Visual != TileVisual::Interactive &&
            tile.Visual != TileVisual::UsedBlock)
            return;

        ++lastEvents_.BlocksBumped;
        StartBlockBump(tileX, tileY);
        for (InteractiveBlockState& block : interactiveBlocks_) {
            if (block.TileX != tileX || block.TileY != tileY || block.Used)
                continue;

            block.Used = true;
            level_.Set(tileX, tileY, Tiles::UsedBlock);
            if (block.Content == BlockContent::Cog) {
                cogs_.push_back({
                    static_cast<float>(tileX * TileMap::TileSize) +
                        (TileMap::TileSize - CogState::Size) * 0.5F,
                    static_cast<float>((tileY - 1) * TileMap::TileSize) +
                        (TileMap::TileSize - CogState::Size) * 0.5F,
                    false});
                ++lastEvents_.BlockContentsReleased;
            }
            return;
        }
    }

    void WorldSimulation::StartBlockBump(const int tileX, const int tileY)
    {
        for (InteractiveBlockState& block : interactiveBlocks_) {
            if (block.TileX == tileX && block.TileY == tileY) {
                block.BumpTicksRemaining = 8;
                return;
            }
        }
    }

    int WorldSimulation::BlockVisualOffset(const int tileX,
                                           const int tileY) const noexcept
    {
        for (const InteractiveBlockState& block : interactiveBlocks_) {
            if (block.TileX != tileX || block.TileY != tileY)
                continue;
            const int phase = block.BumpTicksRemaining;
            return phase >= 6 ? -2 : (phase >= 3 ? -1 : 0);
        }
        return 0;
    }

    bool WorldSimulation::SolidAabb(const float x, const float y,
                                    const float width,
                                    const float height) const noexcept
    {
        const int left = static_cast<int>(std::floor(x / TileMap::TileSize));
        const int right = static_cast<int>(std::floor(
            (x + width - CollisionEpsilon) / TileMap::TileSize));
        const int top = static_cast<int>(std::floor(y / TileMap::TileSize));
        const int bottom = static_cast<int>(std::floor(
            (y + height - CollisionEpsilon) / TileMap::TileSize));
        for (int tileY = top; tileY <= bottom; ++tileY) {
            for (int tileX = left; tileX <= right; ++tileX) {
                if (level_.IsSolid(tileX, tileY))
                    return true;
            }
        }
        return false;
    }

    void WorldSimulation::UpdateCrawlers(const float seconds)
    {
        constexpr float speed = 24.0F;
        constexpr float gravity = 900.0F;
        constexpr float maximumFallSpeed = 300.0F;
        constexpr float activationMargin = 40.0F;
        const float activeLeft = camera_.X() - activationMargin;
        const float activeRight = camera_.X() + camera_.ViewportWidth() +
                                  activationMargin;

        for (CrawlerState& crawler : crawlers_) {
            if (crawler.Defeated)
                continue;
            if (crawler.X + CrawlerState::Width < activeLeft ||
                crawler.X > activeRight) {
                crawler.Active = false;
                continue;
            }
            crawler.Active = true;

            const float proposedX = crawler.X +
                                    static_cast<float>(crawler.Direction) *
                                    speed * seconds;
            const float leadingX = crawler.Direction > 0
                ? proposedX + CrawlerState::Width
                : proposedX;
            const int probeTileX = static_cast<int>(std::floor(
                leadingX / TileMap::TileSize));
            const int floorTileY = static_cast<int>(std::floor(
                (crawler.Y + CrawlerState::Height + 1.0F) /
                TileMap::TileSize));
            const bool wall = SolidAabb(proposedX, crawler.Y,
                                        CrawlerState::Width,
                                        CrawlerState::Height);
            const bool edge = crawler.EdgePolicy == CrawlerEdgePolicy::Turn &&
                              !level_.IsSolid(probeTileX, floorTileY);
            if (wall || edge)
                crawler.Direction = -crawler.Direction;
            else
                crawler.X = proposedX;

            if (crawler.EdgePolicy != CrawlerEdgePolicy::Fall)
                continue;
            crawler.VelocityY = std::min(crawler.VelocityY + gravity * seconds,
                                         maximumFallSpeed);
            const float verticalAmount = crawler.VelocityY * seconds;
            crawler.Y += verticalAmount;
            if (!SolidAabb(crawler.X, crawler.Y, CrawlerState::Width,
                           CrawlerState::Height))
                continue;
            if (verticalAmount > 0.0F) {
                const int tileY = static_cast<int>(std::floor(
                    (crawler.Y + CrawlerState::Height - CollisionEpsilon) /
                    TileMap::TileSize));
                crawler.Y = static_cast<float>(tileY * TileMap::TileSize) -
                            CrawlerState::Height;
            }
            else {
                const int tileY = static_cast<int>(std::floor(
                    crawler.Y / TileMap::TileSize));
                crawler.Y = static_cast<float>((tileY + 1) * TileMap::TileSize);
            }
            crawler.VelocityY = 0.0F;
        }
    }

    void WorldSimulation::ResolvePlayerCrawlerContacts(
        const float previousPlayerBottom)
    {
        const float playerRight = player_.X + PlayerState::Width;
        const float playerBottom = player_.Y + PlayerState::Height;
        for (CrawlerState& crawler : crawlers_) {
            if (!crawler.Active || crawler.Defeated)
                continue;
            const bool overlaps = player_.X < crawler.X + CrawlerState::Width &&
                                  playerRight > crawler.X &&
                                  player_.Y < crawler.Y + CrawlerState::Height &&
                                  playerBottom > crawler.Y;
            if (!overlaps)
                continue;

            const bool stomp = player_.VelocityY >= 0.0F &&
                               previousPlayerBottom <= crawler.Y + 2.0F;
            if (stomp) {
                crawler.Defeated = true;
                crawler.Active = false;
                player_.VelocityY = -190.0F;
                player_.Grounded = false;
                ++lastEvents_.EnemiesDefeated;
                lastEvents_.ScoreAdded += 200;
                score_ += 200;
                continue;
            }

            if (player_.InvulnerabilityTicks > 0)
                continue;
            player_.InvulnerabilityTicks = 75;
            if (player_.Plated)
                player_.Plated = false;
            player_.VelocityX = player_.X < crawler.X ? -110.0F : 110.0F;
            player_.VelocityY = -170.0F;
            player_.Grounded = false;
            ++lastEvents_.PlayerDamaged;
        }
    }
}
