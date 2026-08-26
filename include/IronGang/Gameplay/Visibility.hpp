#pragma once

#include "IronGang/Core/WorldTypes.hpp"

#include <vector>

namespace IronGang
{
    // plan_22 IG-22-002: whether one point can actually see another, or whether the city is in the
    // way.
    //
    // Until now a "witness" was anyone within a radius, wall or no wall -- so a pedestrian inside
    // the warehouse reported a car outside it. Radius alone is a cheap approximation of perception;
    // this is the other half of it, and the reason the police stop chasing the player for things
    // nobody could have seen.
    //
    // Deliberately not a vision **cone**: a cone needs a facing direction per witness and a
    // decision about peripheral vision, and neither changes the outcome nearly as much as noticing
    // that a building is in the way.

    // Whether the segment from @p from to @p to passes through @p box. Slab method, so a segment
    // that starts or ends inside the box counts as intersecting it.
    [[nodiscard]] bool SegmentIntersectsBox(const Vector3& from, const Vector3& to, const WorldBox& box);

    // The same test, additionally reporting **where** along the segment the box is first entered,
    // as a fraction in [0,1] of the way from @p from to @p to. 0 means the segment already starts
    // inside the box. Only meaningful when the function returns true. Camera collision needs the
    // distance, not just the fact, so it can pull the camera in to exactly that point
    // (plan_16 IG-16-003).
    [[nodiscard]] bool SegmentIntersectsBox(const Vector3& from,
                                            const Vector3& to,
                                            const WorldBox& box,
                                            float& entryFraction);

    // True when nothing collidable in @p boxes stands between the two points. Non-collidable
    // boxes -- road markings, trigger markers, the district-exit decal -- are ignored: they are
    // paint on the ground, and treating paint as an occluder would blind every witness standing
    // near a crossing.
    [[nodiscard]] bool HasLineOfSight(const Vector3& from,
                                      const Vector3& to,
                                      const std::vector<WorldBox>& boxes);
}
