#include "VoxelRaycast.hpp"

#include <cmath>
#include <limits>

#include "World.hpp"

namespace CnaCraft::Worlds {

namespace {

float TDelta(float d) {
    return d != 0.0f ? std::abs(1.0f / d) : std::numeric_limits<float>::infinity();
}

float InitialT(float originComp, int voxel, int step, float d) {
    if (step == 0) return std::numeric_limits<float>::infinity();
    const float boundary = step > 0 ? static_cast<float>(voxel + 1) : static_cast<float>(voxel);
    return (boundary - originComp) / d;
}

}

std::optional<RaycastHit> VoxelRaycast::Cast(const World& world,
                                              Core::Vec3f origin,
                                              Core::Vec3f dir,
                                              float maxDistance) {
    const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len < 1e-6f) return std::nullopt;
    dir.x /= len;
    dir.y /= len;
    dir.z /= len;

    int x = static_cast<int>(std::floor(origin.x));
    int y = static_cast<int>(std::floor(origin.y));
    int z = static_cast<int>(std::floor(origin.z));

    const int stepX = dir.x > 0.0f ? 1 : (dir.x < 0.0f ? -1 : 0);
    const int stepY = dir.y > 0.0f ? 1 : (dir.y < 0.0f ? -1 : 0);
    const int stepZ = dir.z > 0.0f ? 1 : (dir.z < 0.0f ? -1 : 0);

    const float tDeltaX = TDelta(dir.x);
    const float tDeltaY = TDelta(dir.y);
    const float tDeltaZ = TDelta(dir.z);

    float tMaxX = InitialT(origin.x, x, stepX, dir.x);
    float tMaxY = InitialT(origin.y, y, stepY, dir.y);
    float tMaxZ = InitialT(origin.z, z, stepZ, dir.z);

    // Which axis we last stepped on — gives the hit face's normal.
    int lastStepAxis = -1;
    float t = 0.0f;

    while (t <= maxDistance) {
        if (world.IsSolid(x, y, z)) {
            RaycastHit hit{x, y, z, 0, 0, 0};
            if (lastStepAxis == 0) hit.nx = -stepX;
            else if (lastStepAxis == 1) hit.ny = -stepY;
            else if (lastStepAxis == 2) hit.nz = -stepZ;
            return hit;
        }

        if (tMaxX < tMaxY && tMaxX < tMaxZ) {
            x += stepX;
            t = tMaxX;
            tMaxX += tDeltaX;
            lastStepAxis = 0;
        } else if (tMaxY < tMaxZ) {
            y += stepY;
            t = tMaxY;
            tMaxY += tDeltaY;
            lastStepAxis = 1;
        } else {
            z += stepZ;
            t = tMaxZ;
            tMaxZ += tDeltaZ;
            lastStepAxis = 2;
        }
    }

    return std::nullopt;
}

}
