// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include "PlayerCollision.hpp"

#include <algorithm>

namespace MeshWorld {

PlayerMoveResult resolve_player_capsule_slide(float start_x, float start_z,
                                               float delta_x, float delta_z,
                                               float feet_y, float head_y,
                                               float radius_m,
                                               const std::vector<CollisionBox>& obstacles) {
    PlayerMoveResult result{start_x, start_z};
    const auto blocked = [&](float x, float z) {
        for (const CollisionBox& box : obstacles) {
            if (head_y < box.min_y || feet_y > box.max_y) continue;
            const float closest_x = std::clamp(x, box.min_x, box.max_x);
            const float closest_z = std::clamp(z, box.min_z, box.max_z);
            const float dx = x - closest_x;
            const float dz = z - closest_z;
            if (dx * dx + dz * dz < radius_m * radius_m) return true;
        }
        return false;
    };

    if (!blocked(start_x + delta_x, start_z)) result.x += delta_x;
    if (!blocked(result.x, start_z + delta_z)) result.z += delta_z;
    return result;
}

} // namespace MeshWorld
