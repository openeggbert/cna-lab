// SPDX-License-Identifier: MS-PL
#pragma once

/**
 * @file CNA/Editor/Panels/AssetBrowserPanel.hpp
 * @brief The project's assets, and the drag source that fills a reference slot.
 */

#include "CNA/Editor/Panels/EditorPanel.hpp"

namespace CNA::Editor
{
    /** @brief Lists the asset database and offers each row as a drag source. */
    class AssetBrowserPanel final : public EditorPanel
    {
    public:
        using EditorPanel::EditorPanel;

        void draw() override;

    private:
        /** @brief Payload type for an asset dragged out of the browser. */
        static constexpr const char* kAssetDragType = "asset";
    };
}
