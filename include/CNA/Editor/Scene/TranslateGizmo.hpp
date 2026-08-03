// SPDX-License-Identifier: MS-PL
#pragma once

/**
 * @file CNA/Editor/Scene/TranslateGizmo.hpp
 * @brief The translate gizmo's geometry, hit-testing and drag state.
 *
 * CNA-free, like the camera and the picker, and for the same reason: the hard parts of a gizmo are
 * arithmetic, not rendering. Where the handles are, which one the cursor is over, and what world
 * delta a drag implies are all questions with exact answers that can be checked in CI. Only the
 * lines and squares need a graphics device, and those are the easy part.
 *
 * Handles are sized in **screen** pixels rather than world units, so the gizmo stays the same size
 * whatever the zoom. A gizmo that shrinks as you zoom out is one you cannot grab exactly when you
 * most need to.
 */

#include <optional>

#include "CNA/Editor/Core/EditorMath.hpp"
#include "CNA/Editor/Core/Uuid.hpp"
#include "CNA/Editor/Scene/EditorCamera2D.hpp"

namespace CNA::Editor
{
    class SceneDocument;

    /** @brief Which part of the translate gizmo the cursor is over. */
    enum class GizmoHandle
    {
        None,
        /** @brief The X arm: dragging moves along world X only. */
        XAxis,
        /** @brief The Y arm: dragging moves along world Y only. */
        YAxis,
        /** @brief The centre square: dragging moves freely on both axes. */
        Both
    };

    /** @brief The gizmo's screen-space layout. All values are pixels. */
    struct TranslateGizmoLayout
    {
        /** @brief The entity's world position, in screen pixels. */
        EditorVector2 origin;

        /** @brief Length of each axis arm. */
        float axisLength = 72.0f;

        /** @brief Half-extent of the centre square. */
        float centerExtent = 11.0f;

        /** @brief How far from an arm still counts as grabbing it. */
        float grabTolerance = 7.0f;

        /** @brief Returns the far end of the X arm. */
        [[nodiscard]] EditorVector2 getXTip() const { return EditorVector2{origin.x + axisLength, origin.y}; }

        /** @brief Returns the far end of the Y arm. */
        [[nodiscard]] EditorVector2 getYTip() const { return EditorVector2{origin.x, origin.y + axisLength}; }
    };

    /**
     * @brief Returns the gizmo layout for @p entityId, or std::nullopt when it has no transform.
     */
    [[nodiscard]] std::optional<TranslateGizmoLayout> computeTranslateGizmoLayout(
        const SceneDocument& scene, const EditorCamera2D& camera, const Uuid& entityId);

    /**
     * @brief Returns which handle @p screenPoint is over.
     *
     * The centre square wins over the arms where they overlap: it is the smaller target and the
     * one a user aiming for "move freely" expects, and the arms remain reachable everywhere else
     * along their length.
     */
    [[nodiscard]] GizmoHandle hitTestTranslateGizmo(const TranslateGizmoLayout& layout,
                                                    const EditorVector2& screenPoint);

    /**
     * @brief Converts a world-space delta into the local delta an entity's position needs.
     *
     * An entity's `position` property is relative to its parent, so dragging a child of a rotated
     * or scaled parent by one world unit is not a one-unit change to its stored position. Getting
     * this wrong makes a gizmo that works perfectly on root entities and drifts on every child.
     */
    [[nodiscard]] EditorVector2 worldDeltaToLocal(const SceneDocument& scene,
                                                  const Uuid& entityId,
                                                  const EditorVector2& worldDelta);

    /**
     * @brief One in-progress gizmo drag.
     *
     * Records where the drag began, in both screen and local space, so every update computes the
     * offset from the *original* position rather than accumulating frame deltas. Accumulating is
     * the classic source of gizmo drift: rounding error compounds, and the object slowly separates
     * from the cursor over a long drag.
     */
    class TranslateGizmoDrag
    {
    public:
        /** @brief Returns true while a drag is in progress. */
        [[nodiscard]] bool isActive() const { return handle_ != GizmoHandle::None; }

        /** @brief Returns the handle being dragged. */
        [[nodiscard]] GizmoHandle getHandle() const { return handle_; }

        /** @brief Returns the entity being moved. */
        [[nodiscard]] const Uuid& getEntityId() const { return entityId_; }

        /**
         * @brief Starts a drag.
         *
         * @param scene The document the entity lives in.
         * @param camera The viewport camera, for the screen-to-world conversion.
         * @param entityId The entity to move.
         * @param handle The grabbed handle.
         * @param screenPoint Where the press landed.
         * @return False when @p entityId has no transform to move.
         */
        bool begin(const SceneDocument& scene,
                   const EditorCamera2D& camera,
                   const Uuid& entityId,
                   GizmoHandle handle,
                   const EditorVector2& screenPoint);

        /**
         * @brief Returns the entity's new local position for a cursor now at @p screenPoint.
         *
         * Axis-constrained by the grabbed handle. Returns std::nullopt when no drag is active.
         */
        [[nodiscard]] std::optional<EditorVector3> update(const SceneDocument& scene,
                                                          const EditorCamera2D& camera,
                                                          const EditorVector2& screenPoint) const;

        /** @brief Ends the drag. */
        void end() { handle_ = GizmoHandle::None; }

    private:
        GizmoHandle handle_ = GizmoHandle::None;
        Uuid entityId_;
        EditorVector2 grabWorld_;
        EditorVector3 startLocalPosition_;
    };
}
