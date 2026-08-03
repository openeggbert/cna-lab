// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/EditorApplication.hpp"

#include <cstring>
#include <iostream>

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
        ui_->beginDockSpace();

        drawMainMenu();
        drawSceneHierarchyPanel();
        drawViewportPanel();
        drawInspectorPanel();
        drawAssetBrowserPanel();
        drawConsolePanel();

        ui_->endDockSpace();
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
                history.undo();
                context_.pruneSelection();
            }
            if (ui_->menuItem("Redo " + history.getRedoDescription(), "Ctrl+Y", history.canRedo()))
            {
                history.redo();
                context_.pruneSelection();
            }
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

        // Grid first, then the game's own content, then the editor's overlay on top. The overlay
        // is a separate pass precisely so its objects never enter the scene document.
        viewport_->renderGrid();
        viewport_->renderScene(context_.getScene());
        viewport_->renderIcons(context_.getScene());
        viewport_->renderSelectionOutline(context_.getSelection());
        viewport_->renderGizmos(context_.getSelection(), gizmoMode_);

        ui_->endPanel();
    }

    void EditorApplication::drawConsolePanel()
    {
        if (!ui_->beginPanel("Console", DockSide::Bottom)) { ui_->endPanel(); return; }
        // The messages themselves live in the UI implementation, which owns the scroll state and
        // the severity filter; nothing to draw from here.
        ui_->endPanel();
    }
}
