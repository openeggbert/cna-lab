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

            // Same rule as the space item below: named for what pressing it does, because a menu
            // has no checkmark here to carry the state. The toolbar is where the state is shown.
            if (ui_.menuItem(actions_.isThreeDimensionalView() ? "2D View" : "3D View"))
            {
                actions_.setThreeDimensionalView(!actions_.isThreeDimensionalView());
            }

            // Offered only where it does something. The 2D viewport has one plane and no choice to
            // make about it, and a menu item that changes nothing visible is a bug report waiting
            // to be filed.
            if (actions_.isThreeDimensionalView())
            {
                const bool ground = actions_.getGridPlane() == GridPlane::Ground;
                if (ui_.menuItem(ground ? "Grid on Scene Plane" : "Grid on Ground Plane"))
                {
                    actions_.setGridPlane(ground ? GridPlane::SceneXY : GridPlane::Ground);
                }
            }

            ui_.separator();

            if (ui_.menuItem("Translate Gizmo", "W")) { actions_.setGizmoMode(GizmoMode::Translate); }
            if (ui_.menuItem("Rotate Gizmo", "E")) { actions_.setGizmoMode(GizmoMode::Rotate); }
            if (ui_.menuItem("Scale Gizmo", "R")) { actions_.setGizmoMode(GizmoMode::Scale); }

            ui_.separator();

            // Named for what pressing it does rather than for the state it is in, since the menu
            // has no checkmark to carry the state.
            if (ui_.menuItem(actions_.getGizmoSpace() == GizmoSpace::World ? "Use Local Space"
                                                                          : "Use World Space",
                             "X"))
            {
                actions_.setGizmoSpace(actions_.getGizmoSpace() == GizmoSpace::World
                                           ? GizmoSpace::Local
                                           : GizmoSpace::World);
            }
            ui_.endMenu();
        }
    }
}
