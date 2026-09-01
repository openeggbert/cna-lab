#include "IronGang/Gameplay/CameraCollision.hpp"

#include "IronGang/Gameplay/Visibility.hpp"

#include <algorithm>

namespace IronGang
{
    CameraObstruction ResolveCameraObstruction(const Vector3& target,
                                               const Vector3& desiredCamera,
                                               const std::vector<WorldBox>& boxes,
                                               float skinMetres,
                                               float minimumDistanceMetres)
    {
        CameraObstruction result;
        result.position = desiredCamera;

        const Vector3 offset = desiredCamera - target;
        const float distance = offset.Length();
        if (distance <= 1e-4F)
        {
            return result; // nothing to pull in along
        }

        float nearest = 1.0F;
        for (const WorldBox& box : boxes)
        {
            if (!box.collidable)
            {
                continue;
            }
            float entry = 0.0F;
            if (SegmentIntersectsBox(target, desiredCamera, box, entry) && entry < nearest)
            {
                nearest = entry;
            }
        }
        if (nearest >= 1.0F)
        {
            return result;
        }

        const float hitDistance = nearest * distance;
        float resolved = 0.0F;
        if (hitDistance <= 1e-4F)
        {
            // The target is already inside geometry -- there is no unobstructed spot anywhere
            // along the boom. Keep the minimum standoff and accept the clipping: a camera inside
            // the player's own model is the worse of the two.
            resolved = std::min(minimumDistanceMetres, distance);
        }
        else
        {
            // Stop short of the surface, because a camera exactly on it still clips through: the
            // near plane has depth. When the wall is nearer than the minimum standoff there is no
            // room for the skin, and the wall wins -- the camera sits on the surface rather than
            // being pushed back through it.
            resolved = std::max(hitDistance - skinMetres, std::min(minimumDistanceMetres, hitDistance));
        }
        resolved = std::clamp(resolved, 0.0F, distance);

        result.fraction = resolved / distance;
        result.obstructed = true;
        result.position = target + offset * result.fraction;
        return result;
    }
}
