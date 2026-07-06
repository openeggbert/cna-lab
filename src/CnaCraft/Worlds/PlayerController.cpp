#include "PlayerController.hpp"

#include <cmath>

#include "World.hpp"

namespace CnaCraft::Worlds {

namespace {
constexpr float kEyeHeight = 1.7f;
constexpr float kPlayerHalfWidth = 0.3f;
constexpr float kPlayerHeight = 1.8f;
constexpr float kMoveSpeed = 4.5f;
constexpr float kFlySpeed = 9.0f; // faster than walking, matches Craft's flying speed being 4x
constexpr float kGravity = 25.0f;
// Bug fix: 7.0 gives a max jump height of v^2/(2g) = 49/50 = 0.98 blocks --
// mathematically just short of clearing a full 1-block step, the single
// most basic Craft-like traversal move. Craft's own jump speed is 8
// (`dy = 8` in src/main.c) against the same gravity=25, giving 64/50=1.28
// blocks -- comfortably enough margin. Matched here.
constexpr float kJumpSpeed = 8.0f;
constexpr float kPitchLimit = 1.55f; // ~89 degrees
}

PlayerController::PlayerController(Core::Vec3f startFeetPosition) : position_(startFeetPosition) {}

bool PlayerController::CollidesAt(const World& world, Core::Vec3f feet) const {
    const int minX = static_cast<int>(std::floor(feet.x - kPlayerHalfWidth));
    const int maxX = static_cast<int>(std::floor(feet.x + kPlayerHalfWidth));
    const int minY = static_cast<int>(std::floor(feet.y));
    const int maxY = static_cast<int>(std::floor(feet.y + kPlayerHeight));
    const int minZ = static_cast<int>(std::floor(feet.z - kPlayerHalfWidth));
    const int maxZ = static_cast<int>(std::floor(feet.z + kPlayerHalfWidth));

    for (int y = minY; y <= maxY; ++y) {
        for (int z = minZ; z <= maxZ; ++z) {
            for (int x = minX; x <= maxX; ++x) {
                if (world.IsCollidable(x, y, z)) return true;
            }
        }
    }
    return false;
}

void PlayerController::Update(const World& world, const PlayerInput& input, float dt) {
    yaw_ += input.lookDeltaYaw;
    // Keep yaw bounded for long play sessions (Craft wraps s->rx the same way).
    constexpr float kTwoPi = 6.28318530717958647692f;
    if (yaw_ >= kTwoPi) yaw_ -= kTwoPi;
    if (yaw_ < 0.0f) yaw_ += kTwoPi;

    pitch_ += input.lookDeltaPitch;
    if (pitch_ > kPitchLimit) pitch_ = kPitchLimit;
    if (pitch_ < -kPitchLimit) pitch_ = -kPitchLimit;

    const float sy = std::sin(yaw_);
    const float cy = std::cos(yaw_);
    // Horizontal-only basis (yaw only, matches house3d_demo.cpp's forwardH/rightH).
    const float forwardX = sy, forwardZ = -cy;
    const float rightX = cy, rightZ = sy;

    Core::Vec3f next = position_;

    if (flying_) {
        // Fly mode (plan.md §11.4): no gravity, free vertical movement.
        // Horizontal axes still collide with the world (matches
        // house3d_demo.cpp's fly branch); vertical does not, so the player
        // can fly through floors/ceilings on purpose.
        const float moveX = (forwardX * input.moveForward + rightX * input.moveRight) * kFlySpeed;
        const float moveZ = (forwardZ * input.moveForward + rightZ * input.moveRight) * kFlySpeed;

        next.x = position_.x + moveX * dt;
        if (CollidesAt(world, next)) next.x = position_.x;

        next.z = position_.z + moveZ * dt;
        if (CollidesAt(world, Core::Vec3f{next.x, position_.y, next.z})) next.z = position_.z;

        next.y = position_.y + input.moveUp * kFlySpeed * dt;
        grounded_ = false;
    } else {
        const float moveX = (forwardX * input.moveForward + rightX * input.moveRight) * kMoveSpeed;
        const float moveZ = (forwardZ * input.moveForward + rightZ * input.moveRight) * kMoveSpeed;

        if (grounded_ && input.jumpPressed) {
            velocity_.y = kJumpSpeed;
            grounded_ = false;
        }
        velocity_.y -= kGravity * dt;

        next.x = position_.x + moveX * dt;
        if (CollidesAt(world, next)) next.x = position_.x;

        next.z = position_.z + moveZ * dt;
        if (CollidesAt(world, Core::Vec3f{next.x, position_.y, next.z})) next.z = position_.z;

        next.y = position_.y + velocity_.y * dt;
        if (CollidesAt(world, Core::Vec3f{next.x, next.y, next.z})) {
            if (velocity_.y < 0.0f) grounded_ = true;
            velocity_.y = 0.0f;
            next.y = position_.y;
        } else {
            grounded_ = false;
        }
    }

    position_ = next;
}

Core::Vec3f PlayerController::EyePosition() const {
    return Core::Vec3f{position_.x, position_.y + kEyeHeight, position_.z};
}

Core::Vec3f PlayerController::LookDirection() const {
    const float cp = std::cos(pitch_);
    const float sp = std::sin(pitch_);
    const float cy = std::cos(yaw_);
    const float sy = std::sin(yaw_);
    return Core::Vec3f{cp * sy, sp, -cp * cy};
}

}
