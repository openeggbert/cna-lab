#include "IronGang/Gameplay/PedestrianCrossing.hpp"

namespace IronGang
{
    bool PedestrianMayCross(SignalPhase trafficPhase, bool signalControlled) noexcept
    {
        if (!signalControlled)
        {
            // An unsignalled crossing is a give-way, and giving way is the driver's job. Modelling
            // it as a pedestrian wait would make people stand at an empty road forever.
            return true;
        }
        return trafficPhase == SignalPhase::Red;
    }

    float PedestrianCrossingClearance(const Vector3& position,
                                      const Vector3& targetPoint,
                                      const std::vector<CrossingPair>& crossings,
                                      SignalPhase trafficPhase) noexcept
    {
        constexpr float kTargetMatchRadius = 0.5F;
        for (const CrossingPair& crossing : crossings)
        {
            if (PedestrianMayCross(trafficPhase, crossing.signalControlled))
            {
                continue;
            }
            const Vector3* standingAt = nullptr;
            const Vector3* headingFor = nullptr;
            if (DistanceSquaredXZ(position, crossing.kerbA) <=
                kKerbHoldRadiusMetres * kKerbHoldRadiusMetres)
            {
                standingAt = &crossing.kerbA;
                headingFor = &crossing.kerbB;
            }
            else if (DistanceSquaredXZ(position, crossing.kerbB) <=
                     kKerbHoldRadiusMetres * kKerbHoldRadiusMetres)
            {
                standingAt = &crossing.kerbB;
                headingFor = &crossing.kerbA;
            }
            if (standingAt == nullptr)
            {
                // Not at a kerb. Either walking the pavement, or already out in the road -- and
                // someone in the road must be let finish, or a signal changing mid-crossing
                // freezes them in a live lane.
                continue;
            }
            if (DistanceSquaredXZ(targetPoint, *headingFor) <= kTargetMatchRadius * kTargetMatchRadius)
            {
                // Zero clearance is a full stop in Pedestrian::Update()'s existing congestion
                // ramp -- the pedestrian stands still and reads as waiting, not as sliding.
                return 0.0F;
            }
        }
        return kNoObstacleAhead;
    }
}
