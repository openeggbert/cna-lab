// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/Panels/AssetBrowserPanel.hpp"

#include <string>

#include "CNA/Editor/EditorContext.hpp"

namespace CNA::Editor
{
    void AssetBrowserPanel::draw()
    {
        if (!ui_.beginPanel("Assets", DockSide::Bottom)) { ui_.endPanel(); return; }

        const AssetDatabase& assets = context_.getAssets();
        ui_.text(std::to_string(assets.getCount()) + " assets");
        ui_.separator();

        for (const AssetRecord* record : assets.getAll())
        {
            // A tree leaf rather than a line of text: it carries the asset's own Uuid as its
            // widget identity, which is what lets the toolkit tell one row from another when a
            // drag starts on it.
            const std::string label = std::string{toString(record->type)} + "  " + record->sourcePath;
            ui_.treeNode(record->id, label, false, true);
            ui_.setDragSource(kAssetDragType, record->id.toString(), label);
        }

        ui_.endPanel();
    }
}
