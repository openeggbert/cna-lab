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
                                      const std::vector<Vector3>& kerbs,
                                      bool mayCross) noexcept
    {
        if (mayCross)
        {
            return kNoObstacleAhead;
        }
        for (const Vector3& kerb : kerbs)
        {
            if (DistanceSquaredXZ(position, kerb) <= kKerbHoldRadiusMetres * kKerbHoldRadiusMetres)
            {
                // Zero clearance is a full stop in Pedestrian::Update()'s existing congestion
                // ramp -- the pedestrian stands still and reads as waiting, not as sliding.
                return 0.0F;
            }
        }
        // Already out in the road: let them finish. A signal changing mid-crossing must not freeze
        // someone in a live lane.
        return kNoObstacleAhead;
    }
}
