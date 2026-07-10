#pragma once

#include "../Core/Vec3.hpp"

namespace CnaCraft::Worlds {

class World;

struct PlayerInput {
    float moveForward = 0.0f;   // -1..1
    float moveRight = 0.0f;     // -1..1
    // Space: jump when grounded and not flying; force full ascend when
    // flying (Minecraft-style independent fly controls, CRAFT_PARITY.md
    // §1.6 — changed 2026-07-10 from Craft's own pitch-coupled flying per
    // user decision, see the class comment below). Ignored in game mode
    // unless grounded.
    bool jumpPressed = false;
    // Left Shift, flying only: force full descend, symmetric with
    // jumpPressed's ascend. Holding both cancels out to no vertical
    // movement. No effect in game mode (no sneak/crouch implemented).
    bool descendPressed = false;
    float lookDeltaYaw = 0.0f;   // radians, already scaled by mouse sensitivity
    float lookDeltaPitch = 0.0f; // radians, already scaled by mouse sensitivity
};

// First-person controller: gravity + jump + axis-separated AABB-vs-voxel-grid
// collision, the same shape as house3d_demo.cpp's game-mode controller,
// adapted to query World::IsSolid instead of a static box list (plan.md §6).
//
// Fly mode (plan.md §11.4, CRAFT_PARITY.md §1.6): no gravity, horizontal
// movement still collides with the world, always at full speed regardless
// of look direction. Vertical movement is Space=ascend/Shift=descend,
// independent of pitch, matching Minecraft's creative-flight controls —
// **changed 2026-07-10 (user decision) from an earlier version that ported
// Craft's own pitch-coupled get_motion_vector flying branch exactly** (look
// up/down while moving to climb/descend, no dedicated descend key at all).
// That was itself a deliberate Craft-fidelity choice made earlier the same
// day; the user later found it awkward compared to Minecraft's flight feel
// and asked for the more ergonomic scheme instead — a legitimate direction
// change, not a bug fix. Toggle is edge-detected by the caller (CnaCraftGame
// tracks the Tab key) and applied via ToggleFlying() — PlayerInput itself
// carries no mode-switch flag.
class PlayerController {
public:
    // Vertical offset from feet position to EyePosition() -- public so
    // callers converting to/from Craft's own eye-based position storage
    // (e.g. WorldStore's player-state persistence, plan.md §12.1 item 17
    // follow-up) have a single source of truth instead of duplicating the
    // literal.
    static constexpr float kEyeHeight = 1.7f;

    // startYaw/startPitch (plan.md §12.1 item 17 follow-up, player-position
    // persistence) let a caller restore a saved look direction along with
    // position -- default 0,0 preserves every existing call site's behavior
    // unchanged.
    explicit PlayerController(Core::Vec3f startFeetPosition, float startYaw = 0.0f, float startPitch = 0.0f);

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
