// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/Panels/ViewportPanel.hpp"

#include <algorithm>
#include <array>

#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "CNA/Editor/EditorContext.hpp"
#include "CNA/Editor/Scene/BuiltinComponents.hpp"
#include "CNA/Editor/Scene/Tilemap.hpp"
#include "CNA/Editor/Scene/BuiltinComponents.hpp"
#include "CNA/Editor/Scene/SceneCommands.hpp"

namespace CNA::Editor
{
    const char* toString(EditorTool tool)
    {
        switch (tool)
        {
            case EditorTool::Select: return "Select";
            case EditorTool::PaintTiles: return "Paint Tiles";
            case EditorTool::EraseTiles: return "Erase Tiles";
            case EditorTool::PickTile: return "Pick Tile";
            case EditorTool::FillTiles: return "Fill Tiles";
        }
        return "Select";
    }

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
        drawToolbar();

        const UiRegion region = ui_.getContentRegion();
        if (region.isEmpty()) { ui_.endPanel(); return; }

        const int width = static_cast<int>(region.width);
        const int height = static_cast<int>(region.height);

        // The scene is rendered at exactly the panel's size. Rendering at a fixed size and
        // stretching would make the grid non-square and, worse, make picking disagree with what
        // is on screen.
        const UiTextureId texture =
            actions_.getViewport().render(context_.getScene(), width, height, context_.getSelection(),
                                          actions_.getGizmoMode(), actions_.getGizmoSpace(),
                                          actions_.getAnimationPreview());

        const UiImageInteraction interaction =
            ui_.image("##viewport", texture, region.width, region.height,
                       actions_.getViewport().isRenderTextureFlippedVertically());

        handleInteraction(interaction);

