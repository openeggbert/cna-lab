#include "IronGang/Gameplay/PedestrianAnimation.hpp"

namespace IronGang
{
    PedestrianAnimation SelectPedestrianAnimation(bool walking, bool turningInPlace, bool fleeing) noexcept
    {
        if (fleeing)
        {
            return PedestrianAnimation::Walk;
        }
        if (turningInPlace)
        {
            return PedestrianAnimation::Turn;
        }
        return walking ? PedestrianAnimation::Walk : PedestrianAnimation::Idle;
    }

    const char* PedestrianAnimationClipName(PedestrianAnimation animation) noexcept
    {
        switch (animation)
        {
            case PedestrianAnimation::Walk: return "Walk";
            case PedestrianAnimation::Turn: return "Turn";
            case PedestrianAnimation::Idle: break;
        }
        return "Idle";
    }

    const char* PedestrianAnimationFallbackClipName(PedestrianAnimation animation) noexcept
    {
        // Only Turn can be missing from an older generated asset; the other two shipped with the
        // first skinned character.
        return animation == PedestrianAnimation::Turn ? "Walk" : PedestrianAnimationClipName(animation);
    }
}
