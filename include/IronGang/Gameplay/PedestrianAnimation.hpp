#pragma once

namespace IronGang
{
    // plan_20 IG-20-003: the three locomotion states a pedestrian's animation can be in. Kept as a
    // pure function of the locomotion flags, separate from both the renderer and the AI, so the
    // mapping can be tested without a graphics device and changed without touching either side.
    enum class PedestrianAnimation
    {
        Idle,
        Walk,
        Turn,
    };

    // fleeing wins over everything: someone running from a car is not idling, and is not politely
    // turning on the spot either. Then turning-in-place, which is the state a pedestrian is in
    // while it reverses at the end of a pavement -- it is deliberately *not* walking, so picking
    // Walk here would slide a walk cycle across the ground while the body pivots.
    [[nodiscard]] PedestrianAnimation SelectPedestrianAnimation(bool walking,
                                                                bool turningInPlace,
                                                                bool fleeing) noexcept;

    // The clip this state asks the character model for.
    [[nodiscard]] const char* PedestrianAnimationClipName(PedestrianAnimation animation) noexcept;

    // What to play when the model has no clip by that name. Generated character assets are not
    // committed (assets/generated is ignored), so a checkout whose asset build predates the Turn
    // clip must still animate rather than freeze mid-pose. Walk is the honest stand-in for
    // turning: the legs move, which is most of what a pivot looks like.
    [[nodiscard]] const char* PedestrianAnimationFallbackClipName(PedestrianAnimation animation) noexcept;
}
