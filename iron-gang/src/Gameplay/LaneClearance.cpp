#include "IronGang/Gameplay/LaneClearance.hpp"

namespace IronGang
{
    bool CrossedLine(const Vector3& previousPosition,
                     const Vector3& currentPosition,
                     const Vector3& linePosition,
                     float approachYaw,
                     float laneHalfWidth)
    {
        const Vector3 forward = ForwardFromYaw(approachYaw);
        Vector3 fromPrevious = linePosition - previousPosition;
        Vector3 fromCurrent = linePosition - currentPosition;
        fromPrevious.Y = 0.0F;
        fromCurrent.Y = 0.0F;

        // Ahead of the line before, behind it after: that is a crossing, and only in the direction
        // the line governs (a car reversing back over it has not run it again).
        if (Vector3::Dot(forward, fromPrevious) <= 0.0F || Vector3::Dot(forward, fromCurrent) > 0.0F)
        {
            return false;
        }

        // ...and it has to have passed within the lane, not through the pavement beside it.
        const float lateral = (fromCurrent - forward * Vector3::Dot(forward, fromCurrent)).Length();
        return lateral <= laneHalfWidth;
    }

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
