// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/Panels/DiagnosticsPanel.hpp"

#include <string>
#include <vector>

#include "CNA/Editor/EditorContext.hpp"

namespace CNA::Editor
{
    void DiagnosticsPanel::draw()
    {
        if (!ui_.beginPanel("Diagnostics", DockSide::Bottom)) { ui_.endPanel(); return; }

        ui_.text(std::string{"Editor UI: "} + ui_.getBackendName());
        ui_.text(std::string{"Viewport: "} + actions_.getViewport().getBackendName());
        ui_.separator();

        const std::vector<ViewportCapability> capabilities = actions_.getViewport().getBackendCapabilities();
        if (capabilities.empty())
        {
            // Said plainly. A blank section reads as "not implemented"; this is the honest state of
            // a headless run, where there is no device to ask.
            ui_.text("No graphics device, so no capabilities to report.");
        }
        else
        {
            ui_.text("Backend capabilities");
            for (const ViewportCapability& capability : capabilities)
            {
                ui_.text(std::string{"    "} + (capability.supported ? "yes  " : "no   ")
                         + capability.name);
            }
        }
        ui_.separator();

        // Because CNA fixes its backend at compile time, "run this on Vulkan" means "launch
        // cna-player-vulkan", and whether that binary exists is a question with a real answer. The
        // editor offers only what it found, and this is where a user sees why.
        const std::vector<PlayerBuild>& builds = actions_.getPlayerBuilds();
        ui_.text("Player builds found: " + std::to_string(builds.size()));
        for (const PlayerBuild& build : builds)
        {
            ui_.text("    " + build.backend + "  " + build.executablePath);
        }
        ui_.separator();

        ui_.text("Backends this editor knows about");
        for (const BackendInfo& backend : getKnownBackends())
        {
            const char* support = "runtime only";
            switch (backend.support)
            {
                case BackendEditorSupport::EditorSupported: support = "editor      "; break;
                case BackendEditorSupport::PreviewOnly: support = "preview only"; break;
                case BackendEditorSupport::RuntimeOnly: support = "runtime only"; break;
            }
            ui_.text(std::string{"    "} + support + "  " + backend.commandLineName + "  ("
                     + backend.displayName + ")");
        }

        ui_.endPanel();
    }
}
