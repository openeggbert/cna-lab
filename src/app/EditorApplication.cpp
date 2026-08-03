// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/EditorApplication.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <optional>

#include "CNA/Editor/Scene/BuiltinComponents.hpp"
#include "CNA/Editor/Scene/SceneCommands.hpp"

namespace CNA::Editor
{
    namespace
    {
        /** @brief Splits "--name=value" into its parts. Returns false when there is no '='. */
        bool splitOption(std::string_view argument, std::string& name, std::string& value)
        {
            const std::size_t equals = argument.find('=');
            if (equals == std::string_view::npos) { return false; }
            name = std::string{argument.substr(0, equals)};
            value = std::string{argument.substr(equals + 1)};
            return true;
        }
    }

    EditorOptions EditorOptions::parse(int argc, const char* const* argv)
    {
        EditorOptions options;

        for (int index = 1; index < argc; ++index)
        {
            const std::string_view argument{argv[index] != nullptr ? argv[index] : ""};
            if (argument.empty()) { continue; }

            std::string name;
            std::string value;

            if (argument == "--help" || argument == "-h") { options.showHelp = true; continue; }
            if (argument == "--version") { options.showVersion = true; continue; }
            if (argument == "--headless") { options.headless = true; continue; }
            if (argument == "--list-backends") { options.listBackends = true; continue; }

            if (splitOption(argument, name, value))
            {
                if (name == "--project") { options.projectPath = value; continue; }
                if (name == "--scene") { options.scenePath = value; continue; }
                if (name == "--ui") { options.uiBackend = value; continue; }
                if (name == "--screenshot") { options.screenshotPath = value; continue; }
                if (name == "--frames")
                {
                    try { options.frameLimit = std::stoi(value); }
                    catch (const std::exception&)
                    {
                        options.hasError = true;
                        options.errorMessage = "--frames expects a number, got '" + value + "'";
                    }
                    continue;
                }
                if (name == "--graphics")
                {
                    // Rejected rather than ignored, because silently accepting it would teach
                    // users a mental model CNA does not support (see EditorOptions::uiBackend).
                    options.hasError = true;
                    options.errorMessage =
                        "--graphics is not an editor option: CNA selects its graphics backend at "
                        "compile time, so this editor binary is fixed to the backend it was built "
                        "against. Pass --graphics to cna-player instead, or use --ui to choose the "
                        "editor's UI toolkit.";
                    continue;
                }
            }

            if (argument.rfind("--", 0) == 0)
            {
                options.hasError = true;
                options.errorMessage = "unknown option '" + std::string{argument} + "'";
                continue;
            }

            // A bare path is the project to open, which is what a file manager passes.
            if (options.projectPath.empty()) { options.projectPath = std::string{argument}; }
        }

        return options;
    }

    std::string EditorOptions::getUsage()
    {
        return
            "cna-editor -- editor and asset tooling for CNA\n"
            "\n"
            "Usage:\n"
            "  cna-editor [options] [project.cnaproject]\n"
            "\n"
            "Options:\n"
            "  --project=PATH     Open this .cnaproject at start-up.\n"
            "  --scene=PATH       Open this .cnascene, overriding the project's startup scene.\n"
            "  --ui=NAME          UI toolkit to use: 'imgui' or 'null'. Default: imgui.\n"
            "  --headless         Run with no window, on the null UI. Implies --ui=null.\n"
            "  --frames=N         Exit after N frames. Useful for smoke tests.\n"
            "  --screenshot=PATH  Write a PNG of the final frame. Requires --frames.\n"
            "  --list-backends    Print the CNA graphics backends this editor knows about.\n"
            "  --version          Print the version and exit.\n"
            "  -h, --help         Print this help and exit.\n"
            "\n"
            "Note: there is no --graphics option. CNA selects its graphics backend at compile\n"
            "time, so this editor binary is fixed to the backend it was built against. To preview\n"
            "a game on a different backend, launch the matching cna-player build:\n"
            "\n"
            "  cna-player --project=MyGame.cnaproject --graphics=software --editor-port=34781\n";
    }

    EditorApplication::EditorApplication(std::unique_ptr<EditorUi> ui, std::unique_ptr<EditorViewport> viewport)
        : ui_(std::move(ui)), viewport_(std::move(viewport))
    {
        // Forward every context message into the console panel, so a single log() call reaches
        // both the UI and, through the UI implementation, stdout in headless runs.
        context_.setLogSink([this](LogSeverity severity, const std::string& message) {
            ui_->log(severity, message);
        });
    }

    void EditorApplication::setViewport(std::unique_ptr<EditorViewport> viewport)
    {
        if (!viewport) { return; }

        // Carry the camera across, so installing a real viewport does not throw away wherever the
        // user had already navigated to.
        const EditorCamera2D previousCamera = viewport_ ? viewport_->getCamera() : EditorCamera2D{};
        viewport_ = std::move(viewport);
        viewport_->getCamera() = previousCamera;

        context_.log(LogSeverity::Info,
                     std::string{"Viewport: "} + viewport_->getBackendName());
    }

