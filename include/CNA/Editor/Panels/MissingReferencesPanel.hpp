// SPDX-License-Identifier: MS-PL
#pragma once

/**
 * @file CNA/Editor/Panels/MissingReferencesPanel.hpp
 * @brief The report of asset references the scene cannot resolve, and the way to fix them.
 */

#include "CNA/Editor/Panels/EditorPanel.hpp"

namespace CNA::Editor
{
    /**
     * @brief Lists every unresolvable asset reference and offers to relink or clear it.
     *
     * Grouped by asset id rather than by entity, because a broken reference is almost always one
     * asset that forty sprites point at -- and the fix is the same for all forty. Relinking is one
     * command, so it undoes in one press.
     *
     * A row is a drop target for the asset browser: dragging the right asset onto it is the
     * shortest path from "this is broken" to "this is fixed", and it reuses the drag-and-drop the
     * inspector's slots already use.
     */
    class MissingReferencesPanel final : public EditorPanel
    {
    public:
        using EditorPanel::EditorPanel;

        void draw() override;

    private:
        /** @brief Payload type for an asset dragged out of the browser. */
        static constexpr const char* kAssetDragType = "asset";
    };
}
