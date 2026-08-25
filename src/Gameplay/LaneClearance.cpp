#include "IronGang/Gameplay/LaneClearance.hpp"

namespace IronGang
{
    float DistanceAheadInLane(const Vector3& fromPosition,
                              float fromYaw,
                              const Vector3& obstaclePosition,
                              float laneHalfWidth)
    {
        const Vector3 forward = ForwardFromYaw(fromYaw);
        Vector3 toObstacle = obstaclePosition - fromPosition;
        toObstacle.Y = 0.0F;
        const float forwardDistance = Vector3::Dot(forward, toObstacle);
        if (forwardDistance <= 0.0F)
        {
            return kNoObstacleAhead;
        }
        const Vector3 lateral = toObstacle - forward * forwardDistance;
        if (lateral.Length() > laneHalfWidth)
        {
            return kNoObstacleAhead;
        }
        return forwardDistance;
    }
}
