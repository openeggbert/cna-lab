// SPDX-License-Identifier: MS-PL
#pragma once

/**
 * @file CNA/Editor/Scene/TransformGizmos3D.hpp
 * @brief The translate manipulator for the 3D viewport (plan.md ED-408).
 *
 * The 3D counterpart of `TransformGizmos.hpp`, and split the same way: layout, hit-test and drag
 * are here, CNA-free and tested in CI, and the renderer is handed finished line segments. What a
 * user can grab is a decision, and decisions do not need a GPU.
 *
 * It is a separate file rather than a mode on the 2D gizmos because almost nothing is shared. The
 * 2D manipulators lay out *in screen space* against `EditorCamera2D` -- two arms, a fixed pixel
 * length, a screen-space drag -- while this one lays out in the world and answers a different
 * question each frame: where along this world-space line is the cursor pointing? The only genuine
 * overlap is `GizmoSnap`, which is reused rather than redeclared.
 *
 * **Translate only, deliberately.** Rotating and scaling in three dimensions need their own
 * gestures -- a ring that has to be picked in screen space and a handle that has to stay grabbable
 * edge-on -- and shipping one manipulator that is right beats three that are approximately right.
 * The 3D view leaves the rest of the mouse alone for them (see ED-409).
 */

#include <optional>
#include <vector>

#include "CNA/Editor/Scene/EditorCamera3D.hpp"
#include "CNA/Editor/Scene/SceneWireframe.hpp"
#include "CNA/Editor/Scene/TransformGizmos.hpp"

namespace CNA::Editor
{
    /** @brief Which arm of the 3D translate gizmo a cursor is over. */
    enum class GizmoAxis3D
    {
        None,
        X,
        Y,
        Z
    };

    /** @brief Returns the stable name of @p axis, for logs and tests. */
    [[nodiscard]] const char* toString(GizmoAxis3D axis);

    /**
     * @brief Where the 3D translate gizmo is, in the world and on the screen.
     *
     * Both, because the two halves need different ones: a drag is solved in the world, where the
     * axis is a line the cursor ray is measured against, while a *grab* is decided on the screen,
     * where "near the arm" means near in pixels however far away the entity is.
     */
    struct TranslateGizmo3DLayout
    {
        /** @brief The entity's world position: where the three arms meet. */
        EditorVector3 origin;

        /** @brief Unit world directions of the X, Y and Z arms, in that order. */
        std::array<EditorVector3, 3> axes{EditorVector3{1.0f, 0.0f, 0.0f}, EditorVector3{0.0f, 1.0f, 0.0f},
                                          EditorVector3{0.0f, 0.0f, 1.0f}};

        /** @brief Length of each arm in world units, chosen so it is a constant size on screen. */
        float armLength = 1.0f;

        /** @brief The origin in viewport pixels. */
        EditorVector2 screenOrigin;

        /** @brief Each arm's far end in viewport pixels. */
        std::array<EditorVector2, 3> screenTips{};

        /** @brief Whether each arm could be projected at all: an arm behind the eye cannot. */
        std::array<bool, 3> armVisible{true, true, true};

        /** @brief How far from an arm, in pixels, still counts as grabbing it. */
        float grabTolerance = 8.0f;
    };

    /** @brief The on-screen length the arms are sized to. Matches the 2D gizmo's, so both feel alike. */
    inline constexpr float kGizmo3DScreenLength = 90.0f;

    /**
     * @brief Returns the layout for @p entityId, or nothing when it has no transform.
     *
     * @param space World points the arms along the world axes; Local points them along the
     *        entity's own, which is what a user placing something inside a rotated rig wants.
     */
    [[nodiscard]] std::optional<TranslateGizmo3DLayout> computeTranslateGizmo3DLayout(
        const SceneDocument& scene, const EditorCamera3D& camera, const Uuid& entityId,
        GizmoSpace space = GizmoSpace::World);

