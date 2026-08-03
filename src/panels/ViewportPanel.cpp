// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/Panels/ViewportPanel.hpp"

#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "CNA/Editor/EditorContext.hpp"
#include "CNA/Editor/Scene/BuiltinComponents.hpp"
#include "CNA/Editor/Scene/SceneCommands.hpp"

namespace CNA::Editor
{
    void ViewportPanel::drawPlayToolbar()
    {
        if (actions_.getPlayMode() == PlayMode::Stopped)
        {
            if (ui_.button("Play")) { actions_.startPlay(); }
        }
        else
        {
            if (ui_.button("Stop")) { actions_.stopPlay(); }
            ui_.sameLine();
            if (ui_.button(actions_.getPlayMode() == PlayMode::Paused ? "Resume" : "Pause"))
            {
                actions_.setPlayPaused(actions_.getPlayMode() != PlayMode::Paused);
            }
            ui_.sameLine();
            if (ui_.button("Step")) { actions_.stepPlayFrame(); }
        }

        ui_.sameLine();

        if (actions_.getPlayerBuilds().empty())
        {
            ui_.text("no player build found");
        }
        else if (actions_.getPlayMode() != PlayMode::Stopped)
        {
            ui_.text("running on "
                     + actions_.getPlayerBuilds()[actions_.getSelectedPlayerBuild()].backend
                     + (actions_.getPlayMode() == PlayMode::Paused ? " (paused)" : ""));
        }
        else
        {
            // The choice is an enum over what is installed, not over the fourteen backends CNA
            // knows about: offering a backend with no player binary would be offering a button
            // that cannot work.
            std::vector<std::string> names;
            names.reserve(actions_.getPlayerBuilds().size());
            for (const PlayerBuild& build : actions_.getPlayerBuilds()) { names.push_back(build.backend); }

            // Wide enough for a backend name, narrow enough to leave the toolbar a toolbar.
            ui_.setNextItemWidth(150.0f);

            PropertyValue value{PropertyValue::EnumValue{names[actions_.getSelectedPlayerBuild()]}};
            if (ui_.propertyField("Backend", value, names))
            {
                const std::string chosen = value.get<PropertyValue::EnumValue>().name;
                for (std::size_t index = 0; index < names.size(); ++index)
                {
                    if (names[index] == chosen) { actions_.selectPlayerBuild(index); break; }
                }
            }
        }

        ui_.separator();
    }
    void ViewportPanel::draw()
    {
        if (!ui_.beginPanel("Viewport", DockSide::Center)) { ui_.endPanel(); return; }

        // Above the image, so the scene is rendered into whatever is left. Putting the controls in
        // their own panel would let the user dock them away from the thing they control.
        drawPlayToolbar();

        const UiRegion region = ui_.getContentRegion();
        if (region.isEmpty()) { ui_.endPanel(); return; }

        const int width = static_cast<int>(region.width);
        const int height = static_cast<int>(region.height);

        // The scene is rendered at exactly the panel's size. Rendering at a fixed size and
        // stretching would make the grid non-square and, worse, make picking disagree with what
        // is on screen.
        const UiTextureId texture =
            actions_.getViewport().render(context_.getScene(), width, height, context_.getSelection(), actions_.getGizmoMode());

        const UiImageInteraction interaction =
            ui_.image("##viewport", texture, region.width, region.height,
                       actions_.getViewport().isRenderTextureFlippedVertically());

        handleInteraction(interaction);

        ui_.endPanel();
    }

    void ViewportPanel::handleInteraction(const UiImageInteraction& interaction)
    {
        EditorCamera2D& camera = actions_.getViewport().getCamera();
        const EditorVector2 cursor{interaction.localMouseX, interaction.localMouseY};

        // A drag in progress owns the pointer, hovered or not. Ending it because the cursor left
        // the panel would drop the entity wherever it happened to cross the edge, and would leave
        // the user holding a button that no longer does anything.
        if (gizmoDrag_.isActive())
        {
            if (interaction.leftDown) { updateGizmoDrag(cursor); }
            else { gizmoDrag_.end(); }
            return;
        }

        if (!interaction.hovered) { return; }

        // Checked before the camera and the picker: a press on a handle is a manipulation, and
        // must not also count as a click that reselects whatever is underneath the gizmo.
        if (interaction.leftPressed && beginGizmoDrag(cursor)) { return; }

        if (interaction.wheel != 0.0f)
        {
            // A constant factor per notch gives geometric zoom, so each notch feels the same
            // whatever the current scale -- a linear step is unusable at both ends of the range.
            constexpr float kZoomPerNotch = 1.15f;
            camera.zoomAt(cursor, std::pow(kZoomPerNotch, interaction.wheel));
        }

        if (interaction.dragging)
        {
            camera.panByScreenDelta(EditorVector2{interaction.dragDeltaX, interaction.dragDeltaY});
        }

        if (interaction.clicked)
        {
            const ScenePickResult pick =
                pickEntityAt(context_.getScene(), camera, cursor, actions_.getViewport().makeSizeProvider());

            // Clicking empty space clears the selection, which is what every editor does and what
            // makes "deselect" reachable without a keyboard.
            context_.select(pick.entityId);
        }
    }

    bool ViewportPanel::beginGizmoDrag(const EditorVector2& cursor)
    {
        if (actions_.getGizmoMode() != GizmoMode::Translate) { return false; }

        const Uuid selectedId = context_.getPrimarySelection();
        const std::optional<TranslateGizmoLayout> layout =
            computeTranslateGizmoLayout(context_.getScene(), actions_.getViewport().getCamera(), selectedId);
        if (!layout) { return false; }

        const GizmoHandle handle = hitTestTranslateGizmo(*layout, cursor);
        if (handle == GizmoHandle::None) { return false; }

        if (!gizmoDrag_.begin(context_.getScene(), actions_.getViewport().getCamera(), selectedId, handle, cursor))
        {
            return false;
        }

        gizmoDragHasEdited_ = false;
        return true;
    }

    void ViewportPanel::updateGizmoDrag(const EditorVector2& cursor)
    {
        const std::optional<EditorVector3> position =
            gizmoDrag_.update(context_.getScene(), actions_.getViewport().getCamera(), cursor);
        if (!position) { return; }

        // A drag that has not moved yet must not push anything: a press and release on a handle is
        // not an edit, and an undo entry that restores the position it already had is worse than
        // no entry -- it costs the user an undo to get back to a change they can actually see.
        const EditorEntity* entity = context_.getScene().findEntity(gizmoDrag_.getEntityId());
        if (entity == nullptr) { return; }
        const EditorComponent* transform = entity->findComponent(BuiltinComponentIds::kTransform);
        if (transform == nullptr) { return; }
        if (transform->getProperty("position").get<EditorVector3>() == *position) { return; }

        // The first edit opens a new undo entry; every later one folds into it. The result is one
        // entry per drag that undoes to where the drag started -- not one per mouse-move event, and
        // not one shared with the previous drag of the same entity.
        context_.execute(std::make_unique<SetPropertyCommand>(
                             context_.getScene(), gizmoDrag_.getEntityId(),
                             BuiltinComponentIds::kTransform, "position", PropertyValue{*position}),
                         gizmoDragHasEdited_ ? MergePolicy::MergeWithPrevious : MergePolicy::NewEntry);
        gizmoDragHasEdited_ = true;
    }
}
