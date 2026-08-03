// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/Panels/MissingReferencesPanel.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "CNA/Editor/EditorContext.hpp"
#include "CNA/Editor/Scene/MissingReferences.hpp"
#include "CNA/Editor/Scene/SceneCommands.hpp"

namespace CNA::Editor
{
    void MissingReferencesPanel::draw()
    {
        if (!ui_.beginPanel("Missing References", DockSide::Bottom)) { ui_.endPanel(); return; }

        const std::vector<MissingReference> missing =
            findMissingReferences(context_.getScene(), context_.getAssets());

        if (missing.empty())
        {
            // Said plainly rather than left blank. An empty panel reads as "not implemented yet",
            // which is exactly the wrong thing for a report whose good state is emptiness.
            ui_.text("No broken asset references.");
            ui_.endPanel();
            return;
        }

        ui_.text(std::to_string(missing.size()) + " broken reference(s)");
        ui_.separator();

        // Recorded and applied after the loop: relinking rewrites the very properties the report
        // was built from, so the vector above would describe a scene that no longer exists.
        Uuid relinkFrom;
        Uuid relinkTo;

        for (const Uuid& assetId : collectMissingAssetIds(missing))
        {
            std::size_t count = 0;
            const char* reason = "";
            std::string users;

            for (const MissingReference& reference : missing)
            {
                if (reference.assetId != assetId) { continue; }

                ++count;
                reason = toString(reference.reason);

                // Three names and a count, rather than forty names. The list exists to make the
                // breakage recognisable, not to enumerate it.
                if (count <= 3)
                {
                    if (!users.empty()) { users += ", "; }
                    users += reference.entityName + "." + reference.propertyName;
                }
            }

            if (count > 3) { users += " and " + std::to_string(count - 3) + " more"; }

            // A tree leaf rather than a line of text: it carries the broken asset's own id as its
            // widget identity, which is what a drop target needs to be told apart from its
            // neighbours.
            ui_.treeNode(assetId, assetId.toString() + "  (" + reason + ")", false, true);

            // Dragging the right asset from the browser onto the row is the shortest path from
            // "this is broken" to "this is fixed", and it reuses the drag the inspector already has.
            if (const std::optional<std::string> dropped = ui_.acceptDrop(kAssetDragType))
            {
                relinkFrom = assetId;
                relinkTo = Uuid::parse(*dropped);
            }

            ui_.text("    " + users);

            if (ui_.button("Clear##" + assetId.toString()))
            {
                relinkFrom = assetId;
                relinkTo = Uuid{};
            }

            ui_.separator();
        }

        ui_.endPanel();

        if (!relinkFrom.isValid()) { return; }

        auto command = std::make_unique<RelinkAssetCommand>(context_.getScene(), relinkFrom, relinkTo);
        if (!command->isValid()) { return; }

        const std::string summary = command->getDescription();
        context_.execute(std::move(command));
        context_.log(LogSeverity::Info, summary + ".");
    }
}