    bool EditorApplication::initialize(const EditorOptions& options)
    {
        frameLimit_ = options.frameLimit;

        context_.log(LogSeverity::Info,
                     std::string{"cna-editor starting (ui="} + ui_->getBackendName()
                         + ", viewport=" + viewport_->getBackendName() + ")");

        if (!options.projectPath.empty())
        {
            if (!context_.openProject(options.projectPath)) { return false; }
        }
        else
        {
            context_.newScene("Untitled");
        }

        if (!options.scenePath.empty())
        {
            if (!context_.openScene(options.scenePath)) { return false; }
        }

        return true;
    }

    int EditorApplication::run()
    {
        while (ui_->beginFrame())
        {
            renderFrame();
            ui_->endFrame();

            ++framesRendered_;
            if (frameLimit_ > 0 && framesRendered_ >= frameLimit_) { break; }
        }
        return 0;
    }

    void EditorApplication::renderFrame()
    {
        handleShortcuts();

        ui_->beginDockSpace();

        drawMainMenu();
        drawSceneHierarchyPanel();
        drawViewportPanel();
        drawInspectorPanel();
        drawAssetBrowserPanel();
        drawConsolePanel();

        ui_->endDockSpace();
    }

    void EditorApplication::handleShortcuts()
    {
        // Ctrl-chords first. Each is exact-modifier matched by the UI layer, so Ctrl+Shift+Z does
        // not also fire the undo bound to Ctrl+Z.
        if (ui_->isShortcutPressed(UiKey::Z, withControl())) { undo(); }
        if (ui_->isShortcutPressed(UiKey::Y, withControl())) { redo(); }
        if (ui_->isShortcutPressed(UiKey::N, withControl())) { context_.newScene(); }
        if (ui_->isShortcutPressed(UiKey::S, withControl()))
        {
            if (!context_.getScenePath().empty()) { context_.saveScene(); }
        }
        if (ui_->isShortcutPressed(UiKey::D, withControl())) { duplicateSelection(); }

        if (ui_->isShortcutPressed(UiKey::Delete)) { deleteSelection(); }
        if (ui_->isShortcutPressed(UiKey::F)) { frameSelection(); }

        if (ui_->isShortcutPressed(UiKey::W)) { setGizmoMode(GizmoMode::Translate); }
        if (ui_->isShortcutPressed(UiKey::E)) { setGizmoMode(GizmoMode::Rotate); }
        if (ui_->isShortcutPressed(UiKey::R)) { setGizmoMode(GizmoMode::Scale); }
    }

    void EditorApplication::undo()
    {
        context_.getHistory().undo();
        context_.pruneSelection();
    }

    void EditorApplication::redo()
    {
        context_.getHistory().redo();
        context_.pruneSelection();
    }

    void EditorApplication::duplicateSelection()
    {
        // Snapshot the selection: the copies are selected as they are made, so iterating the live
        // selection would duplicate the copies as well.
        const std::vector<Uuid> sources = context_.getSelection();
        std::vector<Uuid> copies;

        for (const Uuid& sourceId : sources)
        {
            auto command = std::make_unique<DuplicateEntityCommand>(context_.getScene(), sourceId);
            if (!command->isValid()) { continue; }

            copies.push_back(command->getEntityId());
            context_.execute(std::move(command));
        }

        // Selecting the copies is what makes "duplicate, then drag it somewhere" work without a
        // trip back to the hierarchy panel.
        if (!copies.empty()) { context_.setSelection(std::move(copies)); }
    }

    void EditorApplication::deleteSelection()
    {
        for (const Uuid& entityId : context_.getSelection())
        {
            // A selected descendant of another selected entity is already gone by now, and asking
            // to delete it again would add an undo entry that does nothing.
            if (context_.getScene().findEntity(entityId) == nullptr) { continue; }
            context_.execute(std::make_unique<DeleteEntityCommand>(context_.getScene(), entityId));
        }
        context_.pruneSelection();
    }

