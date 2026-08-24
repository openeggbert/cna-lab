#pragma once

#include "CopperBoots/Camera2D.hpp"
#include "CopperBoots/LevelDefinition.hpp"
#include "CopperBoots/TileMap.hpp"

namespace CopperBoots
{
    struct PlayerInput
    {
        float Move = 0.0F;
        bool Run = false;
        bool JumpPressed = false;
        bool JumpHeld = false;
    };

    enum class PlayerMotion
    {
        Standing,
        Walking,
        Running,
        Jumping,
        Falling,
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
        PlayerMotion Motion = PlayerMotion::Falling;
    };

    class WorldSimulation
    {
    public:
        WorldSimulation();

        void LoadLevel(LevelDefinition level);
        void Update(const PlayerInput& input, float seconds);
        void ResetPlayer();

        [[nodiscard]] const TileMap& Level() const noexcept { return level_; }
        [[nodiscard]] const PlayerState& Player() const noexcept { return player_; }
        [[nodiscard]] const Camera2D& Camera() const noexcept { return camera_; }
        [[nodiscard]] const std::array<float, 3>& ParallaxFactors() const noexcept
        {
            return parallaxFactors_;
        }

    private:
        [[nodiscard]] bool Collides(float x, float y, float width,
                                    float height) const noexcept;
        void MoveHorizontal(float amount);
        void MoveVertical(float amount);
        void UpdateMotion(const PlayerInput& input) noexcept;

        TileMap level_;
        PlayerState player_;
        Camera2D camera_;
        float spawnX_;
        float spawnY_;
        std::array<float, 3> parallaxFactors_{0.10F, 0.25F, 0.50F};
    };
}
