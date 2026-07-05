#pragma once

#include "../Core/Vec3.hpp"

namespace CnaCraft::Worlds {

class World;

struct PlayerInput {
    float moveForward = 0.0f;   // -1..1
    float moveRight = 0.0f;     // -1..1
    bool jumpPressed = false;
    float lookDeltaYaw = 0.0f;   // radians, already scaled by mouse sensitivity
    float lookDeltaPitch = 0.0f; // radians, already scaled by mouse sensitivity
};

// First-person controller: gravity + jump + axis-separated AABB-vs-voxel-grid
// collision, the same shape as house3d_demo.cpp's game-mode controller,
// adapted to query World::IsSolid instead of a static box list (plan.md §6).
class PlayerController {
public:
    explicit PlayerController(Core::Vec3f startFeetPosition);

    void Update(const World& world, const PlayerInput& input, float dt);

    [[nodiscard]] Core::Vec3f EyePosition() const;
    [[nodiscard]] Core::Vec3f LookDirection() const;
    [[nodiscard]] float Yaw() const { return yaw_; }
    [[nodiscard]] float Pitch() const { return pitch_; }
    [[nodiscard]] bool IsGrounded() const { return grounded_; }

private:
    [[nodiscard]] bool CollidesAt(const World& world, Core::Vec3f feetPosition) const;

    Core::Vec3f position_; // feet position (bottom-center of the player AABB)
    Core::Vec3f velocity_{0.0f, 0.0f, 0.0f};
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    bool grounded_ = false;
};

}
