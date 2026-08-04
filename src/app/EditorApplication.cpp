// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/EditorApplication.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <optional>
#include <system_error>

#include "CNA/Editor/Assets/AssetImporters.hpp"
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
                if (name == "--recovery-dir") { options.recoveryDirectory = value; continue; }
                if (name == "--autosave")
                {
                    try { options.autosaveSeconds = std::stod(value); }
                    catch (const std::exception&)
                    {
                        options.hasError = true;
                        options.errorMessage = "--autosave expects a number of seconds, got '" + value + "'";
                    }
                    if (options.autosaveSeconds < 0.0) { options.autosaveSeconds = 0.0; }
                    continue;
                }
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
            "  --autosave=SECONDS Crash-recovery snapshot interval. 0 disables. Default: 30.\n"
            "  --recovery-dir=DIR Where snapshots are kept. Default: the per-user state directory.\n"
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
        : ui_(std::move(ui)),
          viewport_(std::move(viewport)),
          menuBar_(context_, *ui_, *this),
          hierarchyPanel_(context_, *ui_, *this),
          viewportPanel_(context_, *ui_, *this),
          inspectorPanel_(context_, *ui_, *this),
          historyPanel_(context_, *ui_, *this),
          assetBrowserPanel_(context_, *ui_, *this),
          validationPanel_(context_, *ui_, *this),
          consolePanel_(context_, *ui_, *this),
          diagnosticsPanel_(context_, *ui_, *this),
          buildPanel_(context_, *ui_, *this),
          comparisonPanel_(context_, *ui_, *this)
    {
        // Forward every context message into the console panel, so a single log() call reaches
        // both the UI and, through the UI implementation, stdout in headless runs.
        context_.setLogSink([this](LogSeverity severity, const std::string& message) {
            ui_->log(severity, message);
        });

        // Every document change goes through a command (D-06), so this one hook is enough to keep
        // a running player in step -- there is no second path a change could take.
        context_.setCommandObserver([this](const EditorCommand& command) { mirrorToPlayer(command); });
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
        autosaveInterval_ = options.autosaveSeconds;
        if (!options.recoveryDirectory.empty()) { recovery_ = RecoveryStore{options.recoveryDirectory}; }

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

        findRecoverableScene();
        return true;
    }

    void EditorApplication::findRecoverableScene()
    {
        recoverable_.reset();
        autosaveSuspensionReported_ = false;

        if (!context_.hasProject() || autosaveInterval_ <= 0.0) { return; }

        recoverable_ = recovery_.findForProject(context_.getProject().getFilePath());
        if (!recoverable_) { return; }

        // A warning, not information: the alternative reading of this state is that the user's
        // last session ended without saving, and either way there is work on disk that the
        // document in front of them does not contain.
        context_.log(LogSeverity::Warning,
                     "Unsaved changes to scene '" + recoverable_->sceneName + "' from "
                         + formatRecoveryTime(recoverable_->savedAtSeconds)
                         + " were found. File > Recover Unsaved Scene restores them; "
                           "File > Discard Recovered Scene throws them away.");
    }

    void EditorApplication::recoverScene()
    {
        if (!recoverable_) { return; }

        const RecoverySnapshot snapshot = *recoverable_;

        const SceneLoadResult result =
            context_.getScene().loadFromJson(snapshot.scene, context_.getComponentRegistry());
        if (!result.succeeded)
        {
            // The snapshot stays. A recovery that failed is not a reason to delete the only copy
            // of the work it was holding.
            context_.log(LogSeverity::Error, "Cannot recover the scene: " + result.errorMessage);
            return;
        }

        for (const std::string& warning : result.warnings)
        {
            context_.log(LogSeverity::Warning, "Recovered scene: " + warning);
        }

        context_.clearSelection();
        context_.getHistory().clear();

        // The recovered document was never saved, so no position in the fresh history is the file
        // on disk. Saying otherwise would let the user close the editor believing it was.
        context_.getHistory().markUnsaved();

        recoverable_.reset();
        autosaveSuspensionReported_ = false;

        context_.log(LogSeverity::Info,
                     "Recovered scene '" + context_.getScene().getName() + "' from "
                         + formatRecoveryTime(snapshot.savedAtSeconds)
                         + ". The file on disk is unchanged until you save. Undo history was not "
                           "recovered.");
    }

    void EditorApplication::discardRecoveredScene()
    {
        if (!recoverable_) { return; }

        const std::string name = recoverable_->sceneName;
        recovery_.discard(recoverable_->sceneId);
        recoverable_.reset();
        autosaveSuspensionReported_ = false;

        context_.log(LogSeverity::Info, "Discarded the recovered copy of '" + name + "'.");
    }

    void EditorApplication::updateAutosave(double deltaSeconds)
    {
        if (autosaveInterval_ <= 0.0) { return; }

        const SceneDocument& scene = context_.getScene();

        if (!context_.getHistory().isDirty())
        {
            // The document matches its file, so there is nothing a snapshot could rescue. Dropping
            // it here is what stops the next start-up offering a recovery of work already saved --
            // an offer that trains users to click "discard" without reading it.
            if (autosaveWritten_)
            {
                recovery_.discard(scene.getSceneId());
                autosaveWritten_ = false;
            }
            autosaveElapsed_ = 0.0;
            return;
        }

        autosaveElapsed_ += deltaSeconds;
        if (autosaveElapsed_ < autosaveInterval_) { return; }
        autosaveElapsed_ = 0.0;

        // Never write over work from a previous session that the user has not answered for yet.
        // The current session's unsaved seconds are worth less than the previous session's unsaved
        // hours, and the snapshot file is keyed by scene id, so this would overwrite it.
        if (recoverable_ && recoverable_->sceneId == scene.getSceneId())
        {
            if (!autosaveSuspensionReported_)
            {
                autosaveSuspensionReported_ = true;
                context_.log(LogSeverity::Warning,
                             "Autosave is suspended while recovered work from a previous session is "
                             "waiting. Recover it or discard it from the File menu.");
            }
            return;
        }

        RecoverySnapshot snapshot;
        snapshot.projectPath = context_.getProject().getFilePath();
        snapshot.scenePath = context_.getScenePath();
        snapshot.sceneName = scene.getName();
        snapshot.sceneId = scene.getSceneId();
        snapshot.savedAtSeconds = static_cast<std::int64_t>(std::time(nullptr));
        snapshot.scene = scene.toJson();

        std::string errorMessage;
        if (!recovery_.write(snapshot, &errorMessage))
        {
            // Said once. An editor that repeats a filesystem complaint every thirty seconds is one
            // whose console nobody reads.
            if (!autosaveFailureReported_)
            {
                autosaveFailureReported_ = true;
                context_.log(LogSeverity::Error, "Cannot write a crash-recovery snapshot: " + errorMessage);
            }
            return;
        }

        autosaveWritten_ = true;
        autosaveFailureReported_ = false;
    }

    void EditorApplication::setAudio(std::unique_ptr<EditorAudio> audio)
    {
        if (!audio) { return; }

        audio_ = std::move(audio);
        context_.log(LogSeverity::Info, std::string{"Audio: "} + audio_->getBackendName());
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

    void EditorApplication::renderFrame(double deltaSeconds)
    {
        elapsedSeconds_ += deltaSeconds;

        updateAutosave(deltaSeconds);
        buildPanel_.poll();
        comparisonPanel_.poll(elapsedSeconds_);
        pollAssets(deltaSeconds);
        pollPlayer();
        handleShortcuts();

        ui_->beginDockSpace();

        // Order matters only for the menu bar, which must come first. The rest is the reading
        // order of the default layout: left, centre, right, then the bottom pair.
        menuBar_.draw();
        hierarchyPanel_.draw();
        viewportPanel_.draw();
        inspectorPanel_.setFrameDelta(deltaSeconds);
        inspectorPanel_.draw();
        historyPanel_.draw();
        assetBrowserPanel_.draw();
        validationPanel_.draw();
        consolePanel_.draw();
        diagnosticsPanel_.draw();
        buildPanel_.draw();
        comparisonPanel_.draw();

        ui_->endDockSpace();
    }

    void EditorApplication::handleShortcuts()
    {
        // Ctrl-chords first. Each is exact-modifier matched by the UI layer, so Ctrl+Shift+Z does
        // not also fire the undo bound to Ctrl+Z.
        if (ui_->isShortcutPressed(UiKey::Z, withControl())) { undo(); }
        if (ui_->isShortcutPressed(UiKey::Y, withControl())) { redo(); }
        if (ui_->isShortcutPressed(UiKey::N, withControl())) { newScene(); }
        if (ui_->isShortcutPressed(UiKey::S, withControl())) { saveScene(); }
        if (ui_->isShortcutPressed(UiKey::D, withControl())) { duplicateSelection(); }

        if (ui_->isShortcutPressed(UiKey::Delete)) { deleteSelection(); }
        if (ui_->isShortcutPressed(UiKey::F2)) { beginRename(context_.getPrimarySelection()); }
        if (ui_->isShortcutPressed(UiKey::F)) { frameSelection(); }

        if (ui_->isShortcutPressed(UiKey::W)) { setGizmoMode(GizmoMode::Translate); }
        if (ui_->isShortcutPressed(UiKey::E)) { setGizmoMode(GizmoMode::Rotate); }
        if (ui_->isShortcutPressed(UiKey::R)) { setGizmoMode(GizmoMode::Scale); }

        // X toggles rather than selecting, which is what every editor with this key does: there
        // are two spaces, and a toggle needs no second binding to get back.
        if (ui_->isShortcutPressed(UiKey::X))
        {
            setGizmoSpace(gizmoSpace_ == GizmoSpace::World ? GizmoSpace::Local : GizmoSpace::World);
        }
    }

    void EditorApplication::setEditorTool(EditorTool tool)
    {
        if (tool_ == tool) { return; }

        tool_ = tool;
        context_.log(LogSeverity::Info, std::string{"Tool: "} + toString(tool));
    }

    void EditorApplication::newScene() { context_.newScene(); }

    void EditorApplication::saveScene()
    {
        // A scene that has never been saved has no path to save back to. The menu greys the item
        // out; the shortcut has no such affordance, so the guard lives here where both reach it.
        if (context_.getScenePath().empty()) { return; }
        if (!context_.saveScene()) { return; }

        // The file on disk now holds this work, so the snapshot has nothing left to rescue.
        recovery_.discard(context_.getScene().getSceneId());
        autosaveWritten_ = false;
        autosaveElapsed_ = 0.0;
    }

    void EditorApplication::beginRename(const Uuid& entityId)
    {
        hierarchyPanel_.beginRename(entityId);
    }

    void EditorApplication::undo()
    {
        CommandHistory& history = context_.getHistory();
        if (!history.undo()) { return; }
        context_.pruneSelection();

        // An undo is a document change like any other, and a player that saw the edit but not its
        // reversal would be showing a state that exists nowhere.
        if (const EditorCommand* entry = history.getCommandAt(history.getCursor()))
        {
            mirrorToPlayer(*entry);
        }
    }

    void EditorApplication::redo()
    {
        CommandHistory& history = context_.getHistory();
        const std::size_t index = history.getCursor();
        if (!history.redo()) { return; }
        context_.pruneSelection();

        if (const EditorCommand* entry = history.getCommandAt(index)) { mirrorToPlayer(*entry); }
    }

    void EditorApplication::mirrorToPlayer(const EditorCommand& command)
    {
        if (!player_ || playMode_ == PlayMode::Stopped) { return; }

        // A dynamic_cast rather than a virtual on EditorCommand. Asking a command to describe
        // itself in protocol terms would put the wire format into cna-editor-scene, which links
        // neither the bridge nor anything that knows a player exists -- and the whole point of
        // D-03-style layering is that the lower module stays ignorant of the higher one.
        const auto* setProperty = dynamic_cast<const SetPropertyCommand*>(&command);
        if (setProperty == nullptr) { return; }

        // The document, not the command's own new value: after an undo the live value is the old
        // one, and the document is the only thing that is right in both directions.
        const EditorEntity* entity = context_.getScene().findEntity(setProperty->getEntityId());
        if (entity == nullptr) { return; }

        const EditorComponent* component = entity->findComponent(setProperty->getComponentTypeId());
        if (component == nullptr) { return; }

        player_->send(EditorMessage::makeSetProperty(setProperty->getEntityId(),
                                                     setProperty->getComponentTypeId(),
                                                     setProperty->getPropertyName(),
                                                     component->getProperty(setProperty->getPropertyName())));
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
    }

    void EditorApplication::setGizmoSpace(GizmoSpace space)
    {
        if (gizmoSpace_ == space) { return; }

        gizmoSpace_ = space;

        // Worth a line in the console: the two spaces look identical on an unrotated entity, so a
        // user who toggles while nothing is turned would otherwise see no evidence it did anything
        // -- and would reasonably conclude the key is broken.
        context_.log(LogSeverity::Info, std::string{"Gizmo space: "} + toString(space));
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

    void EditorApplication::pollAssets(double deltaSeconds)
    {
        const AssetWatchResult result = watcher_.poll(context_.getAssets(), deltaSeconds);
        if (!result.hasChanges()) { return; }

        for (const Uuid& assetId : result.changed)
        {
            // Dropping the cached texture is what makes the change visible. Without it the editor
            // would report the edit and go on drawing the art from before it.
            viewport_->invalidateAsset(assetId);
            reloadAssetInPlayer(assetId);

            const AssetRecord* record = context_.getAssets().find(assetId);
            context_.log(LogSeverity::Info,
                         "Reloaded '" + (record != nullptr ? record->sourcePath : assetId.toString())
                             + "' after an external change.");
        }

        for (const Uuid& assetId : result.restored)
        {
            viewport_->invalidateAsset(assetId);
            reloadAssetInPlayer(assetId);

            const AssetRecord* record = context_.getAssets().find(assetId);
            context_.log(LogSeverity::Info,
                         "'" + (record != nullptr ? record->sourcePath : assetId.toString())
                             + "' is back.");
        }

        for (const Uuid& assetId : result.removed)
        {
            const AssetRecord* record = context_.getAssets().find(assetId);
            context_.log(LogSeverity::Warning,
                         "'" + (record != nullptr ? record->sourcePath : assetId.toString())
                             + "' has gone missing. Anything referencing it is listed in Missing "
                               "References.");
        }

        // The pixel size of a texture that just changed is no longer the one on record.
        applyImporterFacts(context_.getAssets());
    }

    void EditorApplication::reloadAssetInPlayer(const Uuid& assetId)
    {
        // Only while a game is actually running. Sending to a stopped player is not merely useless
        // -- there is no process to send to, and the failure would be reported as a broken bridge.
        if (!player_ || playMode_ == PlayMode::Stopped) { return; }

        player_->send(EditorMessage::makeReloadAsset(assetId));
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
}
