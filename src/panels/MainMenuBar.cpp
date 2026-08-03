// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/Panels/MainMenuBar.hpp"

#include <string>

#include "CNA/Editor/EditorContext.hpp"

namespace CNA::Editor
{
    void MainMenuBar::draw()
    {
        if (ui_.beginMenu("File"))
        {
            if (ui_.menuItem("New Scene", "Ctrl+N")) { actions_.newScene(); }
            if (ui_.menuItem("Save Scene", "Ctrl+S", !context_.getScenePath().empty()))
            {
                actions_.saveScene();
            }
            // Only shown when there is something to recover. A permanently greyed-out pair of
            // items would teach users to ignore the one message that matters after a crash.
            if (const RecoverySnapshot* snapshot = actions_.getRecoverableScene())
            {
                ui_.separator();
                const std::string when = formatRecoveryTime(snapshot->savedAtSeconds);
                if (ui_.menuItem("Recover Unsaved Scene (" + when + ")")) { actions_.recoverScene(); }
                if (ui_.menuItem("Discard Recovered Scene")) { actions_.discardRecoveredScene(); }
            }

            ui_.separator();
            if (ui_.menuItem("Exit", "Alt+F4")) { /* handled by the UI implementation */ }
            ui_.endMenu();
        }

        if (ui_.beginMenu("Edit"))
        {
            CommandHistory& history = context_.getHistory();
            if (ui_.menuItem("Undo " + history.getUndoDescription(), "Ctrl+Z", history.canUndo()))
            {
                actions_.undo();
            }
            if (ui_.menuItem("Redo " + history.getRedoDescription(), "Ctrl+Y", history.canRedo()))
            {
                actions_.redo();
            }

            ui_.separator();

            const bool hasSelection = !context_.getSelection().empty();
            if (ui_.menuItem("Duplicate", "Ctrl+D", hasSelection)) { actions_.duplicateSelection(); }
            if (ui_.menuItem("Delete", "Del", hasSelection)) { actions_.deleteSelection(); }
            ui_.endMenu();
        }

        if (ui_.beginMenu("View"))
        {
            if (ui_.menuItem("Frame Selected", "F", !context_.getSelection().empty()))
            {
                actions_.frameSelection();
            }

            ui_.separator();

            if (ui_.menuItem("Translate Gizmo", "W")) { actions_.setGizmoMode(GizmoMode::Translate); }
            if (ui_.menuItem("Rotate Gizmo", "E")) { actions_.setGizmoMode(GizmoMode::Rotate); }
            if (ui_.menuItem("Scale Gizmo", "R")) { actions_.setGizmoMode(GizmoMode::Scale); }
            ui_.endMenu();
        }
    }
}
