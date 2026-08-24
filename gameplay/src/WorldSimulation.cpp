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
        level_ = std::move(level.Map);
        spawnX_ = static_cast<float>(level.SpawnTileX * TileMap::TileSize);
        spawnY_ = static_cast<float>(level.SpawnFootTileY * TileMap::TileSize) -
                  PlayerState::Height;
        parallaxFactors_ = level.ParallaxFactors;
        camera_.SetWorldBounds(static_cast<float>(level_.PixelWidth()),
                               static_cast<float>(level_.PixelHeight()));
        ResetPlayer();
    }

    void WorldSimulation::ResetPlayer()
    {
        player_ = {};
        player_.X = spawnX_;
        player_.Y = spawnY_;
        player_.Grounded = true;
        player_.FacingRight = true;
        player_.Motion = PlayerMotion::Standing;
        camera_.SnapTo(player_.X + PlayerState::Width * 0.5F,
                       player_.Y + PlayerState::Height * 0.5F);
    }

    void WorldSimulation::Update(const PlayerInput& input, const float seconds)
    {
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

        UpdateMotion(input);
        camera_.Update(player_.X + PlayerState::Width * 0.5F,
                       player_.Y + PlayerState::Height * 0.5F,
                       player_.VelocityX, seconds);
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
}
