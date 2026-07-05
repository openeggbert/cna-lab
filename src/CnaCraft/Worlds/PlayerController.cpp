#include "PlayerController.hpp"

#include <cmath>

#include "World.hpp"

namespace CnaCraft::Worlds {

namespace {
constexpr float kEyeHeight = 1.7f;
constexpr float kPlayerHalfWidth = 0.3f;
constexpr float kPlayerHeight = 1.8f;
constexpr float kMoveSpeed = 4.5f;
constexpr float kGravity = 25.0f;
constexpr float kJumpSpeed = 7.0f;
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
                if (world.IsSolid(x, y, z)) return true;
            }
        }
    }
    return false;
}

void PlayerController::Update(const World& world, const PlayerInput& input, float dt) {
    yaw_ += input.lookDeltaYaw;
    pitch_ += input.lookDeltaPitch;
    if (pitch_ > kPitchLimit) pitch_ = kPitchLimit;
    if (pitch_ < -kPitchLimit) pitch_ = -kPitchLimit;

    const float sy = std::sin(yaw_);
    const float cy = std::cos(yaw_);
    // Horizontal-only basis (yaw only, matches house3d_demo.cpp's forwardH/rightH).
    const float forwardX = sy, forwardZ = -cy;
    const float rightX = cy, rightZ = sy;

    const float moveX = (forwardX * input.moveForward + rightX * input.moveRight) * kMoveSpeed;
    const float moveZ = (forwardZ * input.moveForward + rightZ * input.moveRight) * kMoveSpeed;

    if (grounded_ && input.jumpPressed) {
        velocity_.y = kJumpSpeed;
        grounded_ = false;
    }
    velocity_.y -= kGravity * dt;

    Core::Vec3f next = position_;

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
