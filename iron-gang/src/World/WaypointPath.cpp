#include "IronGang/World/WaypointPath.hpp"

#include <algorithm>
#include <cmath>

namespace IronGang
{
    float AdvanceAlongPath(const WaypointPath& path,
                           Vector3& position,
                           std::size_t& targetIndex,
                           float speed,
                           float deltaSeconds,
                           float arrivalRadius,
                           float previousYaw)
    {
        if (path.Empty() || speed <= 0.0F)
        {
            return previousYaw;
        }
        if (targetIndex >= path.points.size())
        {
            targetIndex = 0;
        }

        Vector3 toTarget = path.points[targetIndex] - position;
        toTarget.Y = 0.0F;
        float distance = toTarget.Length();

        if (distance <= arrivalRadius)
        {
            if (targetIndex + 1 < path.points.size())
            {
                ++targetIndex;
            }
            else if (path.loop)
            {
                targetIndex = 0;
            }
            toTarget = path.points[targetIndex] - position;
            toTarget.Y = 0.0F;
            distance = toTarget.Length();
        }

        if (distance <= 1e-4F)
        {
            return previousYaw;
        }

        const Vector3 direction = toTarget / distance;
        const float step = std::min(distance, speed * deltaSeconds);
        position += direction * step;
        return std::atan2(direction.X, -direction.Z); // matches ForwardFromYaw's own convention
    }
}