        ui_.endPanel();
    }

    void ViewportPanel::handleInteraction(const UiImageInteraction& interaction)
    {
        const EditorVector2 cursor{interaction.localMouseX, interaction.localMouseY};

        // Highest priority first, and each rule below is a real one with a reason.
        if (updateActiveDrag(interaction, cursor)) { return; }
        if (!interaction.hovered) { return; }

        // A tool outranks the gizmo. A tilemap's gizmo sits directly over its own first tiles, so
        // without this the opening press of every stroke would drag the map instead of painting it.
        const bool toolActive = actions_.getEditorTool() != EditorTool::Select;
        if (toolActive) { applyToolInput(interaction, cursor); }

        // The gizmo outranks the picker: a press on a handle is a manipulation, and must not also
        // count as a click that reselects whatever is underneath the gizmo.
        else if (interaction.leftPressed && beginGizmoDrag(cursor)) { return; }

        // Zoom and pan work whatever is active. A paint tool you cannot scroll is unusable.
        updateCamera(interaction, cursor);

        // Click-to-select does not, though: while a brush is active the first press would take
        // away the very tilemap being painted into.
        if (interaction.clicked && !toolActive)
        {
            const ScenePickResult pick =
                pickEntityAt(context_.getScene(), actions_.getViewport().getCamera(), cursor,
                             actions_.getViewport().makeSizeProvider());

            // Ctrl adds and removes rather than replacing, which is how a multi-selection is built
            // anywhere else. Ctrl on empty space does nothing at all: clearing a selection the user
            // is halfway through assembling is the one outcome they cannot have meant.
            if (interaction.control)
            {
                if (pick.entityId.isValid()) { context_.toggleSelection(pick.entityId); }
                return;
            }

            // Clicking empty space clears the selection, which is what every editor does and what
            // makes "deselect" reachable without a keyboard.
            context_.select(pick.entityId);
        }
    }

    bool ViewportPanel::updateActiveDrag(const UiImageInteraction& interaction, const EditorVector2& cursor)
    {
        if (isGizmoDragActive())
        {
            if (interaction.leftDown) { updateGizmoDrag(cursor, getSnap(interaction)); }
            else { endGizmoDrag(); }
            return true;
        }

        if (paintStrokeHasEdited_)
        {
            if (interaction.leftDown)
            {
                paintTileAt(cursor, false);
                return true;
            }
            paintStrokeHasEdited_ = false;
        }

        // A fill is drawn by dragging and applied on release, so the drag belongs to it until then
        // -- including the frames where the cursor has left the panel, since the release still
        // decides the rectangle.
        if (fillStart_)
        {
            if (interaction.leftDown) { return true; }

            fillTilesTo(cursor);
            fillStart_.reset();
            return true;
        }

        return false;
    }

    void ViewportPanel::applyToolInput(const UiImageInteraction& interaction, const EditorVector2& cursor)
    {
        switch (actions_.getEditorTool())
        {
            case EditorTool::PaintTiles:
            case EditorTool::EraseTiles:
                if (interaction.leftPressed) { paintTileAt(cursor, true); }
                else if (interaction.leftDown) { paintTileAt(cursor, false); }
                return;

            case EditorTool::PickTile:
                if (interaction.leftPressed) { pickTileAt(cursor); }
                return;

            case EditorTool::FillTiles:
                if (interaction.leftPressed) { fillStart_ = tileUnder(cursor, true); }
                return;

            case EditorTool::Select:
                return;
        }
    }

    void ViewportPanel::updateCamera(const UiImageInteraction& interaction, const EditorVector2& cursor)
    {
        EditorCamera2D& camera = actions_.getViewport().getCamera();

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
    }

    void ViewportPanel::drawToolbar()
    {
        static const std::array<EditorTool, 5> kOrder{EditorTool::Select, EditorTool::PaintTiles,
                                                      EditorTool::EraseTiles, EditorTool::PickTile,
                                                      EditorTool::FillTiles};

        std::vector<std::string> names;
        names.reserve(kOrder.size());
        for (const EditorTool tool : kOrder) { names.emplace_back(toString(tool)); }

        ui_.setNextItemWidth(130.0f);
        PropertyValue chosen{PropertyValue::EnumValue{toString(actions_.getEditorTool())}};
        if (ui_.propertyField("##tool", chosen, names))
        {
            const std::string name = chosen.get<PropertyValue::EnumValue>().name;
            for (std::size_t index = 0; index < kOrder.size(); ++index)
            {
                if (names[index] == name) { actions_.setEditorTool(kOrder[index]); }
            }
        }

        // Only where it means something. A tile index beside the Select tool is a control that
        // does nothing, which is worse than one that is not there. The eraser has no index either;
        // the eyedropper sets one rather than reading it.
        const EditorTool active = actions_.getEditorTool();
        if (active != EditorTool::PaintTiles && active != EditorTool::FillTiles) { return; }

        ui_.sameLine();
        ui_.setNextItemWidth(90.0f);
        PropertyValue tile{actions_.getPaintTile()};
        if (ui_.propertyField("Tile", tile))
        {
            actions_.setPaintTile(std::max<std::int64_t>(0, tile.get<std::int64_t>(0)));
        }
    }

    std::optional<TileCoordinate> ViewportPanel::tileUnder(const EditorVector2& cursor,
                                                           bool reportWhenMissing)
    {
        const Uuid selectedId = context_.getPrimarySelection();
        const EditorEntity* entity = context_.getScene().findEntity(selectedId);
        const EditorComponent* tilemap =
            entity != nullptr ? entity->findComponent(BuiltinComponentIds::kTilemap) : nullptr;

        if (tilemap == nullptr)
        {
            // Said once per press rather than per frame: a brush over a sprite is a near miss, and
            // sixty lines a second about it is how a console stops being read.
            if (reportWhenMissing)
            {
                context_.log(LogSeverity::Warning,
                             "Select an entity with a Tilemap component to paint into.");
            }
            return std::nullopt;
        }

        const std::optional<WorldTransform> transform =
            computeWorldTransform(context_.getScene(), selectedId);
        if (!transform) { return std::nullopt; }

        const ComponentDescriptor* descriptor =
            context_.getComponentRegistry().find(BuiltinComponentIds::kTilemap);

        const EditorVector2 world = actions_.getViewport().getCamera().screenToWorld(cursor);
        return worldToTile(
            *transform,
            static_cast<int>(tilemap->getPropertyOrDefault(TilemapKeys::kTileWidth, descriptor)
                                 .get<std::int64_t>(0)),
            static_cast<int>(tilemap->getPropertyOrDefault(TilemapKeys::kTileHeight, descriptor)
                                 .get<std::int64_t>(0)),
            world);
    }

    void ViewportPanel::paintTileAt(const EditorVector2& cursor, bool startStroke)
    {
        const std::optional<TileCoordinate> cell = tileUnder(cursor, startStroke);
        if (!cell) { return; }

        if (startStroke)
        {
            ++paintStroke_;
            paintStrokeHasEdited_ = false;
        }

        const std::int64_t value =
            actions_.getEditorTool() == EditorTool::EraseTiles ? kEmptyTile : actions_.getPaintTile();

        auto command = std::make_unique<PaintTilesCommand>(context_.getScene(),
                                                           context_.getComponentRegistry(),
                                                           context_.getPrimarySelection(), paintStroke_);
        if (!command->paint(cell->x, cell->y, value)) { return; }

        // The first cell of a stroke opens a new entry and every later one merges into it, which is
        // what makes a drag across forty tiles one Ctrl+Z.
        const MergePolicy policy =
            paintStrokeHasEdited_ ? MergePolicy::MergeWithPrevious : MergePolicy::NewEntry;
        paintStrokeHasEdited_ = true;
        context_.execute(std::move(command), policy);
    }

    void ViewportPanel::pickTileAt(const EditorVector2& cursor)
    {
        const std::optional<TileCoordinate> cell = tileUnder(cursor, true);
        if (!cell) { return; }

        const Uuid selectedId = context_.getPrimarySelection();
        const EditorComponent* tilemap =
            context_.getScene().findEntity(selectedId)->findComponent(BuiltinComponentIds::kTilemap);

        const TilemapGrid grid = readTilemapGrid(
            *tilemap, context_.getComponentRegistry().find(BuiltinComponentIds::kTilemap));

        const std::int64_t picked = grid.at(cell->x, cell->y);
        if (picked < 0)
        {
            // An empty cell is not a tile. Taking -1 as the brush would silently turn the
            // eyedropper into an eraser, which is a different tool the user did not choose.
            context_.log(LogSeverity::Info, "That cell is empty; the brush was left alone.");
            return;
        }

        actions_.setPaintTile(picked);

        // Straight back to painting, which is what every editor does and what makes the eyedropper
        // worth reaching for: picking a tile is never the goal, painting with it is.
        actions_.setEditorTool(EditorTool::PaintTiles);
        context_.log(LogSeverity::Info, "Brush set to tile " + std::to_string(picked) + ".");
    }

    void ViewportPanel::fillTilesTo(const EditorVector2& cursor)
    {
        if (!fillStart_) { return; }

        const std::optional<TileCoordinate> end = tileUnder(cursor, false);
        if (!end) { return; }

        const int minX = std::min(fillStart_->x, end->x);
        const int maxX = std::max(fillStart_->x, end->x);
        const int minY = std::min(fillStart_->y, end->y);
        const int maxY = std::max(fillStart_->y, end->y);

        ++paintStroke_;

        auto command = std::make_unique<PaintTilesCommand>(context_.getScene(),
                                                           context_.getComponentRegistry(),
                                                           context_.getPrimarySelection(), paintStroke_);

        // Every cell into one command, so a fill is one undo entry however large the rectangle.
        // Cells outside the map are refused by the command itself, so dragging past the edge fills
        // what exists rather than nothing.
        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x) { command->paint(x, y, actions_.getPaintTile()); }
        }

        if (!command->isValid()) { return; }

        const std::size_t cells = command->getCells().size();
        context_.execute(std::move(command));
        context_.log(LogSeverity::Info, "Filled " + std::to_string(cells) + " tile(s).");
    }

    bool ViewportPanel::isGizmoDragActive() const
    {
        return translateDrag_.isActive() || rotateDrag_.isActive() || scaleDrag_.isActive();
    }

    void ViewportPanel::endGizmoDrag()
    {
        translateDrag_.end();
        rotateDrag_.end();
        scaleDrag_.end();
        multiDrag_.end();
    }

    std::optional<EditorVector2> ViewportPanel::getGizmoPivot() const
    {
        const std::vector<Uuid>& selection = context_.getSelection();
        if (selection.empty()) { return std::nullopt; }
        if (selection.size() == 1)
        {
            const std::optional<WorldTransform> world = computeWorldTransform(context_.getScene(), selection.front());
            if (!world) { return std::nullopt; }
            return EditorVector2{world->position.x, world->position.y};
        }
        return computeSelectionPivot(context_.getScene(), selection);
    }

    bool ViewportPanel::beginGizmoDrag(const EditorVector2& cursor)
    {
        const Uuid selectedId = context_.getPrimarySelection();
        const SceneDocument& scene = context_.getScene();
        const EditorCamera2D& camera = actions_.getViewport().getCamera();

        // The shared pivot, when several entities are selected. Where the gizmo is *drawn* is where
        // it must be grabbed, and the renderer places it the same way.
        const std::optional<EditorVector2> pivot =
            context_.getSelection().size() > 1 ? getGizmoPivot() : std::nullopt;

        // Only the manipulator that is actually drawn can be grabbed. Hit-testing the others would
        // let a press land on a handle nobody can see, which is indistinguishable from a bug.
        switch (actions_.getGizmoMode())
        {
            case GizmoMode::Translate:
            {
                auto layout =
                    computeTranslateGizmoLayout(scene, camera, selectedId, actions_.getGizmoSpace());
                if (!layout) { return false; }
                if (pivot) { placeGizmoAt(*layout, camera, *pivot); }

                const GizmoHandle handle = hitTestTranslateGizmo(*layout, cursor);
                if (!translateDrag_.begin(scene, camera, selectedId, handle, cursor,
                                          actions_.getGizmoSpace()))
                {
                    return false;
                }
                break;
            }

            case GizmoMode::Rotate:
            {
                auto layout = computeRotateGizmoLayout(scene, camera, selectedId);
                if (!layout) { return false; }
                if (pivot) { placeGizmoAt(*layout, camera, *pivot); }

                if (hitTestRotateGizmo(*layout, cursor) == GizmoHandle::None) { return false; }
                if (!rotateDrag_.begin(scene, *layout, selectedId, cursor)) { return false; }
                break;
            }

            case GizmoMode::Scale:
            {
                auto layout = computeScaleGizmoLayout(scene, camera, selectedId);
                if (!layout) { return false; }
                if (pivot) { placeGizmoAt(*layout, camera, *pivot); }

                const GizmoHandle handle = hitTestScaleGizmo(*layout, cursor);
                if (!scaleDrag_.begin(scene, *layout, selectedId, handle, cursor)) { return false; }
                break;
            }

            case GizmoMode::None:
                return false;
        }

        gizmoDragHasEdited_ = false;

        // A multi-selection drag runs beside the single-entity one: that computes the gesture, this
        // turns it into the edits a whole selection needs. Started only when there is more than one
        // entity, so a single selection takes exactly the path it always did.
        multiDrag_.end();
        if (context_.getSelection().size() > 1)
        {
            if (const std::optional<EditorVector2> pivot = getGizmoPivot())
            {
                multiDrag_.begin(scene, context_.getSelection(), *pivot);
                ++multiDragId_;
            }
        }
        return true;
    }

    void ViewportPanel::commitGizmoEdit(const Uuid& entityId, const std::string& property,
                                        const PropertyValue& value)
    {
        const EditorEntity* entity = context_.getScene().findEntity(entityId);
        if (entity == nullptr) { return; }

        const EditorComponent* transform = entity->findComponent(BuiltinComponentIds::kTransform);
        if (transform == nullptr) { return; }

        // A drag that has not moved yet must not push anything: a press and release on a handle is
        // not an edit, and an undo entry that restores the value it already had is worse than no
        // entry -- it costs the user an undo to get back to a change they can actually see.
        if (transform->getProperty(property) == value) { return; }

        // The first edit opens a new undo entry; every later one folds into it. The result is one
        // entry per drag that undoes to where the drag started -- not one per mouse-move event, and
        // not one shared with the previous drag of the same entity.
        context_.execute(std::make_unique<SetPropertyCommand>(context_.getScene(), entityId,
                                                              BuiltinComponentIds::kTransform,
                                                              property, value),
                         gizmoDragHasEdited_ ? MergePolicy::MergeWithPrevious : MergePolicy::NewEntry);
        gizmoDragHasEdited_ = true;
    }

    GizmoSnap ViewportPanel::getSnap(const UiImageInteraction& interaction) const
    {
        if (!interaction.control) { return {}; }

        GizmoSnap snap;
        snap.translate = chooseGridSpacing(actions_.getViewport().getCamera().getZoom(),
                                           kGridTargetPixels);
        snap.rotate = kDefaultRotationSnap;
        snap.scale = kDefaultScaleSnap;
        return snap;
    }

    void ViewportPanel::updateMultiDrag(const EditorVector2& cursor, const GizmoSnap& snap)
    {
        const SceneDocument& scene = context_.getScene();
        const EditorCamera2D& camera = actions_.getViewport().getCamera();

        std::vector<EntityTransformEdit> edits;

        if (translateDrag_.isActive())
        {
            edits = multiDrag_.translate(scene, translateDrag_.getWorldDelta(camera, cursor, snap));
        }
        else if (rotateDrag_.isActive())
        {
            auto layout = computeRotateGizmoLayout(scene, camera, rotateDrag_.getEntityId());
            if (!layout) { return; }

            // The pivot captured when the drag began, not a fresh centroid: the entities are moving
            // as the drag proceeds, and a centre recomputed from them would chase itself.
            placeGizmoAt(*layout, camera, multiDrag_.getPivot());
            edits = multiDrag_.rotate(scene, rotateDrag_.getDeltaAngle(*layout, cursor, snap));
        }
        else if (scaleDrag_.isActive())
        {
            auto layout = computeScaleGizmoLayout(scene, camera, scaleDrag_.getEntityId());
            if (!layout) { return; }
            placeGizmoAt(*layout, camera, multiDrag_.getPivot());

            const float factor = scaleDrag_.getFactor(*layout, cursor, snap);
            const GizmoHandle handle = scaleDrag_.getHandle();
            edits = multiDrag_.scale(scene,
                                     EditorVector2{handle == GizmoHandle::YAxis ? 1.0f : factor,
                                                   handle == GizmoHandle::XAxis ? 1.0f : factor});
        }

        if (edits.empty()) { return; }

        // One command for the whole selection, and one undo entry for the whole drag. A command per
        // entity would make undoing one drag several presses of Ctrl+Z -- and would undo them one
        // at a time, through arrangements the scene was never in.
        auto command = std::make_unique<TransformEntitiesCommand>(
            context_.getScene(), std::move(edits),
            "transform-many:" + std::to_string(multiDragId_));

        context_.execute(std::move(command),
                         gizmoDragHasEdited_ ? MergePolicy::MergeWithPrevious : MergePolicy::NewEntry);
        gizmoDragHasEdited_ = true;
    }

    void ViewportPanel::updateGizmoDrag(const EditorVector2& cursor, const GizmoSnap& snap)
    {
        if (multiDrag_.isActive())
        {
            updateMultiDrag(cursor, snap);
            return;
        }

        const SceneDocument& scene = context_.getScene();
        const EditorCamera2D& camera = actions_.getViewport().getCamera();

        if (translateDrag_.isActive())
        {
            if (const auto position = translateDrag_.update(scene, camera, cursor, snap))
            {
                commitGizmoEdit(translateDrag_.getEntityId(), "position", PropertyValue{*position});
            }
            return;
        }

        if (rotateDrag_.isActive())
        {
            // The layout is recomputed each frame rather than kept from the press, so the ring
            // stays under the entity if something else moves it -- a parent being animated by the
            // running player, say. The drag's own start angle is what must not be recomputed, and
            // that lives in the drag.
            const auto layout = computeRotateGizmoLayout(scene, camera, rotateDrag_.getEntityId());
            if (!layout) { return; }

            if (const auto rotation = rotateDrag_.update(*layout, cursor, snap))
            {
                commitGizmoEdit(rotateDrag_.getEntityId(), "rotation", PropertyValue{*rotation});
            }
            return;
        }

        if (scaleDrag_.isActive())
        {
            const auto layout = computeScaleGizmoLayout(scene, camera, scaleDrag_.getEntityId());
            if (!layout) { return; }

            if (const auto scale = scaleDrag_.update(*layout, cursor, snap))
            {
                commitGizmoEdit(scaleDrag_.getEntityId(), "scale", PropertyValue{*scale});
            }
        }
    }
}
