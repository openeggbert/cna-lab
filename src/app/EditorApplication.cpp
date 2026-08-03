// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/EditorApplication.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
#include <system_error>

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

        /**
         * @brief Returns the console filter's display name for @p severity.
         *
         * Distinct from toString(), which produces the short prefix a log line carries ("warn").
         * Reusing that here would leave the combo's current value absent from its own option list,
         * and a combo whose selection matches nothing renders empty.
         */
        const char* severityDisplayName(LogSeverity severity)
        {
            switch (severity)
            {
                case LogSeverity::Trace: return "Trace";
                case LogSeverity::Info: return "Info";
                case LogSeverity::Warning: return "Warning";
                case LogSeverity::Error: return "Error";
            }
            return "Trace";
        }

        /** @brief Maps a display name from the console's filter back onto the enumeration. */
        LogSeverity parseLogSeverityName(std::string_view name)
        {
            if (name == "Error") { return LogSeverity::Error; }
            if (name == "Warning") { return LogSeverity::Warning; }
            if (name == "Info") { return LogSeverity::Info; }
            return LogSeverity::Trace;
        }

        /**
         * @brief Maps a severity name from the wire onto the console's enumeration.
         *
         * An unrecognised name becomes Info rather than being dropped: a player built from a newer
         * revision that invents a severity should still have its message reach the user.
         */
        LogSeverity parseLogSeverity(std::string_view name)
        {
            if (name == "error") { return LogSeverity::Error; }
            if (name == "warn" || name == "warning") { return LogSeverity::Warning; }
            if (name == "trace") { return LogSeverity::Trace; }
            return LogSeverity::Info;
        }
    }

    EditorOptions EditorOptions::parse(int argc, const char* const* argv)
    {
        EditorOptions options;

        // argv[0] is how the editor finds the `cna-player-*` binaries beside it. Because CNA fixes
        // its backend at compile time (finding F-01), the set of backends play mode can offer is
        // the set of player executables actually installed next to this one.
        if (argc > 0 && argv[0] != nullptr) { options.executablePath = argv[0]; }

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

        if (!options.executablePath.empty())
        {
            setPlayerBuilds(discoverPlayerBuilds(
                std::filesystem::path{options.executablePath}.parent_path().generic_string()));
        }

        return true;
    }

    void EditorApplication::setPlayerBuilds(std::vector<PlayerBuild> builds)
    {
        playerBuilds_ = std::move(builds);
        selectedBuild_ = 0;

        if (playerBuilds_.empty()) { return; }

        std::string names;
        for (const PlayerBuild& build : playerBuilds_)
        {
            if (!names.empty()) { names += ", "; }
            names += build.backend;
        }
        context_.log(LogSeverity::Info, "Player builds: " + names);
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
        pollPlayer();
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
        if (ui_->isShortcutPressed(UiKey::F2)) { beginRename(context_.getPrimarySelection()); }
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

    void EditorApplication::startPlay()
    {
        if (playMode_ != PlayMode::Stopped) { return; }

        if (playerBuilds_.empty())
        {
            context_.log(LogSeverity::Warning,
                         "No cna-player build was found beside the editor. Build one -- play mode "
                         "runs the game in a separate process, so it needs a player executable.");
            return;
        }
        if (!context_.hasProject())
        {
            context_.log(LogSeverity::Warning,
                         "Play needs an open project: the player is a separate process and loads "
                         "the project from disk.");
            return;
        }
        if (context_.getScenePath().empty())
        {
            context_.log(LogSeverity::Warning,
                         "Save the scene before playing: the player reads it from disk, so a scene "
                         "that has never been saved has nothing for it to load.");
            return;
        }

        // The player loads from disk, so what is on screen has to be on disk. Saving silently
        // would hide a real write to the user's file; saying so costs one console line.
        if (context_.getHistory().isDirty())
        {
            if (!context_.saveScene())
            {
                context_.log(LogSeverity::Error, "Could not save the scene; not starting the player.");
                return;
            }
            context_.log(LogSeverity::Info, "Saved the scene before playing.");
        }

        if (selectedBuild_ >= playerBuilds_.size()) { selectedBuild_ = 0; }
        const PlayerBuild& build = playerBuilds_[selectedBuild_];

        // The player takes the scene relative to the project, not as an absolute path: the two
        // processes may not agree on a working directory, and the project root is the one anchor
        // both of them already have.
        const std::filesystem::path projectFile{context_.getProject().getFilePath()};
        std::error_code relativeError;
        const std::filesystem::path relativeScene = std::filesystem::relative(
            std::filesystem::path{context_.getScenePath()}, projectFile.parent_path(), relativeError);

        auto player = std::make_unique<PlayerProcess>();
        if (!player->start(build, context_.getProject().getFilePath(),
                           relativeError ? std::string{} : relativeScene.generic_string()))
        {
            context_.log(LogSeverity::Error,
                         "Could not start '" + build.executablePath + "': " + player->getError());
            return;
        }

        player_ = std::move(player);
        playMode_ = PlayMode::Playing;
        context_.log(LogSeverity::Info,
                     "Playing on '" + build.backend + "' (port "
                         + std::to_string(player_->getPort()) + ")");
    }

    void EditorApplication::stopPlay()
    {
        if (!player_)
        {
            playMode_ = PlayMode::Stopped;
            return;
        }

        player_->stop();
        player_.reset();
        playMode_ = PlayMode::Stopped;
        context_.log(LogSeverity::Info, "Stopped playing.");
    }

    void EditorApplication::setPlayPaused(bool paused)
    {
        if (!player_ || playMode_ == PlayMode::Stopped) { return; }
        if ((playMode_ == PlayMode::Paused) == paused) { return; }

        EditorMessage message;
        message.type = paused ? EditorMessageType::Pause : EditorMessageType::Resume;
        message.payload = JsonValue::makeObject();

        // Only follow the player's state once the request is actually on the wire. A toolbar that
        // says "Paused" over a game that never got the message is worse than one that did nothing.
        if (!player_->send(message)) { return; }

        playMode_ = paused ? PlayMode::Paused : PlayMode::Playing;
    }

    void EditorApplication::stepPlayFrame()
    {
        // Stepping only means something while paused; the player ignores it otherwise, and
        // offering it while running would suggest a control that does nothing.
        if (!player_ || playMode_ != PlayMode::Paused) { return; }

        EditorMessage message;
        message.type = EditorMessageType::StepFrame;
        message.payload = JsonValue::makeObject();
        player_->send(message);
    }

    void EditorApplication::pollPlayer()
    {
        if (!player_) { return; }

        for (const EditorMessage& message : player_->poll())
        {
            switch (message.type)
            {
                case EditorMessageType::Ready:
                    context_.log(LogSeverity::Info,
                                 "Player ready: backend " + message.payload["backend"].asString()
                                     + ", scene '" + message.payload["scene"].asString() + "'");
                    break;

                case EditorMessageType::ReportLog:
                    context_.log(parseLogSeverity(message.payload["severity"].asString()),
                                 "player: " + message.payload["text"].asString());
                    break;

                case EditorMessageType::ReportException:
                    context_.log(LogSeverity::Error,
                                 "player: " + message.payload["message"].asString());
                    break;

                default:
                    break;
            }
        }

        if (player_->isRunning()) { return; }

        // The player going away on its own is normal -- the user closed the game window -- but the
        // toolbar has to follow it back to Stopped rather than keep offering Pause to a process
        // that no longer exists.
        const PlayerExitReason reason = player_->getExitReason();
        context_.log(reason == PlayerExitReason::Crashed ? LogSeverity::Error : LogSeverity::Info,
                     std::string{"Player: "} + toString(reason));

        player_.reset();
        playMode_ = PlayMode::Stopped;
    }

    void EditorApplication::drawPlayToolbar()
    {
        if (playMode_ == PlayMode::Stopped)
        {
            if (ui_->button("Play")) { startPlay(); }
        }
        else
        {
            if (ui_->button("Stop")) { stopPlay(); }
            ui_->sameLine();
            if (ui_->button(playMode_ == PlayMode::Paused ? "Resume" : "Pause"))
            {
                setPlayPaused(playMode_ != PlayMode::Paused);
            }
            ui_->sameLine();
            if (ui_->button("Step")) { stepPlayFrame(); }
        }

        ui_->sameLine();

        if (playerBuilds_.empty())
        {
            ui_->text("no player build found");
        }
        else if (playMode_ != PlayMode::Stopped)
        {
            ui_->text("running on " + playerBuilds_[selectedBuild_].backend
                      + (playMode_ == PlayMode::Paused ? " (paused)" : ""));
        }
        else
        {
            // The choice is an enum over what is installed, not over the fourteen backends CNA
            // knows about: offering a backend with no player binary would be offering a button
            // that cannot work.
            std::vector<std::string> names;
            names.reserve(playerBuilds_.size());
            for (const PlayerBuild& build : playerBuilds_) { names.push_back(build.backend); }

            // Wide enough for a backend name, narrow enough to leave the toolbar a toolbar.
            ui_->setNextItemWidth(150.0f);

            PropertyValue value{PropertyValue::EnumValue{names[selectedBuild_]}};
            if (ui_->propertyField("Backend", value, names))
            {
                const std::string chosen = value.get<PropertyValue::EnumValue>().name;
                for (std::size_t index = 0; index < names.size(); ++index)
                {
                    if (names[index] == chosen) { selectedBuild_ = index; break; }
                }
            }
        }

        ui_->separator();
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

        // Applied after the whole tree is drawn, never inside the recursion: a reparent reorders
        // the very lists drawHierarchyNode() is walking, and a delete invalidates them outright.
        applyPendingHierarchyAction();
    }

    void EditorApplication::drawHierarchyNode(const Uuid& entityId)
    {
        const EditorEntity* entity = context_.getScene().findEntity(entityId);
        if (entity == nullptr) { return; }

        const std::vector<Uuid> children = context_.getScene().getChildren(entityId);

        if (renamingEntity_ == entityId)
        {
            // A row being renamed is a text field, not a tree node, so nothing is pushed and
            // nothing is popped. Its children still draw, so the tree does not appear to lose a
            // subtree for as long as the field is open.
            drawRenameField(entityId);
            for (const Uuid& childId : children) { drawHierarchyNode(childId); }
            return;
        }

        const UiTreeNodeResult node =
            ui_->treeNode(entityId, entity->getName(), context_.isSelected(entityId), children.empty());

        // Whether the node pushed onto the tree stack is settled here and read nowhere else.
        // Pairing the pop with renamingEntity_ instead would be wrong in both directions: starting
        // a rename below would skip a pop that is owed, and committing one would add a pop that is
        // not -- and ImGui's tree stack does not survive either mistake.
        const bool pushed = node.expanded;

        // The drag source and the drop target are both the node just drawn: dragging one entity
        // onto another is how a hierarchy is rearranged, and both roles belong to every node.
        ui_->setDragSource(kEntityDragType, entityId.toString(), entity->getName());

        if (const std::optional<std::string> dropped = ui_->acceptDrop(kEntityDragType))
        {
            pending_.kind = HierarchyAction::Reparent;
            pending_.entityId = Uuid::parse(*dropped);
            pending_.parentId = entityId;
        }

        drawHierarchyContextMenu(entityId);

        // Double-click first: a double-click also reports a click, and starting a rename must win
        // over merely reselecting what is already selected.
        if (node.doubleClicked) { beginRename(entityId); }
        else if (node.clicked)
        {
            // Ctrl extends the selection rather than replacing it, which is what every editor does
            // and what makes "these three, not that one" reachable at all.
            if (ui_->getModifiers().control) { context_.toggleSelection(entityId); }
            else { context_.select(entityId); }
        }

        if (!pushed) { return; }

        for (const Uuid& childId : children) { drawHierarchyNode(childId); }
        ui_->treePop();
    }

    void EditorApplication::drawHierarchyContextMenu(const Uuid& entityId)
    {
        if (!ui_->beginContextMenu("entity-" + entityId.toString())) { return; }

        // Right-clicking a node acts on that node, so it becomes the selection first. Acting on
        // something other than what was right-clicked is the classic context-menu bug.
        if (!context_.isSelected(entityId)) { context_.select(entityId); }

        if (ui_->menuItem("Rename", "F2")) { beginRename(entityId); }
        if (ui_->menuItem("Duplicate", "Ctrl+D")) { duplicateSelection(); }

        const bool hasParent = context_.getScene().findEntity(entityId) != nullptr
                            && context_.getScene().findEntity(entityId)->getParentId().isValid();
        if (ui_->menuItem("Move to Root", {}, hasParent))
        {
            pending_.kind = HierarchyAction::Reparent;
            pending_.entityId = entityId;
            pending_.parentId = Uuid{};
        }

        ui_->separator();
        if (ui_->menuItem("Delete", "Del")) { pending_.kind = HierarchyAction::Delete; }

        ui_->endContextMenu();
    }

    void EditorApplication::beginRename(const Uuid& entityId)
    {
        const EditorEntity* entity = context_.getScene().findEntity(entityId);
        if (entity == nullptr) { return; }

        renamingEntity_ = entityId;
        renameBuffer_ = entity->getName();
        renameNeedsFocus_ = true;
        context_.select(entityId);
    }

    void EditorApplication::drawRenameField(const Uuid& entityId)
    {
        const UiTextFieldResult result =
            ui_->inputText("##rename-" + entityId.toString(), renameBuffer_, renameNeedsFocus_);

        // Only the first frame asks for focus. Asking every frame would make the field impossible
        // to leave, because it would take the keyboard back the instant anything else claimed it.
        renameNeedsFocus_ = false;

        if (!result.committed) { return; }

        renamingEntity_ = Uuid{};

        // An empty name is a slip, not an instruction: an unnamed row in the hierarchy is
        // unusable, and the old name is still right there to keep.
        const EditorEntity* entity = context_.getScene().findEntity(entityId);
        if (entity == nullptr || renameBuffer_.empty() || renameBuffer_ == entity->getName()) { return; }

        context_.execute(std::make_unique<RenameEntityCommand>(context_.getScene(), entityId, renameBuffer_));
    }

    void EditorApplication::applyPendingHierarchyAction()
    {
        const PendingHierarchyAction action = pending_;
        pending_ = PendingHierarchyAction{};

        switch (action.kind)
        {
            case HierarchyAction::None:
                return;

            case HierarchyAction::Delete:
                deleteSelection();
                return;

            case HierarchyAction::Reparent: {
                if (!action.entityId.isValid()) { return; }
                if (action.entityId == action.parentId) { return; }

                // Dropping a parent onto its own descendant would make a cycle. SceneDocument
                // rejects it, but a command that does nothing still lands in the undo stack, and
                // an entry the user cannot see the effect of is worse than no entry.
                if (action.parentId.isValid()
                    && context_.getScene().isAncestorOf(action.entityId, action.parentId))
                {
                    context_.log(LogSeverity::Warning,
                                 "Cannot move an entity under one of its own children.");
                    return;
                }

                const EditorEntity* entity = context_.getScene().findEntity(action.entityId);
                if (entity == nullptr || entity->getParentId() == action.parentId) { return; }

                context_.execute(std::make_unique<ReparentEntityCommand>(
                    context_.getScene(), action.entityId, action.parentId));
                return;
            }
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

        // Removal is deferred past the loop. Executing it here would mutate the very vector being
        // iterated, and invalidate the component reference the loop body is holding.
        std::optional<std::size_t> removeIndex;

        const std::vector<EditorComponent>& components = entity->getComponents();
        for (std::size_t index = 0; index < components.size(); ++index)
        {
            const EditorComponent& component = components[index];
            const ComponentDescriptor* descriptor = context_.getComponentRegistry().find(component.getTypeId());
            if (descriptor == nullptr)
            {
                // An unregistered component still has to be visible, or the user has no way to
                // discover that a scene depends on a plugin that failed to load. It is removable:
                // dropping a component whose plugin is gone is a legitimate way to fix a scene.
                ui_->text(component.getTypeId() + "  (unregistered -- data preserved, not editable)");
                ui_->sameLine();
                if (ui_->button("Remove##" + std::to_string(index))) { removeIndex = index; }
                ui_->separator();
                continue;
            }

            ui_->text(descriptor->displayName);

            // A required component is what makes the entity what it is -- removing the transform
            // would leave it with no position -- so it gets no button rather than a dead one.
            if (!descriptor->required)
            {
                ui_->sameLine();
                // The "##index" suffix is what keeps two components of the same type from sharing
                // one button identity; ImGui derives a widget's id from its label.
                if (ui_->button("Remove##" + std::to_string(index))) { removeIndex = index; }
            }

            for (const PropertyDescriptor& property : descriptor->properties)
            {
                const PropertyValue value = component.getPropertyOrDefault(property.name, descriptor);
                const std::optional<PropertyValue> edited =
                    drawPropertyRow(selectedId, component.getTypeId(), property, value);
                if (!edited) { continue; }

                // Merging means an inspector drag collapses into a single undo entry that
                // returns to the value the drag started from.
                context_.execute(std::make_unique<SetPropertyCommand>(
                                     context_.getScene(), selectedId, component.getTypeId(),
                                     property.name, *edited),
                                 MergePolicy::MergeWithPrevious);
            }
            ui_->separator();
        }

        drawAddComponentControl(*entity);
        ui_->endPanel();

        // Past every reference into the entity's component vector, so the mutation is safe.
        if (removeIndex)
        {
            auto command = std::make_unique<RemoveComponentCommand>(
                context_.getScene(), context_.getComponentRegistry(), selectedId, *removeIndex);
            if (command->isValid()) { context_.execute(std::move(command)); }
        }
    }

    std::optional<PropertyValue> EditorApplication::drawPropertyRow(const Uuid& entityId,
                                                                     const std::string& componentTypeId,
                                                                     const PropertyDescriptor& property,
                                                                     const PropertyValue& value)
    {
        const std::string label = property.displayName.empty() ? property.name : property.displayName;

        if (value.getType() != PropertyType::Quaternion)
        {
            PropertyValue edited = value;
            const bool changed = ui_->propertyField(label, edited, property.enumOptions, property.readOnly);

            // An asset slot is also a drop target for the browser, which is the only way to fill
            // one without copying a Uuid by hand.
            if (property.type == PropertyType::AssetReference && !property.readOnly)
            {
                if (const std::optional<PropertyValue> dropped = acceptAssetDrop(property))
                {
                    return dropped;
                }
            }

            return changed ? std::optional<PropertyValue>{edited} : std::nullopt;
        }

        // Quaternions are shown as degrees, because four raw components are not something anyone
        // can author: rotating a sprite by 45 degrees should not require working out a quaternion.
        const EditorQuaternion stored = value.get<EditorQuaternion>();

        // Reuse what the user typed for as long as the stored value is still exactly the one it
        // produced. Recomputing every frame would let the other two angles jump to an equivalent
        // spelling mid-edit; comparing against our own output means an undo, a gizmo drag or a
        // reload is picked up immediately, because none of those produce that exact quaternion.
        const bool cacheApplies = eulerEdit_.matches(entityId, componentTypeId, property.name)
                               && eulerEdit_.produced == stored;

        PropertyValue shown{cacheApplies ? eulerEdit_.degrees : eulerDegreesOf(stored)};
        if (!ui_->propertyField(label + " (deg)", shown, {}, property.readOnly))
        {
            return std::nullopt;
        }

        const EditorVector3 degrees = shown.get<EditorVector3>();
        const EditorQuaternion produced = quaternionFromEulerDegrees(degrees);

        eulerEdit_ = EulerEdit{entityId, componentTypeId, property.name, degrees, produced};
        return PropertyValue{produced};
    }

    std::optional<PropertyValue> EditorApplication::acceptAssetDrop(const PropertyDescriptor& property)
    {
        const std::optional<std::string> dropped = ui_->acceptDrop(kAssetDragType);
        if (!dropped) { return std::nullopt; }

        const Uuid assetId = Uuid::parse(*dropped);
        const AssetRecord* record = context_.getAssets().find(assetId);
        if (record == nullptr)
        {
            context_.log(LogSeverity::Warning, "Dropped asset is no longer in the database.");
            return std::nullopt;
        }

        // A slot that declares what it takes refuses everything else, and says which is which.
        // Silently accepting a sound into a texture slot would produce a scene that loads and a
        // sprite that never appears, with nothing anywhere to explain it.
        if (!property.assetType.empty() && property.assetType != toString(record->type))
        {
            context_.log(LogSeverity::Warning,
                         "'" + record->sourcePath + "' is a " + std::string{toString(record->type)}
                             + "; this field takes a " + property.assetType + ".");
            return std::nullopt;
        }

        return PropertyValue{PropertyValue::AssetReference{assetId}};
    }

    void EditorApplication::drawAddComponentControl(const EditorEntity& entity)
    {
        std::vector<std::string> labels;
        std::vector<std::string> typeIds;

        for (const std::string& typeId : context_.getComponentRegistry().getTypeIds())
        {
            const ComponentDescriptor* descriptor = context_.getComponentRegistry().find(typeId);
            if (descriptor == nullptr) { continue; }

            // A unique component the entity already has cannot be added again, so listing it would
            // be listing an entry that does nothing -- AddComponentCommand would refuse it anyway.
            if (descriptor->unique && entity.findComponent(typeId) != nullptr) { continue; }

            labels.push_back(descriptor->category.empty()
                                 ? descriptor->displayName
                                 : descriptor->category + " / " + descriptor->displayName);
            typeIds.push_back(typeId);
        }

        if (labels.empty())
        {
            ui_->text("Every component type is already on this entity.");
            return;
        }

        // The choice is remembered as a *type id*, not as an index: the list shortens the moment a
        // unique component is added, and an index would then silently point at a different type.
        std::size_t chosen = 0;
        for (std::size_t index = 0; index < typeIds.size(); ++index)
        {
            if (typeIds[index] == addComponentChoice_) { chosen = index; break; }
        }
        addComponentChoice_ = typeIds[chosen];

        ui_->setNextItemWidth(190.0f);

        PropertyValue value{PropertyValue::EnumValue{labels[chosen]}};
        if (ui_->propertyField("##addComponentType", value, labels))
        {
            const std::string picked = value.get<PropertyValue::EnumValue>().name;
            for (std::size_t index = 0; index < labels.size(); ++index)
            {
                if (labels[index] == picked) { addComponentChoice_ = typeIds[index]; break; }
            }
        }

        ui_->sameLine();

        if (ui_->button("Add Component"))
        {
            auto command = std::make_unique<AddComponentCommand>(
                context_.getScene(), context_.getComponentRegistry(), entity.getId(), addComponentChoice_);
            if (command->isValid()) { context_.execute(std::move(command)); }
        }
    }

    void EditorApplication::drawAssetBrowserPanel()
    {
        if (!ui_->beginPanel("Assets", DockSide::Bottom)) { ui_->endPanel(); return; }

        const AssetDatabase& assets = context_.getAssets();
        ui_->text(std::to_string(assets.getCount()) + " assets");
        ui_->separator();

        for (const AssetRecord* record : assets.getAll())
        {
            // A tree leaf rather than a line of text: it carries the asset's own Uuid as its
            // widget identity, which is what lets the toolkit tell one row from another when a
            // drag starts on it.
            const std::string label = std::string{toString(record->type)} + "  " + record->sourcePath;
            ui_->treeNode(record->id, label, false, true);
            ui_->setDragSource(kAssetDragType, record->id.toString(), label);
        }

        ui_->endPanel();
    }

    void EditorApplication::drawViewportPanel()
    {
        if (!ui_->beginPanel("Viewport", DockSide::Center)) { ui_->endPanel(); return; }

        // Above the image, so the scene is rendered into whatever is left. Putting the controls in
        // their own panel would let the user dock them away from the thing they control.
        drawPlayToolbar();

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

        if (ui_->button("Copy")) { ui_->setClipboardText(ui_->getLogText(consoleMinimumSeverity_)); }
        ui_->sameLine();
        if (ui_->button("Clear")) { ui_->clearLog(); }
        ui_->sameLine();
        ui_->checkbox("Auto-scroll", consoleAutoScroll_);
        ui_->sameLine();

        // The filter is the console's own state, not a document property, so it is not a command
        // and does not belong in the undo stack -- what a user chooses to look at is not an edit.
        static const std::vector<std::string> kSeverities{"Trace", "Info", "Warning", "Error"};
        ui_->setNextItemWidth(110.0f);

        PropertyValue severity{PropertyValue::EnumValue{severityDisplayName(consoleMinimumSeverity_)}};
        if (ui_->propertyField("##consoleSeverity", severity, kSeverities))
        {
            consoleMinimumSeverity_ = parseLogSeverityName(severity.get<PropertyValue::EnumValue>().name);
        }

        ui_->separator();

        UiLogViewOptions options;
        options.minimumSeverity = consoleMinimumSeverity_;
        options.autoScroll = consoleAutoScroll_;
        ui_->drawLogView(options);

        ui_->endPanel();
    }
}
