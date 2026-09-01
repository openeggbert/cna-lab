#pragma once

#include "IronGang/Core/WorldTypes.hpp"

#include <vector>

namespace IronGang
{
    // plan_16 IG-16-003: keep the third-person camera out of the walls.
    //
    // The follow camera is placed a fixed distance behind whatever it is following. Standing with
    // a building behind you puts that fixed point inside the building, and the camera then renders
    // from inside geometry -- looking through a wall, or at the inside faces of one.
    //
    // The fix is the standard one: treat the line from the look-at target to the desired camera
    // position as a ray, and pull the camera in to the first thing it hits. Deliberately a segment
    // test against the district's own collidable boxes rather than a swept sphere through the
    // physics world: the boxes are already there, already tested (`SegmentIntersectsBox`), and a
    // sphere cast would need the camera to become a physics body for a problem a segment solves.

    struct CameraObstruction
    {
        // Where the camera should actually go.
        Vector3 position{};
        // How far along target -> desired it got, in [0,1]. 1 when nothing is in the way.
        float fraction{1.0F};
        bool obstructed{false};
    };

    // How far in front of the wall the camera stops, in metres. The near plane is not zero, so a
    // camera exactly on the surface still clips through it.
    inline constexpr float kCameraCollisionSkinMetres = 0.35F;
    // How close to the target the camera may ever get, in metres. This is a **distance**, not a
    // fraction of the boom: as a fraction it silently scales with the boom length, and at 7.5 m
    // behind the player even a modest fraction is more than a metre -- which pushed the camera
    // straight back into a wall the player was standing a metre in front of. When the obstruction
    // is nearer than this, the wall wins and the camera sits on its surface; there is no good
    // answer at that range, and a camera inside the player's own model is the worse one.
    inline constexpr float kCameraMinimumDistanceMetres = 0.6F;

    // Non-collidable boxes are ignored, for the same reason HasLineOfSight() ignores them: road
    // markings and trigger decals are paint on the ground, and pulling the camera in to a lane
    // stripe would make it unusable everywhere on a road.
    [[nodiscard]] CameraObstruction ResolveCameraObstruction(const Vector3& target,
                                                             const Vector3& desiredCamera,
                                                             const std::vector<WorldBox>& boxes,
                                                             float skinMetres = kCameraCollisionSkinMetres,
                                                             float minimumDistanceMetres =
                                                                 kCameraMinimumDistanceMetres);
}
