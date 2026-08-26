#include "IronGang/Gameplay/Visibility.hpp"

#include <algorithm>
#include <cmath>

namespace IronGang
{
    bool SegmentIntersectsBox(const Vector3& from, const Vector3& to, const WorldBox& box)
    {
        const Vector3 direction = to - from;
        const Vector3 half = box.size * 0.5F;
        const Vector3 minimum = box.center - half;
        const Vector3 maximum = box.center + half;

        float entry = 0.0F;
        float exit = 1.0F;

        const float origin[3] = {from.X, from.Y, from.Z};
        const float delta[3] = {direction.X, direction.Y, direction.Z};
        const float low[3] = {minimum.X, minimum.Y, minimum.Z};
        const float high[3] = {maximum.X, maximum.Y, maximum.Z};

        for (int axis = 0; axis < 3; ++axis)
        {
            if (std::fabs(delta[axis]) < 1e-6F)
            {
                // Parallel to this pair of slabs: either the segment is inside them for its whole
                // length, or it can never be inside the box at all.
                if (origin[axis] < low[axis] || origin[axis] > high[axis])
                {
                    return false;
                }
                continue;
            }
            float near = (low[axis] - origin[axis]) / delta[axis];
            float far = (high[axis] - origin[axis]) / delta[axis];
            if (near > far)
            {
                std::swap(near, far);
            }
            entry = std::max(entry, near);
            exit = std::min(exit, far);
            if (entry > exit)
            {
                return false;
            }
        }
        return true;
    }

    bool HasLineOfSight(const Vector3& from, const Vector3& to, const std::vector<WorldBox>& boxes)
    {
        for (const WorldBox& box : boxes)
        {
            if (!box.collidable)
            {
                continue; // paint on the ground, not a wall
            }
            if (SegmentIntersectsBox(from, to, box))
            {
                return false;
            }
        }
        return true;
    }
}