    void EditorApplication::frameSelection()
    {
        const std::vector<Uuid>& selection = context_.getSelection();
        if (selection.empty()) { return; }

        const SpriteSizeProvider sizeProvider = viewport_->makeSizeProvider();
        std::optional<WorldBounds2D> total;

        for (const Uuid& entityId : selection)
        {
            std::optional<WorldBounds2D> bounds =
                computeHierarchyBounds2D(context_.getScene(), entityId, sizeProvider);

            if (!bounds)
            {
                // An entity with no drawable geometry -- a camera, an empty grouping node -- still
                // has a position, and framing it should centre on it rather than do nothing.
                const std::optional<WorldTransform> world =
                    computeWorldTransform(context_.getScene(), entityId);
                if (!world) { continue; }

                const EditorVector2 point{world->position.x, world->position.y};
                bounds = WorldBounds2D{point, point};
            }

            if (!total) { total = bounds; continue; }

            total->min.x = std::min(total->min.x, bounds->min.x);
            total->min.y = std::min(total->min.y, bounds->min.y);
            total->max.x = std::max(total->max.x, bounds->max.x);
            total->max.y = std::max(total->max.y, bounds->max.y);
        }

        if (total) { viewport_->getCamera().frame(*total); }
    }

    void EditorApplication::setGizmoMode(GizmoMode mode)
    {
        gizmoMode_ = mode;

        // Rotate and scale have no manipulator yet. Saying so beats letting the gizmo silently
        // disappear and leaving the user to guess whether they broke something.
        if (mode == GizmoMode::Rotate || mode == GizmoMode::Scale)
        {
            context_.log(LogSeverity::Info,
                         std::string{mode == GizmoMode::Rotate ? "Rotate" : "Scale"}
                             + " gizmo is not implemented yet; press W for the translate gizmo");
        }
    }

    void EditorApplication::drawMainMenu()
    {
        if (ui_->beginMenu("File"))
        {
            if (ui_->menuItem("New Scene", "Ctrl+N")) { context_.newScene(); }
            if (ui_->menuItem("Save Scene", "Ctrl+S", !context_.getScenePath().empty()))
            {
                context_.saveScene();
            }
            ui_->separator();
            if (ui_->menuItem("Exit", "Alt+F4")) { /* handled by the UI implementation */ }
            ui_->endMenu();
        }

        if (ui_->beginMenu("Edit"))
        {
            CommandHistory& history = context_.getHistory();
            if (ui_->menuItem("Undo " + history.getUndoDescription(), "Ctrl+Z", history.canUndo()))
            {
                undo();
            }
            if (ui_->menuItem("Redo " + history.getRedoDescription(), "Ctrl+Y", history.canRedo()))
            {
                redo();
            }

            ui_->separator();

            const bool hasSelection = !context_.getSelection().empty();
            if (ui_->menuItem("Duplicate", "Ctrl+D", hasSelection)) { duplicateSelection(); }
            if (ui_->menuItem("Delete", "Del", hasSelection)) { deleteSelection(); }
            ui_->endMenu();
        }

        if (ui_->beginMenu("View"))
        {
            if (ui_->menuItem("Frame Selected", "F", !context_.getSelection().empty()))
            {
                frameSelection();
            }

            ui_->separator();

            if (ui_->menuItem("Translate Gizmo", "W")) { setGizmoMode(GizmoMode::Translate); }
            if (ui_->menuItem("Rotate Gizmo", "E")) { setGizmoMode(GizmoMode::Rotate); }
            if (ui_->menuItem("Scale Gizmo", "R")) { setGizmoMode(GizmoMode::Scale); }
            ui_->endMenu();
        }
    }

    void EditorApplication::drawSceneHierarchyPanel()
    {
        if (!ui_->beginPanel("Scene Hierarchy", DockSide::Left)) { ui_->endPanel(); return; }

        ui_->text(context_.getScene().getName() + " ("
                  + std::to_string(context_.getScene().getEntityCount()) + " entities)");
        ui_->separator();

        if (ui_->button("Add Entity"))
        {
            EditorEntity entity{Uuid::generate(), "Entity"};
            EditorComponent transform{BuiltinComponentIds::kTransform};
            if (const ComponentDescriptor* descriptor =
                    context_.getComponentRegistry().find(BuiltinComponentIds::kTransform))
            {
                transform.applyDefaults(*descriptor);
            }
            entity.addComponent(std::move(transform));

            auto command = std::make_unique<CreateEntityCommand>(context_.getScene(), std::move(entity));
            const Uuid createdId = command->getEntityId();
            context_.execute(std::move(command));
            context_.select(createdId);
        }

        for (const Uuid& rootId : context_.getScene().getRootEntities()) { drawHierarchyNode(rootId); }

        ui_->endPanel();
    }

    void EditorApplication::drawHierarchyNode(const Uuid& entityId)
    {
        const EditorEntity* entity = context_.getScene().findEntity(entityId);
        if (entity == nullptr) { return; }

        const std::vector<Uuid> children = context_.getScene().getChildren(entityId);
        bool clicked = false;
        const bool expanded = ui_->treeNode(entityId, entity->getName(), context_.isSelected(entityId),
                                            children.empty(), clicked);

        if (clicked) { context_.select(entityId); }

        if (expanded)
        {
            for (const Uuid& childId : children) { drawHierarchyNode(childId); }
            ui_->treePop();
        }
    }

