#include "PlayerController.hpp"

#include <cmath>

#include "World.hpp"

namespace CnaCraft::Worlds {

namespace {
constexpr float kEyeHeight = 1.7f;
constexpr float kPlayerHalfWidth = 0.3f;
constexpr float kPlayerHeight = 1.8f;
constexpr float kMoveSpeed = 4.5f;
// Exactly 4x kMoveSpeed (CRAFT_PARITY.md §1.5), matching Craft's own
// walk=5/fly=20 ratio (main.c) -- changed from an earlier 2x value
// (9.0f) per user decision 2026-07-10 to minimize differences from Craft.
constexpr float kFlySpeed = 18.0f;
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
// Exactly Craft's own clamp (`s->ry = MAX(s->ry, -RADIANS(90)); MIN(...,
// RADIANS(90))`, main.c:handle_mouse_input) -- changed from an earlier
// 1.55f (~89 degrees) approximation per user decision 2026-07-10.
constexpr float kPitchLimit = 1.57079632679489661923f; // exactly pi/2

// Collision substepping (CRAFT_PARITY.md §1.7): Craft explicitly breaks a
// frame's movement into `step = MAX(8, estimate)` substeps (main.c,
// handle_movement), each resolved against collision separately, so a fast
// frame's total displacement can never skip clean over a thin (1-block)
// obstacle without ever being tested against it -- CollidesAt only tests
// discrete positions, not the swept path between them, so without this a
// large enough per-call `dt` (a dropped/hitched frame, or a future speed
// increase) could tunnel through a wall. kMinSubsteps mirrors Craft's own
// floor of 8. kMaxSubsteps is a deliberate addition Craft's own code
// doesn't have (Craft's `step` is otherwise unbounded) -- a defensive cap
// so a pathologically large one-off `dt` can't blow up this frame's cost.
constexpr int kMinSubsteps = 8;
constexpr int kMaxSubsteps = 64;
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

    // Pitch-coupled flight (CRAFT_PARITY.md §1.6): ports Craft's own
    // get_motion_vector's flying branch exactly (main.c) -- there is no
    // dedicated descend key in real Craft at all. Moving forward/back while
    // flying scales horizontal speed by cos(pitch) and adds a vertical
    // component of sin(pitch) (flipped when moving backward, since backward
    // is away from where you're looking); strafing alone (no forward/back
    // input) uses full horizontal speed and has no vertical component at
    // all. Space (input.jumpPressed) then unconditionally overrides this
    // computed vertical speed with a full-speed ascend, matching Craft's own
    // `if (flying) vy = 1` after get_motion_vector.
    float flyHorizontalScale = 1.0f;
    float flyVerticalFraction = 0.0f;
    if (flying_ && (moveForwardInput != 0.0f || moveRightInput != 0.0f)) {
        flyHorizontalScale = std::cos(pitch_);
        flyVerticalFraction = std::sin(pitch_);
        const bool strafing = moveRightInput != 0.0f;
        const bool movingForwardOrBack = moveForwardInput != 0.0f;
        if (strafing) {
            if (!movingForwardOrBack) flyVerticalFraction = 0.0f;
            flyHorizontalScale = 1.0f;
        }
        if (moveForwardInput < 0.0f) flyVerticalFraction = -flyVerticalFraction;
    }
    float flyVerticalSpeed = flyVerticalFraction * kFlySpeed;
    if (flying_ && input.jumpPressed) flyVerticalSpeed = kFlySpeed;

    const float speed = flying_ ? kFlySpeed * flyHorizontalScale : kMoveSpeed;
    const float moveX = (forwardX * moveForwardInput + rightX * moveRightInput) * speed;
    const float moveZ = (forwardZ * moveForwardInput + rightZ * moveRightInput) * speed;

    // Estimate how many substeps this frame's movement needs (see the
    // kMinSubsteps/kMaxSubsteps comment above) -- Craft's own formula
    // (main.c) scales substep count with total estimated speed * dt; ported
    // with the same shape, using this project's own speed constants.
    const float verticalSpeedEstimate = flying_ ? flyVerticalSpeed : velocity_.y;
    const float totalSpeedEstimate =
        std::sqrt(moveX * moveX + verticalSpeedEstimate * verticalSpeedEstimate + moveZ * moveZ);
    const int estimate = static_cast<int>(std::round(totalSpeedEstimate * dt * 8.0f));
    const int step = std::min(kMaxSubsteps, std::max(kMinSubsteps, estimate));
    const float subDt = dt / static_cast<float>(step);

    for (int i = 0; i < step; ++i) {
        Core::Vec3f next = position_;

        if (flying_) {
            // Fly mode (plan.md §11.4): no gravity. Horizontal axes still
            // collide with the world (matches house3d_demo.cpp's fly
            // branch); vertical does not, so the player can fly through
            // floors/ceilings on purpose.
            next.x = position_.x + moveX * subDt;
            if (CollidesAt(world, next)) next.x = position_.x;

            next.z = position_.z + moveZ * subDt;
            if (CollidesAt(world, Core::Vec3f{next.x, position_.y, next.z})) next.z = position_.z;

            next.y = position_.y + flyVerticalSpeed * subDt;
            grounded_ = false;
        } else {
            // Jump impulse and grounded state are only ever set/consumed
            // once per Update() call: grounded_ flips to false the instant
            // the impulse is applied, so subsequent substeps within this
            // same call can't re-trigger it (equivalent to Craft's own
            // static `dy` accumulator, just applied at substep granularity
            // instead of via a persistent global).
            if (grounded_ && input.jumpPressed) {
                velocity_.y = kJumpSpeed;
                grounded_ = false;
            }
            velocity_.y -= kGravity * subDt;
            if (velocity_.y < kTerminalVelocity) velocity_.y = kTerminalVelocity;

            next.x = position_.x + moveX * subDt;
            if (CollidesAt(world, next)) next.x = position_.x;

            next.z = position_.z + moveZ * subDt;
            if (CollidesAt(world, Core::Vec3f{next.x, position_.y, next.z})) next.z = position_.z;

            next.y = position_.y + velocity_.y * subDt;
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

    // Floor-catch safety net (CRAFT_PARITY.md §1.8): ports Craft's own
    // `if (s->y < 0) s->y = highest_block(s->x, s->z) + 2` (main.c), a
    // last-resort recovery if the player ever ends up below the world.
    // Checked once per Update() call, same as Craft's own placement (after
    // the substep loop, not inside it). Matched exactly, including not
    // resetting velocity_.y afterward -- a genuine last-resort net, not
    // expected to trigger during normal play now that y=0 is always solid
    // Bedrock wherever a column is actually loaded (World::GenerateColumn).
    if (position_.y < 0.0f) {
        const int nx = static_cast<int>(std::round(position_.x));
        const int nz = static_cast<int>(std::round(position_.z));
        // HighestCollidableY's -1 ("no ground") is ambiguous under
        // streaming (plan.md §12.1 item 19): it now also means "this
        // column isn't loaded yet", not just "loaded but genuinely empty".
        // Skip the catch rather than trusting -1+2=1 in that case --
        // CnaCraftGame's own streaming loop loads a column around the
        // player's current position every frame regardless of y, so this
        // resolves itself within a frame or two once real terrain exists
        // to catch on.
        if (world.IsColumnLoaded(ChunkCoordOf(nx), ChunkCoordOf(nz))) {
            position_.y = static_cast<float>(world.HighestCollidableY(nx, nz) + 2);
        }
    }
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
