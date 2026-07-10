#pragma once

#include "../Core/Vec3.hpp"

namespace CnaCraft::Worlds {

class World;

struct PlayerInput {
    float moveForward = 0.0f;   // -1..1
    float moveRight = 0.0f;     // -1..1
    // Space, in both modes -- matches Craft's own CRAFT_KEY_JUMP double duty
    // (main.c: jump when grounded and not flying, force full ascend when
    // flying). Ignored in game mode unless grounded.
    bool jumpPressed = false;
    float lookDeltaYaw = 0.0f;   // radians, already scaled by mouse sensitivity
    float lookDeltaPitch = 0.0f; // radians, already scaled by mouse sensitivity
};

// First-person controller: gravity + jump + axis-separated AABB-vs-voxel-grid
// collision, the same shape as house3d_demo.cpp's game-mode controller,
// adapted to query World::IsSolid instead of a static box list (plan.md §6).
//
// Fly mode (plan.md §11.4, CRAFT_PARITY.md §1.6): no gravity, horizontal
// movement still collides with the world. Vertical movement is
// pitch-coupled, matching Craft's own get_motion_vector's flying branch
// exactly (main.c) -- there is no dedicated descend key in real Craft at
// all: looking up/down while moving forward/back trades horizontal speed
// for climb/descend, strafing alone has no vertical component, and Space
// always forces a full-speed ascend regardless of pitch. Toggle is
// edge-detected by the caller (CnaCraftGame tracks the Tab key) and applied
// via ToggleFlying() — PlayerInput itself carries no mode-switch flag.
class PlayerController {
public:
    explicit PlayerController(Core::Vec3f startFeetPosition);

    void Update(const World& world, const PlayerInput& input, float dt);
    void ToggleFlying() { flying_ = !flying_; if (flying_) velocity_.y = 0.0f; }

    [[nodiscard]] Core::Vec3f EyePosition() const;
    [[nodiscard]] Core::Vec3f LookDirection() const;
    [[nodiscard]] float Yaw() const { return yaw_; }
    [[nodiscard]] float Pitch() const { return pitch_; }
    [[nodiscard]] bool IsGrounded() const { return grounded_; }
    [[nodiscard]] bool IsFlying() const { return flying_; }

    // Whether the player's current AABB covers the given block cell
    // (CRAFT_PARITY.md §2.6) — ports Craft's `player_intersects_block`
    // check used by `on_right_click` to reject a placement that would
    // overlap the player's own body. Uses the exact same cell-enumeration
    // bounds as CollidesAt, so a cell flagged here is guaranteed to be one
    // CollidesAt would also flag for this player's current position.
    [[nodiscard]] bool IntersectsBlock(int bx, int by, int bz) const;

private:
    [[nodiscard]] bool CollidesAt(const World& world, Core::Vec3f feetPosition) const;

    Core::Vec3f position_; // feet position (bottom-center of the player AABB)
    Core::Vec3f velocity_{0.0f, 0.0f, 0.0f};
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    bool grounded_ = false;
    bool flying_ = false;
};

}
