// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#include "Mc3Collision.hpp"

#include <vector>

namespace MeshWorld {

struct PlayerMoveResult {
    float x{0.0f};
    float z{0.0f};
};

// Resolves one horizontal player move against AABB obstacles using the app's
// established X-then-Z slide convention.  Kept outside the SDL/CNA app so the
// collision contract can be unit-tested in the core library.
PlayerMoveResult resolve_player_capsule_slide(float start_x, float start_z,
                                               float delta_x, float delta_z,
                                               float feet_y, float head_y,
                                               float radius_m,
                                               const std::vector<CollisionBox>& obstacles);

} // namespace MeshWorld
