// SPDX-License-Identifier: MS-PL
#pragma once

/**
 * @file CNA/Editor/Panels/ViewportPanel.hpp
 * @brief The scene preview, its play toolbar, and every pointer interaction over it.
 */

#include "CNA/Editor/Panels/EditorPanel.hpp"
#include <cstdint>

#include "CNA/Editor/Scene/TranslateGizmo.hpp"

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

        /** @brief Turns one frame of pointer input into camera moves, selection and gizmo drags. */
        void handleInteraction(const UiImageInteraction& interaction);

        /**
         * @brief Starts a gizmo drag if @p cursor is over a handle of the selected entity's gizmo.
         * @return True when a drag began, in which case the press must not also reach the picker.
         */
        bool beginGizmoDrag(const EditorVector2& cursor);

        /** @brief Applies the in-progress drag to the entity's position as one merged command. */
        void updateGizmoDrag(const EditorVector2& cursor);

        /** @brief Draws the tool picker and the tile index the paint tool writes. */
        void drawToolbar();

        /**
         * @brief Paints or erases the tile under @p cursor on the selected tilemap.
         *
         * @param startStroke True on the frame the button went down, which begins a new undo entry.
         *        Every frame after it merges into that entry, so one drag is one Ctrl+Z.
         */
        void paintTileAt(const EditorVector2& cursor, bool startStroke);

        TranslateGizmoDrag gizmoDrag_;

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
    };
}
