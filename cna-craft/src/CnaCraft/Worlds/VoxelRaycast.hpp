#pragma once

#include <optional>

#include "../Core/Vec3.hpp"

namespace CnaCraft::Worlds {

class World;

struct RaycastHit {
    int x = 0, y = 0, z = 0;    // hit block coordinate
    int nx = 0, ny = 0, nz = 0; // face normal, pointing from the hit block toward the ray origin
};

// Amanatides-Woo DDA voxel traversal, stepping cell by cell along the ray up
// to maxDistance and testing World::IsSolid at each cell.
class VoxelRaycast {
public:
    static std::optional<RaycastHit> Cast(const World& world,
                                           Core::Vec3f origin,
                                           Core::Vec3f direction,
                                           float maxDistance);
};

}