    void EditorApplication::drawInspectorPanel()
    {
        if (!ui_->beginPanel("Inspector", DockSide::Right)) { ui_->endPanel(); return; }

        const Uuid selectedId = context_.getPrimarySelection();
        const EditorEntity* entity = context_.getScene().findEntity(selectedId);
        if (entity == nullptr)
        {
            ui_->text("Nothing selected.");
            ui_->endPanel();
            return;
        }

        ui_->text("Entity: " + entity->getName());
        ui_->text("Id: " + entity->getId().toString());
        ui_->separator();

        for (const EditorComponent& component : entity->getComponents())
        {
            const ComponentDescriptor* descriptor = context_.getComponentRegistry().find(component.getTypeId());
            if (descriptor == nullptr)
            {
                // An unregistered component still has to be visible, or the user has no way to
                // discover that a scene depends on a plugin that failed to load.
                ui_->text(component.getTypeId() + "  (unregistered -- data preserved, not editable)");
                ui_->separator();
                continue;
            }

            ui_->text(descriptor->displayName);

            for (const PropertyDescriptor& property : descriptor->properties)
            {
                PropertyValue value = component.getPropertyOrDefault(property.name, descriptor);
                const std::string label = property.displayName.empty() ? property.name : property.displayName;

                if (ui_->propertyField(label, value, property.enumOptions, property.readOnly))
                {
                    // Merging means an inspector drag collapses into a single undo entry that
                    // returns to the value the drag started from.
                    context_.execute(std::make_unique<SetPropertyCommand>(
                                         context_.getScene(), entity->getId(), component.getTypeId(),
                                         property.name, value),
                                     MergePolicy::MergeWithPrevious);
                }
            }
            ui_->separator();
        }

        ui_->endPanel();
    }

    void EditorApplication::drawAssetBrowserPanel()
    {
        if (!ui_->beginPanel("Assets", DockSide::Bottom)) { ui_->endPanel(); return; }

        const AssetDatabase& assets = context_.getAssets();
        ui_->text(std::to_string(assets.getCount()) + " assets");
        ui_->separator();

        for (const AssetRecord* record : assets.getAll())
        {
            ui_->text(std::string{toString(record->type)} + "  " + record->sourcePath);
        }

        ui_->endPanel();
    }

    void EditorApplication::drawViewportPanel()
    {
        if (!ui_->beginPanel("Viewport", DockSide::Center)) { ui_->endPanel(); return; }

        const UiRegion region = ui_->getContentRegion();
        if (region.isEmpty()) { ui_->endPanel(); return; }

        const int width = static_cast<int>(region.width);
        const int height = static_cast<int>(region.height);

        // The scene is rendered at exactly the panel's size. Rendering at a fixed size and
        // stretching would make the grid non-square and, worse, make picking disagree with what
        // is on screen.
        const UiTextureId texture =
            viewport_->render(context_.getScene(), width, height, context_.getSelection(), gizmoMode_);

        const UiImageInteraction interaction =
            ui_->image("##viewport", texture, region.width, region.height,
                       viewport_->isRenderTextureFlippedVertically());

        handleViewportInteraction(interaction);

        ui_->endPanel();
    }

    void EditorApplication::handleViewportInteraction(const UiImageInteraction& interaction)
    {
        EditorCamera2D& camera = viewport_->getCamera();
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
                pickEntityAt(context_.getScene(), camera, cursor, viewport_->makeSizeProvider());

            // Clicking empty space clears the selection, which is what every editor does and what
            // makes "deselect" reachable without a keyboard.
            context_.select(pick.entityId);
        }
    }

    bool EditorApplication::beginGizmoDrag(const EditorVector2& cursor)
    {
        if (gizmoMode_ != GizmoMode::Translate) { return false; }

        const Uuid selectedId = context_.getPrimarySelection();
        const std::optional<TranslateGizmoLayout> layout =
            computeTranslateGizmoLayout(context_.getScene(), viewport_->getCamera(), selectedId);
        if (!layout) { return false; }

        const GizmoHandle handle = hitTestTranslateGizmo(*layout, cursor);
        if (handle == GizmoHandle::None) { return false; }

        if (!gizmoDrag_.begin(context_.getScene(), viewport_->getCamera(), selectedId, handle, cursor))
        {
            return false;
        }

        gizmoDragHasEdited_ = false;
        return true;
    }

    void EditorApplication::updateGizmoDrag(const EditorVector2& cursor)
    {
        const std::optional<EditorVector3> position =
            gizmoDrag_.update(context_.getScene(), viewport_->getCamera(), cursor);
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

    void EditorApplication::drawConsolePanel()
    {
        if (!ui_->beginPanel("Console", DockSide::Bottom)) { ui_->endPanel(); return; }
        ui_->drawLogView();
        ui_->endPanel();
    }
}
