// SPDX-License-Identifier: MS-PL
#pragma once

/**
 * @file CNA/Editor/Panels/ViewportPanel.hpp
 * @brief The scene preview, its play toolbar, and every pointer interaction over it.
 */

#include "CNA/Editor/Panels/EditorPanel.hpp"
#include <cstdint>
#include <optional>

#include "CNA/Editor/Scene/Tilemap.hpp"
#include "CNA/Editor/Scene/TransformGizmos.hpp"

namespace CNA::Editor
{
    /** @brief Draws the rendered scene, the play controls above it, and drives the gizmo. */
    class ViewportPanel final : public EditorPanel
    {
    public:
        using EditorPanel::EditorPanel;

        void draw() override;

    private:
        /** @brief Draws the play controls at the top of the panel. */
        void drawPlayToolbar();

        /**
         * @brief Turns one frame of pointer input into camera moves, selection, gizmos and tools.
         *
         * Split into the pieces below rather than written as one function, because the *ordering*
         * between them is load-bearing and was becoming impossible to see: a drag in progress
         * outranks everything, a tool outranks the gizmo, and the gizmo outranks the picker. Each
         * of those is a real rule with a reason, and each is now one line here.
         */
        void handleInteraction(const UiImageInteraction& interaction);

        /**
         * @brief Continues a gizmo drag or a paint stroke already in progress.
         *
         * @return True when one was active, in which case the pointer belongs to it and nothing
         *         else this frame may look at it. Deliberately independent of hover: a drag that
         *         wandered off the panel must keep going and end only on release, or the entity is
         *         dropped wherever the cursor happened to cross the edge.
         */
        bool updateActiveDrag(const UiImageInteraction& interaction, const EditorVector2& cursor);

        /** @brief Applies this frame's input to the active tool. */
        void applyToolInput(const UiImageInteraction& interaction, const EditorVector2& cursor);

        /** @brief Applies wheel zoom and drag panning, which every tool leaves alone. */
        void updateCamera(const UiImageInteraction& interaction, const EditorVector2& cursor);

        /**
         * @brief Starts a gizmo drag if @p cursor is over a handle of the selected entity's gizmo.
         *
         * Dispatches on the active mode: exactly one manipulator is on screen, so exactly one can
         * be grabbed, and each knows only about its own handles.
         *
         * @return True when a drag began, in which case the press must not also reach the picker.
         */
        bool beginGizmoDrag(const EditorVector2& cursor);

        /** @brief Returns true while any of the three manipulators is being dragged. */
        [[nodiscard]] bool isGizmoDragActive() const;

        /** @brief Ends whichever drag is in progress. */
        void endGizmoDrag();

        /** @brief Applies the in-progress drag to the entity, as one merged command per drag. */
        void updateGizmoDrag(const EditorVector2& cursor, const GizmoSnap& snap);

        /**
         * @brief Returns what a drag should round to this frame.
         *
         * Translation snaps to the *visible* grid rather than to a constant: the lines the user can
         * see are the ones they are lining things up with, and the two are the same function
         * (`chooseGridSpacing`) so a snapped entity always lands on a line that is drawn.
         */
        [[nodiscard]] GizmoSnap getSnap(const UiImageInteraction& interaction) const;

        /**
         * @brief Pushes @p value into the selected entity's transform, merging within one drag.
         *
         * Shared by all three manipulators because the undo rule is theirs jointly: the first edit
         * of a drag opens an entry and every later one folds into it, and a drag that has not
         * changed anything must push nothing at all -- an undo entry restoring a value the entity
         * already had costs the user an undo to reach a change they can actually see.
         */
        void commitGizmoEdit(const Uuid& entityId, const std::string& property,
                             const PropertyValue& value);

        /** @brief Draws the tool picker and the tile index the paint tool writes. */
        void drawToolbar();

        /** @brief Takes the tile under @p cursor as the brush and returns to painting. */
        void pickTileAt(const EditorVector2& cursor);

        /** @brief Fills the rectangle between the press cell and @p cursor, as one undo entry. */
        void fillTilesTo(const EditorVector2& cursor);

        /**
         * @brief Returns the tilemap cell under @p cursor, or nothing when there is none to paint.
         *
         * Shared by every tile tool, because "which cell is this, on which entity" is the same
         * question for all of them and answering it four times is four chances to differ.
         */
        [[nodiscard]] std::optional<TileCoordinate> tileUnder(const EditorVector2& cursor,
                                                              bool reportWhenMissing);

        /**
         * @brief Paints or erases the tile under @p cursor on the selected tilemap.
         *
         * @param startStroke True on the frame the button went down, which begins a new undo entry.
         *        Every frame after it merges into that entry, so one drag is one Ctrl+Z.
         */
        void paintTileAt(const EditorVector2& cursor, bool startStroke);

        /**
         * @brief Returns the world point the gizmo sits on, and whether it is a shared pivot.
         *
         * One entity selected: its own position. Several: their average, and the drag then moves
         * all of them about it.
         */
        [[nodiscard]] std::optional<EditorVector2> getGizmoPivot() const;

        /** @brief Applies an in-progress multi-selection drag as one command. */
        void updateMultiDrag(const EditorVector2& cursor, const GizmoSnap& snap);

        TranslateGizmoDrag translateDrag_;
        RotateGizmoDrag rotateDrag_;
        ScaleGizmoDrag scaleDrag_;

        /**
         * @brief The selection-wide half of a drag, active only when several entities are selected.
         *
         * Runs *beside* the single-entity drags rather than instead of them: those still compute
         * the gesture -- how far, how much, by what angle -- and this turns one gesture into the
         * edits a whole selection needs. One quantity, many entities, so they cannot drift apart.
         */
        MultiTransformDrag multiDrag_;

        /** @brief Distinguishes one multi-selection drag from the next, for merging. */
        std::uint64_t multiDragId_ = 0;

        /**
         * @brief Whether the current drag has already pushed a command.
         *
         * The first edit of a drag is a *new* undo entry and every later one merges into it.
         * Without this, a second drag of the same entity would merge into the first -- the merge
         * key is entity + component + property and has no notion of where one interaction ends --
         * and the two moves would undo together as if they had been one.
         */
        bool gizmoDragHasEdited_ = false;

        /**
         * @brief Which paint stroke is in progress, and whether it has pushed a command yet.
         *
         * The stroke id is part of the merge key, which is what keeps two separate drags from
         * collapsing into one undo entry -- the property alone cannot tell them apart.
         */
        std::uint64_t paintStroke_ = 0;
        bool paintStrokeHasEdited_ = false;

        /** @brief Where a fill drag began, while one is in progress. */
        std::optional<TileCoordinate> fillStart_;
    };
}