    /**
     * @brief Returns which arm @p screenPoint is over, or None.
     *
     * Nearest arm wins where two overlap on screen, which they do exactly when one is pointing
     * almost at the camera -- the case where a user cannot tell them apart either, and where
     * picking the first in some fixed order would feel arbitrary.
     */
    [[nodiscard]] GizmoAxis3D hitTestTranslateGizmo3D(const TranslateGizmo3DLayout& layout,
                                                      const EditorVector2& screenPoint);

    /**
     * @brief Returns the segments that draw @p layout, with @p active drawn highlighted.
     *
     * In the wireframe's own currency, so the 3D viewport draws its gizmo through exactly the path
     * it draws everything else through.
     */
    [[nodiscard]] std::vector<WireSegment> buildTranslateGizmo3DSegments(
        const TranslateGizmo3DLayout& layout, GizmoAxis3D active = GizmoAxis3D::None);

    /**
     * @brief Converts a world-space delta into the local delta an entity's position needs.
     *
     * The 3D form of `worldDeltaToLocal`. An entity's `position` is relative to its parent, so
     * dragging a child of a rotated or scaled parent one world unit is not a one-unit change to
     * what is stored -- the classic gizmo bug that works on roots and drifts on children.
     */
    [[nodiscard]] EditorVector3 worldDeltaToLocal3D(const SceneDocument& scene, const Uuid& entityId,
                                                    const EditorVector3& worldDelta);

    /**
     * @brief Returns the parameter along @p axis nearest to @p ray, or nothing when they are parallel.
     *
     * The whole of the drag's mathematics: an axis drag asks where along this line the user is
     * pointing, and a line and a ray that never converge have no answer -- which is exactly the
     * case of an arm pointing straight at the camera, where a pixel of cursor movement would
     * otherwise fling the entity across the level.
     */
    [[nodiscard]] std::optional<float> closestPointOnAxis(const WorldRay& ray, const EditorVector3& origin,
                                                          const EditorVector3& axis);

    /**
     * @brief One in-progress 3D translate drag.
     *
     * Holds where the drag began, in world and local space, so every update is computed from the
     * press rather than from the previous frame: accumulating frame-to-frame deltas drifts, and
     * the drift is worst exactly when the pointer is moving fastest.
     */
    class TranslateGizmo3DDrag
    {
    public:
        [[nodiscard]] bool isActive() const { return axis_ != GizmoAxis3D::None; }
        [[nodiscard]] const Uuid& getEntityId() const { return entityId_; }
        [[nodiscard]] GizmoAxis3D getAxis() const { return axis_; }

        /**
         * @brief Starts a drag on the arm under @p cursor.
         *
         * @return True when an arm was grabbed, in which case the press belongs to the gizmo and
         *         must not also reselect whatever is behind it.
         */
        bool begin(const SceneDocument& scene, const EditorCamera3D& camera,
                   const TranslateGizmo3DLayout& layout, const Uuid& entityId,
                   const EditorVector2& cursor);

        /**
         * @brief Returns the entity's new *local* position, or nothing when it has not moved.
         *
         * Nothing rather than the unchanged value, so a drag that has not moved pushes no command
         * -- an undo entry restoring a value the entity already had costs the user an undo to
         * reach a change they can see.
         */
        [[nodiscard]] std::optional<EditorVector3> update(const SceneDocument& scene,
                                                          const EditorCamera3D& camera,
                                                          const EditorVector2& cursor,
                                                          const GizmoSnap& snap);

        void end() { axis_ = GizmoAxis3D::None; }

    private:
        Uuid entityId_;
        GizmoAxis3D axis_ = GizmoAxis3D::None;

        /** @brief The grabbed arm's world direction, fixed at the press. */
        EditorVector3 direction_;

        EditorVector3 startWorld_;
        EditorVector3 startLocal_;

        /** @brief Where along the axis the press pointed, so the entity does not jump to the cursor. */
        float startParameter_ = 0.0f;
    };
}
