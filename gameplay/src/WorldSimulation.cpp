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
        constexpr float StompBounceImpulse = 300.0F;
        constexpr float HighStompBounceImpulse = 390.0F;
        constexpr float MaximumFallSpeed = 420.0F;
        constexpr float EarlyReleaseGravityMultiplier = 2.2F;
        constexpr float CollisionEpsilon = 0.001F;
        constexpr float StompContactTolerance = 2.0F;
        constexpr float MinimumStompOverlap = 2.0F;

        [[nodiscard]] float Approach(const float value, const float target,
                                     const float maximumDelta)
        {
            if (value < target)
                return std::min(value + maximumDelta, target);
            return std::max(value - maximumDelta, target);
        }
    }

    EnemyContactKind ClassifyEnemyContact(
        const MotionBounds& player, const MotionBounds& enemy) noexcept
    {
        const float playerRight = player.CurrentX + player.Width;
        const float playerBottom = player.CurrentY + player.Height;
        const float enemyRight = enemy.CurrentX + enemy.Width;
        const float enemyBottom = enemy.CurrentY + enemy.Height;
        const bool overlapsNow = player.CurrentX < enemyRight &&
                                 playerRight > enemy.CurrentX &&
                                 player.CurrentY < enemyBottom &&
                                 playerBottom > enemy.CurrentY;
        if (!overlapsNow)
            return EnemyContactKind::None;

        const float previousPlayerBottom = player.PreviousY + player.Height;
        const float previousEnemyTop = enemy.PreviousY;
        const float previousSeparation = previousPlayerBottom - previousEnemyTop;
        const float currentSeparation = playerBottom - enemy.CurrentY;
        const float relativeDownwardMotion = currentSeparation -
                                             previousSeparation;
        if (relativeDownwardMotion <= CollisionEpsilon ||
            previousSeparation > StompContactTolerance ||
            currentSeparation < 0.0F) {
            return EnemyContactKind::Harmful;
        }

        const float crossingTime = std::clamp(
            -previousSeparation / relativeDownwardMotion, 0.0F, 1.0F);
        const float playerCrossingX = player.PreviousX +
            (player.CurrentX - player.PreviousX) * crossingTime;
        const float enemyCrossingX = enemy.PreviousX +
            (enemy.CurrentX - enemy.PreviousX) * crossingTime;
        const float horizontalOverlap = std::min(
            playerCrossingX + player.Width, enemyCrossingX + enemy.Width) -
            std::max(playerCrossingX, enemyCrossingX);
        return horizontalOverlap >= MinimumStompOverlap
            ? EnemyContactKind::Stomp
            : EnemyContactKind::Harmful;
    }

    EnemyContactKind HighestPriorityEnemyContact(
        const std::span<const EnemyContactKind> contacts) noexcept
    {
        EnemyContactKind result = EnemyContactKind::None;
        for (const EnemyContactKind contact : contacts) {
            if (contact == EnemyContactKind::Stomp)
                return contact;
            if (contact == EnemyContactKind::Harmful)
                result = contact;
        }
        return result;
    }

    PlayerPose SelectPlayerPose(const PlayerState& player,
                                const std::uint64_t tick) noexcept
    {
        if (player.Dead || player.Motion == PlayerMotion::Dead)
            return PlayerPose::Dead;
        if (player.InvulnerabilityTicks > 0 &&
            (player.InvulnerabilityTicks / 3) % 2 == 0) {
            return PlayerPose::DamageBlink;
        }
        switch (player.Motion) {
        case PlayerMotion::Walking:
            return (tick / 8U) % 2U == 0U
                ? PlayerPose::WalkA
                : PlayerPose::WalkB;
        case PlayerMotion::Running:
            return (tick / 4U) % 2U == 0U
                ? PlayerPose::RunA
                : PlayerPose::RunB;
        case PlayerMotion::Jumping:
            return PlayerPose::Rise;
        case PlayerMotion::Falling:
            return PlayerPose::Fall;
        case PlayerMotion::Dead:
            return PlayerPose::Dead;
        case PlayerMotion::Standing:
        case PlayerMotion::Transition:
            return PlayerPose::Idle;
        }
        return PlayerPose::Idle;
    }

    WorldSimulation::WorldSimulation()
        : level_(TileMap::CreateTestRoom()),
          camera_(320.0F, 180.0F),
          spawnX_(3.0F * static_cast<float>(TileMap::TileSize)),
          spawnY_(9.0F * static_cast<float>(TileMap::TileSize) - PlayerState::Height),
          checkpointX_(spawnX_),
          checkpointY_(spawnY_)
    {
        camera_.SetWorldBounds(static_cast<float>(level_.PixelWidth()),
                               static_cast<float>(level_.PixelHeight()));
        ResetPlayer();
    }

    void WorldSimulation::LoadLevel(LevelDefinition level)
    {
        levelName_ = std::move(level.Name);
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
            CrawlerState state;
            state.X = static_cast<float>(
                crawler.Position.X * TileMap::TileSize) + 1.0F;
            state.Y = static_cast<float>(
                (crawler.Position.Y + 1) * TileMap::TileSize) -
                CrawlerState::Height;
            state.PreviousX = state.X;
            state.PreviousY = state.Y;
            state.EdgePolicy = crawler.FallsAtEdges
                ? CrawlerEdgePolicy::Fall
                : CrawlerEdgePolicy::Turn;
            crawlers_.push_back(state);
        }
        crawlerContacts_.assign(crawlers_.size(), EnemyContactKind::None);
        platingPickups_.clear();
        platingPickups_.reserve(level.PlatingPickups.size());
        for (const TileCoordinate& pickup : level.PlatingPickups) {
            platingPickups_.push_back({
                static_cast<float>(pickup.X * TileMap::TileSize) + 2.0F,
                static_cast<float>(pickup.Y * TileMap::TileSize) + 2.0F,
                0.0F, 1, 0, false});
        }
        capacitorPickups_.clear();
        capacitorPickups_.reserve(level.CapacitorPickups.size());
        for (const TileCoordinate& pickup : level.CapacitorPickups) {
            capacitorPickups_.push_back({
                static_cast<float>(pickup.X * TileMap::TileSize) + 3.0F,
                static_cast<float>(pickup.Y * TileMap::TileSize) + 3.0F,
                0, false});
        }
        checkpoints_.clear();
        checkpoints_.reserve(level.Checkpoints.size());
        for (const TileCoordinate& checkpoint : level.Checkpoints)
            checkpoints_.push_back({checkpoint.X, checkpoint.Y, false});
        projectiles_ = {};
        interactiveBlocks_.clear();
        interactiveBlocks_.reserve(level.InteractiveBlocks.size());
        for (const InteractiveBlockDefinition& block : level.InteractiveBlocks) {
            interactiveBlocks_.push_back({block.Position.X, block.Position.Y,
                                          block.Content, false, 0});
        }
        routeEndpoints_ = std::move(level.RouteEndpoints);
        routes_ = std::move(level.Routes);
        currentArea_ = std::move(level.InitialArea);
        routeTransition_ = {};
        routeInteractionLocked_ = false;
        level_ = std::move(level.Map);
        spawnX_ = static_cast<float>(level.SpawnTileX * TileMap::TileSize);
        spawnY_ = static_cast<float>(level.SpawnFootTileY * TileMap::TileSize) -
                  PlayerState::Height;
        checkpointX_ = static_cast<float>(
            level.CheckpointTileX * TileMap::TileSize);
        checkpointY_ = static_cast<float>(
            level.CheckpointFootTileY * TileMap::TileSize) - PlayerState::Height;
        parallaxFactors_ = level.ParallaxFactors;
        collectedCogs_ = 0;
        score_ = 0;
        lives_ = 3;
        tickCount_ = 0;
        result_ = {};
        completionTicks_ = 0;
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
        if (result_.Completed) {
            completionTicks_ = std::min(completionTicks_ + 1, 60);
            ++tickCount_;
            return;
        }
        if (!input.InteractHeld)
            routeInteractionLocked_ = false;
        if (routeTransition_.Active) {
            UpdateRouteTransition();
            ++tickCount_;
            return;
        }
        if (player_.Dead) {
            if (player_.DeathTicksRemaining > 0)
                --player_.DeathTicksRemaining;
            if (player_.DeathTicksRemaining == 0) {
                RespawnAtCheckpoint();
                ++lastEvents_.PlayerRespawned;
            }
            ++tickCount_;
            return;
        }
        if (player_.InvulnerabilityTicks > 0)
            --player_.InvulnerabilityTicks;
        if (player_.PowerTransitionTicks > 0)
            --player_.PowerTransitionTicks;
        if (TryStartRouteTransition(input)) {
            ++tickCount_;
            return;
        }
        const float previousPlayerX = player_.X;
        const float previousPlayerY = player_.Y;
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
            StartPlayerDeath();

        if (!player_.Dead && TouchesCollision(TileCollision::Hazard))
            StartPlayerDeath();

        if (!player_.Dead) {
            ActivateOverlappingCheckpoints();
            if (TouchesCollision(TileCollision::Exit)) {
                StartLevelCompletion();
                camera_.Update(player_.X + PlayerState::Width * 0.5F,
                               player_.Y + PlayerState::Height * 0.5F,
                               0.0F, seconds);
                ++tickCount_;
                return;
            }
        }

        UpdateCrawlers(seconds);
        UpdatePlatingPickups(seconds);
        UpdateCapacitorPickups();
        if (!player_.Dead) {
            ResolvePlayerCrawlerContacts(previousPlayerX, previousPlayerY,
                                          input.JumpHeld);
            CollectOverlappingCogs();
            CollectOverlappingPlatingPickups();
            CollectOverlappingCapacitorPickups();
            TryFireProjectile(input);
        }
        UpdateProjectiles(seconds);
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

        const float previousY = player_.Y;
        player_.Y += amount;
        const bool hitSolid = Collides(player_.X, player_.Y,
                                       PlayerState::Width, PlayerState::Height);
        if (!hitSolid && amount > 0.0F) {
            const float previousBottom = previousY + PlayerState::Height;
            const float currentBottom = player_.Y + PlayerState::Height;
            const int tileY = static_cast<int>(std::floor(
                (currentBottom - CollisionEpsilon) / TileMap::TileSize));
            const float tileTop = static_cast<float>(tileY * TileMap::TileSize);
            const int left = static_cast<int>(std::floor(
                player_.X / TileMap::TileSize));
            const int right = static_cast<int>(std::floor(
                (player_.X + PlayerState::Width - CollisionEpsilon) /
                TileMap::TileSize));
            if (previousBottom <= tileTop + CollisionEpsilon &&
                currentBottom >= tileTop) {
                for (int tileX = left; tileX <= right; ++tileX) {
                    if (level_.Get(tileX, tileY).Collision !=
                        TileCollision::OneWay)
                        continue;
                    player_.Y = tileTop - PlayerState::Height;
                    player_.VelocityY = 0.0F;
                    player_.Grounded = true;
                    return;
                }
            }
        }
        if (!hitSolid)
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
        if (player_.Dead) {
            player_.Motion = PlayerMotion::Dead;
        }
        else if (!player_.Grounded) {
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
            else if (block.Content == BlockContent::Plating) {
                platingPickups_.push_back({
                    static_cast<float>(tileX * TileMap::TileSize) + 2.0F,
                    static_cast<float>(tileY * TileMap::TileSize),
                    0.0F, 1, 24, false});
                ++lastEvents_.BlockContentsReleased;
                ++lastEvents_.PowerUpsReleased;
            }
            else if (block.Content == BlockContent::Capacitor) {
                capacitorPickups_.push_back({
                    static_cast<float>(tileX * TileMap::TileSize) + 3.0F,
                    static_cast<float>(tileY * TileMap::TileSize),
                    24, false});
                ++lastEvents_.BlockContentsReleased;
                ++lastEvents_.CapacitorsReleased;
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

    float WorldSimulation::RouteFadeAmount() const noexcept
    {
        if (!routeTransition_.Active)
            return 0.0F;
        const int elapsed = RouteTransitionState::TotalTicks -
                            routeTransition_.TicksRemaining;
        const int phase = std::min(elapsed,
            RouteTransitionState::TotalTicks - elapsed);
        return static_cast<float>(phase) /
               static_cast<float>(RouteTransitionState::DestinationTick);
    }

    bool WorldSimulation::TryStartRouteTransition(
        const PlayerInput& input) noexcept
    {
        if (!input.InteractHeld || routeInteractionLocked_ ||
            !player_.Grounded || routeEndpoints_.empty()) {
            return false;
        }

        constexpr float HorizontalAlignmentTolerance = 4.0F;
        constexpr float VerticalAlignmentTolerance = 0.01F;
        const float playerCenter = player_.X + PlayerState::Width * 0.5F;
        const float playerFoot = player_.Y + PlayerState::Height;
        for (std::size_t endpointIndex = 0;
             endpointIndex < routeEndpoints_.size(); ++endpointIndex) {
            const RouteEndpointDefinition& source =
                routeEndpoints_[endpointIndex];
            if (source.Area != currentArea_)
                continue;
            const float endpointCenter = static_cast<float>(
                source.Position.X * TileMap::TileSize) +
                TileMap::TileSize * 0.5F;
            const float endpointFoot = static_cast<float>(
                source.Position.Y * TileMap::TileSize);
            if (std::abs(playerCenter - endpointCenter) >
                    HorizontalAlignmentTolerance ||
                std::abs(playerFoot - endpointFoot) >
                    VerticalAlignmentTolerance) {
                continue;
            }

            for (const RouteDefinition& route : routes_) {
                if (route.Source != source.Name)
                    continue;
                for (std::size_t destinationIndex = 0;
                     destinationIndex < routeEndpoints_.size();
                     ++destinationIndex) {
                    const RouteEndpointDefinition& destination =
                        routeEndpoints_[destinationIndex];
                    if (destination.Name != route.Destination)
                        continue;
                    player_.X = endpointCenter - PlayerState::Width * 0.5F;
                    player_.VelocityX = 0.0F;
                    player_.VelocityY = 0.0F;
                    player_.Motion = PlayerMotion::Transition;
                    routeTransition_ = {
                        true, false, source.Area != destination.Area,
                        RouteTransitionState::TotalTicks,
                        endpointIndex, destinationIndex};
                    routeInteractionLocked_ = true;
                    ++lastEvents_.RouteTransitionsStarted;
                    return true;
                }
            }
        }
        return false;
    }

    void WorldSimulation::UpdateRouteTransition() noexcept
    {
        if (!routeTransition_.Active)
            return;
        if (routeTransition_.TicksRemaining > 0)
            --routeTransition_.TicksRemaining;
        if (!routeTransition_.DestinationReached &&
            routeTransition_.TicksRemaining ==
                RouteTransitionState::DestinationTick) {
            const RouteEndpointDefinition& destination =
                routeEndpoints_[routeTransition_.DestinationEndpoint];
            player_.X = static_cast<float>(
                destination.Position.X * TileMap::TileSize) +
                (TileMap::TileSize - PlayerState::Width) * 0.5F;
            player_.Y = static_cast<float>(
                destination.Position.Y * TileMap::TileSize) -
                PlayerState::Height;
            currentArea_ = destination.Area;
            routeTransition_.DestinationReached = true;
            camera_.SnapTo(player_.X + PlayerState::Width * 0.5F,
                           player_.Y + PlayerState::Height * 0.5F);
            ++lastEvents_.RouteDestinationsReached;
        }
        if (routeTransition_.TicksRemaining > 0)
            return;

        routeTransition_.Active = false;
        routeInteractionLocked_ = true;
        player_.Grounded = Collides(player_.X, player_.Y + 1.0F,
                                    PlayerState::Width, PlayerState::Height);
        player_.Motion = player_.Grounded
            ? PlayerMotion::Standing
            : PlayerMotion::Falling;
        ++lastEvents_.RouteTransitionsCompleted;
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
            crawler.PreviousX = crawler.X;
            crawler.PreviousY = crawler.Y;
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

        for (std::size_t first = 0; first < crawlers_.size(); ++first) {
            CrawlerState& a = crawlers_[first];
            if (!a.Active || a.Defeated)
                continue;
            for (std::size_t second = first + 1; second < crawlers_.size();
                 ++second) {
                CrawlerState& b = crawlers_[second];
                if (!b.Active || b.Defeated)
                    continue;
                const bool overlaps = a.X < b.X + CrawlerState::Width &&
                    a.X + CrawlerState::Width > b.X &&
                    a.Y < b.Y + CrawlerState::Height &&
                    a.Y + CrawlerState::Height > b.Y;
                if (!overlaps)
                    continue;

                const bool aWasLeft =
                    a.PreviousX + CrawlerState::Width * 0.5F <=
                    b.PreviousX + CrawlerState::Width * 0.5F;
                a.X = a.PreviousX;
                b.X = b.PreviousX;
                a.Direction = aWasLeft ? -1 : 1;
                b.Direction = aWasLeft ? 1 : -1;
            }
        }
    }

    void WorldSimulation::ResolvePlayerCrawlerContacts(
        const float previousPlayerX, const float previousPlayerY,
        const bool jumpHeld)
    {
        const MotionBounds playerBounds{
            previousPlayerX, previousPlayerY, player_.X, player_.Y,
            PlayerState::Width, PlayerState::Height};
        std::fill(crawlerContacts_.begin(), crawlerContacts_.end(),
                  EnemyContactKind::None);
        for (std::size_t index = 0; index < crawlers_.size(); ++index) {
            const CrawlerState& crawler = crawlers_[index];
            if (!crawler.Active || crawler.Defeated)
                continue;
            crawlerContacts_[index] = ClassifyEnemyContact(playerBounds, {
                crawler.PreviousX, crawler.PreviousY, crawler.X, crawler.Y,
                CrawlerState::Width, CrawlerState::Height});
        }

        const EnemyContactKind priority = HighestPriorityEnemyContact(
            crawlerContacts_);
        if (priority == EnemyContactKind::None)
            return;
        if (priority == EnemyContactKind::Stomp) {
            int defeatedCount = 0;
            for (std::size_t index = 0; index < crawlers_.size(); ++index) {
                if (crawlerContacts_[index] != EnemyContactKind::Stomp)
                    continue;
                CrawlerState& crawler = crawlers_[index];
                crawler.Defeated = true;
                crawler.Active = false;
                ++lastEvents_.EnemiesDefeated;
                lastEvents_.ScoreAdded += 200;
                score_ += 200;
                ++defeatedCount;
            }
            if (defeatedCount > 0) {
                player_.VelocityY = jumpHeld
                    ? -HighStompBounceImpulse
                    : -StompBounceImpulse;
                player_.Grounded = false;
            }
            return;
        }

        if (player_.InvulnerabilityTicks > 0)
            return;
        std::size_t damagingIndex = 0;
        while (damagingIndex < crawlerContacts_.size() &&
               crawlerContacts_[damagingIndex] != EnemyContactKind::Harmful) {
            ++damagingIndex;
        }
        if (damagingIndex == crawlerContacts_.size())
            return;

        const CrawlerState& crawler = crawlers_[damagingIndex];
        if (player_.Plated) {
            player_.Plated = false;
            player_.InvulnerabilityTicks = 75;
        }
        else {
            StartPlayerDeath();
        }
        const float playerCenter = player_.X + PlayerState::Width * 0.5F;
        const float crawlerCenter = crawler.X + CrawlerState::Width * 0.5F;
        player_.VelocityX = playerCenter < crawlerCenter ? -110.0F : 110.0F;
        player_.VelocityY = -170.0F;
        player_.Grounded = false;
        ++lastEvents_.PlayerDamaged;
    }

    void WorldSimulation::StartPlayerDeath() noexcept
    {
        if (player_.Dead)
            return;
        player_.Dead = true;
        player_.DeathTicksRemaining = 45;
        player_.Motion = PlayerMotion::Dead;
        player_.VelocityX = 0.0F;
        player_.VelocityY = 0.0F;
        lives_ = std::max(0, lives_ - 1);
        ++lastEvents_.PlayerDied;
    }

    void WorldSimulation::RespawnAtCheckpoint()
    {
        player_ = {};
        player_.X = checkpointX_;
        player_.Y = checkpointY_;
        player_.Grounded = Collides(player_.X, player_.Y + 1.0F,
                                    PlayerState::Width, PlayerState::Height);
        player_.FacingRight = true;
        player_.Motion = player_.Grounded
            ? PlayerMotion::Standing
            : PlayerMotion::Falling;
        camera_.SnapTo(player_.X + PlayerState::Width * 0.5F,
                       player_.Y + PlayerState::Height * 0.5F);
    }

    bool WorldSimulation::TouchesCollision(
        const TileCollision collision) const noexcept
    {
        const int left = static_cast<int>(std::floor(
            player_.X / TileMap::TileSize));
        const int right = static_cast<int>(std::floor(
            (player_.X + PlayerState::Width - CollisionEpsilon) /
            TileMap::TileSize));
        const int top = static_cast<int>(std::floor(
            player_.Y / TileMap::TileSize));
        const int bottom = static_cast<int>(std::floor(
            (player_.Y + PlayerState::Height - CollisionEpsilon) /
            TileMap::TileSize));
        for (int tileY = top; tileY <= bottom; ++tileY) {
            for (int tileX = left; tileX <= right; ++tileX) {
                if (level_.Get(tileX, tileY).Collision == collision)
                    return true;
            }
        }
        return false;
    }

    void WorldSimulation::ActivateOverlappingCheckpoints() noexcept
    {
        const float playerRight = player_.X + PlayerState::Width;
        const float playerBottom = player_.Y + PlayerState::Height;
        for (CheckpointState& checkpoint : checkpoints_) {
            if (checkpoint.Activated)
                continue;
            const float x = static_cast<float>(checkpoint.TileX * TileMap::TileSize);
            const float y = static_cast<float>(checkpoint.TileY * TileMap::TileSize);
            const bool overlaps = player_.X < x + TileMap::TileSize &&
                                  playerRight > x &&
                                  player_.Y < y + TileMap::TileSize &&
                                  playerBottom > y;
            if (!overlaps)
                continue;
            for (CheckpointState& other : checkpoints_)
                other.Activated = false;
            checkpoint.Activated = true;
            checkpointX_ = x;
            checkpointY_ = static_cast<float>(
                (checkpoint.TileY + 1) * TileMap::TileSize) - PlayerState::Height;
            ++lastEvents_.CheckpointsActivated;
            return;
        }
    }

    void WorldSimulation::StartLevelCompletion() noexcept
    {
        if (result_.Completed)
            return;
        result_.Completed = true;
        result_.Score = score_;
        result_.CollectedCogs = collectedCogs_;
        result_.CompletionTick = tickCount_;
        completionTicks_ = 1;
        player_.Motion = PlayerMotion::Transition;
        player_.VelocityX = 0.0F;
        player_.VelocityY = 0.0F;
        ++lastEvents_.LevelCompleted;
    }

    void WorldSimulation::UpdatePlatingPickups(const float seconds)
    {
        constexpr float horizontalSpeed = 28.0F;
        constexpr float gravity = 900.0F;
        constexpr float maximumFallSpeed = 300.0F;
        for (PlatingPickupState& pickup : platingPickups_) {
            if (pickup.Collected)
                continue;
            if (pickup.EmergenceTicks > 0) {
                pickup.Y -= 0.5F;
                --pickup.EmergenceTicks;
                continue;
            }

            const float proposedX = pickup.X +
                static_cast<float>(pickup.Direction) * horizontalSpeed * seconds;
            if (SolidAabb(proposedX, pickup.Y, PlatingPickupState::Width,
                          PlatingPickupState::Height)) {
                pickup.Direction = -pickup.Direction;
            }
            else {
                pickup.X = proposedX;
            }

            pickup.VelocityY = std::min(pickup.VelocityY + gravity * seconds,
                                        maximumFallSpeed);
            const float verticalAmount = pickup.VelocityY * seconds;
            pickup.Y += verticalAmount;
            if (!SolidAabb(pickup.X, pickup.Y, PlatingPickupState::Width,
                           PlatingPickupState::Height))
                continue;
            if (verticalAmount > 0.0F) {
                const int tileY = static_cast<int>(std::floor(
                    (pickup.Y + PlatingPickupState::Height - CollisionEpsilon) /
                    TileMap::TileSize));
                pickup.Y = static_cast<float>(tileY * TileMap::TileSize) -
                           PlatingPickupState::Height;
            }
            else {
                const int tileY = static_cast<int>(std::floor(
                    pickup.Y / TileMap::TileSize));
                pickup.Y = static_cast<float>((tileY + 1) * TileMap::TileSize);
            }
            pickup.VelocityY = 0.0F;
        }
    }

    void WorldSimulation::CollectOverlappingPlatingPickups() noexcept
    {
        const float playerRight = player_.X + PlayerState::Width;
        const float playerBottom = player_.Y + PlayerState::Height;
        for (PlatingPickupState& pickup : platingPickups_) {
            if (pickup.Collected)
                continue;
            const bool overlaps = player_.X < pickup.X + PlatingPickupState::Width &&
                                  playerRight > pickup.X &&
                                  player_.Y < pickup.Y + PlatingPickupState::Height &&
                                  playerBottom > pickup.Y;
            if (!overlaps)
                continue;
            pickup.Collected = true;
            player_.Plated = true;
            player_.PowerTransitionTicks = 18;
            score_ += 500;
            lastEvents_.ScoreAdded += 500;
            ++lastEvents_.PowerUpsCollected;
        }
    }

    void WorldSimulation::UpdateCapacitorPickups() noexcept
    {
        for (CapacitorPickupState& pickup : capacitorPickups_) {
            if (!pickup.Collected && pickup.EmergenceTicks > 0) {
                pickup.Y -= 0.5F;
                --pickup.EmergenceTicks;
            }
        }
    }

    void WorldSimulation::CollectOverlappingCapacitorPickups() noexcept
    {
        const float playerRight = player_.X + PlayerState::Width;
        const float playerBottom = player_.Y + PlayerState::Height;
        for (CapacitorPickupState& pickup : capacitorPickups_) {
            if (pickup.Collected || pickup.EmergenceTicks > 0)
                continue;
            const bool overlaps = player_.X < pickup.X + CapacitorPickupState::Size &&
                                  playerRight > pickup.X &&
                                  player_.Y < pickup.Y + CapacitorPickupState::Size &&
                                  playerBottom > pickup.Y;
            if (!overlaps)
                continue;
            pickup.Collected = true;
            player_.ArcCapacitor = true;
            player_.PowerTransitionTicks = 18;
            score_ += 750;
            lastEvents_.ScoreAdded += 750;
            ++lastEvents_.CapacitorsCollected;
        }
    }

    void WorldSimulation::TryFireProjectile(const PlayerInput& input) noexcept
    {
        if (!input.AttackPressed || !player_.ArcCapacitor)
            return;
        for (ProjectileState& projectile : projectiles_) {
            if (projectile.Active)
                continue;
            const int aim = std::clamp(input.Aim, -1, 1);
            const float facing = player_.FacingRight ? 1.0F : -1.0F;
            projectile.Active = true;
            projectile.X = player_.FacingRight
                ? player_.X + PlayerState::Width
                : player_.X - ProjectileState::Size;
            projectile.Y = player_.Y + 7.0F;
            projectile.VelocityX = facing * (aim == 0 ? 160.0F : 110.0F);
            projectile.VelocityY = aim < 0 ? -150.0F : (aim > 0 ? 85.0F : 0.0F);
            ++lastEvents_.ProjectilesFired;
            return;
        }
    }

    void WorldSimulation::UpdateProjectiles(const float seconds)
    {
        constexpr float gravity = 420.0F;
        constexpr float bounceVelocity = -125.0F;
        const float cleanupLeft = camera_.X() - 40.0F;
        const float cleanupRight = camera_.X() + camera_.ViewportWidth() + 40.0F;
        for (ProjectileState& projectile : projectiles_) {
            if (!projectile.Active)
                continue;

            const float proposedX = projectile.X + projectile.VelocityX * seconds;
            if (SolidAabb(proposedX, projectile.Y, ProjectileState::Size,
                          ProjectileState::Size)) {
                projectile.Active = false;
                continue;
            }
            projectile.X = proposedX;
            projectile.VelocityY = std::min(projectile.VelocityY + gravity * seconds,
                                            300.0F);
            const float verticalAmount = projectile.VelocityY * seconds;
            projectile.Y += verticalAmount;
            if (SolidAabb(projectile.X, projectile.Y, ProjectileState::Size,
                          ProjectileState::Size)) {
                if (verticalAmount > 0.0F) {
                    const int tileY = static_cast<int>(std::floor(
                        (projectile.Y + ProjectileState::Size - CollisionEpsilon) /
                        TileMap::TileSize));
                    projectile.Y = static_cast<float>(tileY * TileMap::TileSize) -
                                   ProjectileState::Size;
                    projectile.VelocityY = bounceVelocity;
                }
                else {
                    projectile.Active = false;
                    continue;
                }
            }

            for (CrawlerState& crawler : crawlers_) {
                if (crawler.Defeated)
                    continue;
                const bool overlaps = projectile.X < crawler.X + CrawlerState::Width &&
                                      projectile.X + ProjectileState::Size > crawler.X &&
                                      projectile.Y < crawler.Y + CrawlerState::Height &&
                                      projectile.Y + ProjectileState::Size > crawler.Y;
                if (!overlaps)
                    continue;
                crawler.Defeated = true;
                crawler.Active = false;
                projectile.Active = false;
                ++lastEvents_.EnemiesDefeated;
                lastEvents_.ScoreAdded += 200;
                score_ += 200;
                break;
            }

            if (projectile.X + ProjectileState::Size < cleanupLeft ||
                projectile.X > cleanupRight || projectile.Y < -40.0F ||
                projectile.Y > static_cast<float>(level_.PixelHeight() + 40))
                projectile.Active = false;
        }
    }
}
