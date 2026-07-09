#include "PlayerController.hpp"

#include <cmath>

#include "World.hpp"

namespace CnaCraft::Worlds {

namespace {
constexpr float kEyeHeight = 1.7f;
constexpr float kPlayerHalfWidth = 0.3f;
constexpr float kPlayerHeight = 1.8f;
constexpr float kMoveSpeed = 4.5f;
// NOTE (CRAFT_PARITY.md §1.5): this is 2x kMoveSpeed, not 4x. Craft's own
// ratio is 4x (walk=5, fly=20, main.c). Left as-is rather than doubled to
// 18.0 -- unlike the jump-height fix (2127b8c, a literal "can't clear a
// step" bug), fly-speed is a subjective tuning value with no broken
// mechanic to fix, so changing it is a gameplay-feel decision, not a bug
// fix; marked needs_human in CRAFT_PARITY.md/plan.md §12.1 if revisited.
constexpr float kFlySpeed = 9.0f;
constexpr float kGravity = 25.0f;
// Craft clamps fall speed to -250 units/s (`dy = MAX(dy, -250)`, main.c) so a
// long fall doesn't accelerate forever; ported as-is (CRAFT_PARITY.md §1.8).
constexpr float kTerminalVelocity = -250.0f;
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

bool PlayerController::IntersectsBlock(int bx, int by, int bz) const {
    const int minX = static_cast<int>(std::floor(position_.x - kPlayerHalfWidth));
    const int maxX = static_cast<int>(std::floor(position_.x + kPlayerHalfWidth));
    const int minY = static_cast<int>(std::floor(position_.y));
    const int maxY = static_cast<int>(std::floor(position_.y + kPlayerHeight));
    const int minZ = static_cast<int>(std::floor(position_.z - kPlayerHalfWidth));
    const int maxZ = static_cast<int>(std::floor(position_.z + kPlayerHalfWidth));
    return bx >= minX && bx <= maxX && by >= minY && by <= maxY && bz >= minZ && bz <= maxZ;
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

    // Normalize the (moveForward, moveRight) input to a unit vector before
    // scaling by speed (CRAFT_PARITY.md §1.5) -- matches Craft's own
    // get_motion_vector, which always produces a unit motion vector via
    // atan2f(sz, sx) regardless of how many movement keys are held. Without
    // this, holding two movement keys at once (e.g. W+D) moved sqrt(2)x
    // faster than a single key -- a real bug relative to Craft, not a
    // deliberate design choice.
    float moveForwardInput = input.moveForward;
    float moveRightInput = input.moveRight;
    const float inputLenSq = moveForwardInput * moveForwardInput + moveRightInput * moveRightInput;
    if (inputLenSq > 1.0e-8f) {
        const float invLen = 1.0f / std::sqrt(inputLenSq);
        moveForwardInput *= invLen;
        moveRightInput *= invLen;
    }

    Core::Vec3f next = position_;

    if (flying_) {
        // Fly mode (plan.md §11.4): no gravity, free vertical movement.
        // Horizontal axes still collide with the world (matches
        // house3d_demo.cpp's fly branch); vertical does not, so the player
        // can fly through floors/ceilings on purpose.
        const float moveX = (forwardX * moveForwardInput + rightX * moveRightInput) * kFlySpeed;
        const float moveZ = (forwardZ * moveForwardInput + rightZ * moveRightInput) * kFlySpeed;

        next.x = position_.x + moveX * dt;
        if (CollidesAt(world, next)) next.x = position_.x;

        next.z = position_.z + moveZ * dt;
        if (CollidesAt(world, Core::Vec3f{next.x, position_.y, next.z})) next.z = position_.z;

        next.y = position_.y + input.moveUp * kFlySpeed * dt;
        grounded_ = false;
    } else {
        const float moveX = (forwardX * moveForwardInput + rightX * moveRightInput) * kMoveSpeed;
        const float moveZ = (forwardZ * moveForwardInput + rightZ * moveRightInput) * kMoveSpeed;

        if (grounded_ && input.jumpPressed) {
            velocity_.y = kJumpSpeed;
            grounded_ = false;
        }
        velocity_.y -= kGravity * dt;
        if (velocity_.y < kTerminalVelocity) velocity_.y = kTerminalVelocity;

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
