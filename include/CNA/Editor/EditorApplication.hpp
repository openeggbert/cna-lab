// SPDX-License-Identifier: MS-PL
#pragma once

/**
 * @file CNA/Editor/EditorApplication.hpp
 * @brief Ties the context, the UI, the viewport and the panels together and runs the frame loop.
 *
 * What is left here after ED-210 is what no single panel owns: the document lifecycle, the
 * keyboard shortcuts, the play process, and the handful of operations a panel, the menu bar and a
 * shortcut can all trigger. Those reach a panel through EditorActions rather than through a back
 * reference to the whole application, which is what keeps a panel from quietly growing a
 * dependency on the editor's internals.
 */

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "CNA/Editor/Assets/AssetWatcher.hpp"
#include "CNA/Editor/EditorContext.hpp"
#include "CNA/Editor/Panels/AssetBrowserPanel.hpp"
#include "CNA/Editor/Panels/ConsolePanel.hpp"
#include "CNA/Editor/Panels/DiagnosticsPanel.hpp"
#include "CNA/Editor/Panels/EditorPanel.hpp"
#include "CNA/Editor/Panels/HierarchyPanel.hpp"
#include "CNA/Editor/Panels/InspectorPanel.hpp"
#include "CNA/Editor/Panels/MainMenuBar.hpp"
#include "CNA/Editor/Panels/HistoryPanel.hpp"
#include "CNA/Editor/Panels/ValidationPanel.hpp"
#include "CNA/Editor/Panels/ViewportPanel.hpp"
#include "CNA/Editor/Project/RecoveryStore.hpp"
#include "CNA/Editor/RuntimeBridge/PlayerProcess.hpp"
#include "CNA/Editor/Ui/EditorUi.hpp"
#include "CNA/Editor/Viewport/EditorViewport.hpp"

namespace CNA::Editor
{
    /** @brief Parsed command line. */
    struct EditorOptions
    {
        /** @brief Path to a `.cnaproject` to open at start-up. */
        std::string projectPath;

        /** @brief Path to a `.cnascene` to open, overriding the project's startup scene. */
        std::string scenePath;

        /**
         * @brief Requested UI toolkit, e.g. "imgui" or "null".
         *
         * Note that this selects the *UI toolkit*, not the CNA graphics backend. CNA's backend is
         * fixed at compile time (ANALYSIS.md finding F-01), so `--graphics=` on the editor would
         * be a lie -- it appears instead on cna-player, where it chooses which player binary to
         * launch.
         */
        std::string uiBackend = "imgui";

        /** @brief Run with no window, on NullEditorUi. */
        bool headless = false;

        /** @brief Exit after this many frames. Zero means run until the user quits. */
        int frameLimit = 0;

        /**
         * @brief Write a PNG of the final frame here. Requires a frame limit.
         *
         * A smoke test that only checks the process exited cleanly cannot tell a working editor
         * from one that opened a blank window; an image can.
         */
        std::string screenshotPath;

        /**
         * @brief argv[0], used to find the `cna-player-*` binaries beside the editor.
         *
         * Play mode offers exactly the backends whose player executable is installed, which is a
         * direct consequence of CNA fixing its backend at compile time (ANALYSIS.md finding F-01).
         */
        std::string executablePath;

        /**
         * @brief Seconds between crash-recovery snapshots of an unsaved scene. Zero disables them.
         *
         * The reliable half of crash recovery is the part that runs before the crash: the snapshot
         * is already on disk when the process dies, and needs nothing from the dying process. This
         * is how much work a crash can cost.
         */
        double autosaveSeconds = 30.0;

        /**
         * @brief Where snapshots are kept. Empty means the per-user default.
         *
         * Set it to keep a sandboxed or portable run from writing outside its own directory.
         */
        std::string recoveryDirectory;

        /** @brief Print the backend table and exit. */
        bool listBackends = false;

        /** @brief Print usage and exit. */
        bool showHelp = false;

        /** @brief Print the version and exit. */
        bool showVersion = false;

        /** @brief Set when parsing failed; @c errorMessage says why. */
        bool hasError = false;
        std::string errorMessage;

        /** @brief Parses argv. Never throws and never exits the process. */
        static EditorOptions parse(int argc, const char* const* argv);

        /** @brief Returns the usage text. */
        static std::string getUsage();
    };

    /**
     * @brief The editor application.
     *
     * Owns the context, the UI, the viewport, the panels and the play process, and runs the frame
     * loop. It is constructed with an EditorUi and an EditorViewport rather than creating them, so
     * `--headless` and the unit tests build the exact same application over the null
     * implementations -- there is no separate "test mode" path that can drift from the real one.
     *
     * The panels are classes now (plan.md ED-210); what remains here is what is genuinely shared:
     * the document lifecycle, the keyboard shortcuts, the play process, and the operations that a
     * panel, the menu bar and a shortcut can all trigger. It implements EditorActions so that a
     * panel reaches those through one narrow interface rather than through the whole application.
     */
    class EditorApplication final : public EditorActions
    {
    public:
        /**
         * @param ui The UI implementation; must not be null.
         * @param viewport The viewport implementation; must not be null.
         */
        EditorApplication(std::unique_ptr<EditorUi> ui, std::unique_ptr<EditorViewport> viewport);

        /** @brief Applies @p options: opens the project and scene, sets the frame limit. */
        bool initialize(const EditorOptions& options);

        /** @brief Runs frames until exit. Returns the process exit code. */
        int run();

        /**
         * @brief Draws exactly one frame. Exposed so tests can step the editor deterministically.
         *
         * @param deltaSeconds Time since the previous frame, which paces the asset watcher. Tests
         *        pass an explicit value so a poll can be forced without any sleeping.
         */
        void renderFrame(double deltaSeconds = 0.0);

        /** @brief Returns the asset watcher, so a test can shorten its interval. */
        [[nodiscard]] AssetWatcher& getAssetWatcher() { return watcher_; }

        [[nodiscard]] EditorContext& getContext() { return context_; }
        [[nodiscard]] EditorUi& getUi() { return *ui_; }
        [[nodiscard]] EditorViewport& getViewport() override { return *viewport_; }

        /**
         * @brief Replaces the viewport.
         *
         * The CNA-backed viewport needs a graphics device and a UI renderer, neither of which
         * exists when the application is constructed. Swapping it in later keeps every panel
         * written against the abstraction and keeps the construction order honest, rather than
         * having the application reach out for a device it does not own.
         */
        void setViewport(std::unique_ptr<EditorViewport> viewport);

        /**
         * @brief Replaces the discovered player builds.
         *
         * Discovery scans the editor's own directory, which a unit test has no control over. This
         * lets a test state what is installed instead of depending on how the build tree happens to
         * be laid out on the machine running it.
         */
        void setPlayerBuilds(std::vector<PlayerBuild> builds);

        // EditorActions. Each of these is reachable from the menu bar, from a keyboard shortcut and
        // from at least one panel, and must behave identically whichever asked.
        void undo() override;
        void redo() override;
        void newScene() override;
        void saveScene() override;
        void duplicateSelection() override;
        void deleteSelection() override;
        void frameSelection() override;
        void beginRename(const Uuid& entityId) override;

        void setGizmoMode(GizmoMode mode) override;
        [[nodiscard]] GizmoMode getGizmoMode() const override { return gizmoMode_; }

        void startPlay() override;
        void stopPlay() override;
        void setPlayPaused(bool paused) override;
        void stepPlayFrame() override;

        [[nodiscard]] PlayMode getPlayMode() const override { return playMode_; }
        [[nodiscard]] const std::vector<PlayerBuild>& getPlayerBuilds() const override { return playerBuilds_; }
        [[nodiscard]] std::size_t getSelectedPlayerBuild() const override { return selectedBuild_; }
        void selectPlayerBuild(std::size_t index) override { selectedBuild_ = index; }

        [[nodiscard]] const RecoverySnapshot* getRecoverableScene() const override
        {
            return recoverable_ ? &*recoverable_ : nullptr;
        }
        void recoverScene() override;
        void discardRecoveredScene() override;

        void setEditorTool(EditorTool tool) override;
        [[nodiscard]] EditorTool getEditorTool() const override { return tool_; }

        void setPaintTile(std::int64_t tile) override { paintTile_ = tile; }
        [[nodiscard]] std::int64_t getPaintTile() const override { return paintTile_; }

        void setAnimationPreview(const AnimationPreview& preview) override { animationPreview_ = preview; }
        [[nodiscard]] const AnimationPreview& getAnimationPreview() const override
        {
            return animationPreview_;
        }

        /** @brief Returns the snapshot store, so a test can point it at a scratch directory. */
        [[nodiscard]] RecoveryStore& getRecoveryStore() { return recovery_; }

    private:
        /**
         * @brief Applies this frame's keyboard shortcuts.
         *
         * Runs before the panels, so a shortcut and the menu item bound to the same operation both
         * take effect on the frame they are triggered.
         */
        void handleShortcuts();

        /**
         * @brief Reads whatever the player sent this frame and routes it into the console.
         *
         * Also notices the player going away on its own -- the user closing the game window is a
         * perfectly normal way to end play mode, and the toolbar has to follow it back to Stopped
         * rather than keep offering Pause for a process that is gone.
         */
        void pollPlayer();

        /**
         * @brief Notices assets changed outside the editor and reloads what they affect.
         *
         * A texture edited in another program is the common case, and without this the editor goes
         * on showing the old art until it is restarted.
         */
        void pollAssets(double deltaSeconds);

        /**
         * @brief Writes a crash-recovery snapshot when one is due, or drops a stale one.
         *
         * Only while the document differs from its file: a snapshot of a scene that matches disk
         * protects nothing and would offer a pointless recovery on the next start-up.
         */
        void updateAutosave(double deltaSeconds);

        /** @brief Looks for unsaved work from a previous session and reports what it finds. */
        void findRecoverableScene();

        /**
         * @brief Sends a document change to the running player, when it is one the wire can carry.
         *
         * Every document change goes through a command (D-06), so this one hook sees all of them.
         * Only property edits are mirrored today; anything else is left alone rather than guessed
         * at, because a partially applied scene in the player would be worse than a stale one.
         */
        void mirrorToPlayer(const EditorCommand& command);

        /** @brief Tells the running player that one asset changed on disk. */
        void reloadAssetInPlayer(const Uuid& assetId);

        EditorContext context_;
        std::unique_ptr<EditorUi> ui_;
        std::unique_ptr<EditorViewport> viewport_;

        MainMenuBar menuBar_;
        HierarchyPanel hierarchyPanel_;
        ViewportPanel viewportPanel_;
        InspectorPanel inspectorPanel_;
        HistoryPanel historyPanel_;
        AssetBrowserPanel assetBrowserPanel_;
        ValidationPanel validationPanel_;
        ConsolePanel consolePanel_;
        DiagnosticsPanel diagnosticsPanel_;

        GizmoMode gizmoMode_ = GizmoMode::Translate;
        EditorTool tool_ = EditorTool::Select;
        std::int64_t paintTile_ = 0;
        AnimationPreview animationPreview_;

        /**
         * @brief The player builds installed beside the editor, and which one Play will launch.
         *
         * A list rather than a single choice because CNA fixes its backend at compile time
         * (ANALYSIS.md finding F-01): "play this on Vulkan" means "launch `cna-player-vulkan`", so
         * the set of available backends is the set of binaries actually on disk.
         */
        std::vector<PlayerBuild> playerBuilds_;
        std::size_t selectedBuild_ = 0;

        AssetWatcher watcher_;

        std::unique_ptr<PlayerProcess> player_;
        PlayMode playMode_ = PlayMode::Stopped;

        int frameLimit_ = 0;
        int framesRendered_ = 0;

        RecoveryStore recovery_{getDefaultRecoveryDirectory()};
        double autosaveInterval_ = 30.0;
        double autosaveElapsed_ = 0.0;

        /** @brief Unsaved work from a previous session, waiting for the user to accept or drop it. */
        std::optional<RecoverySnapshot> recoverable_;

        /** @brief Whether a snapshot for the open scene is currently on disk. */
        bool autosaveWritten_ = false;

        /** @brief Whether the "autosave is suspended" warning has already been said. */
        bool autosaveSuspensionReported_ = false;

        /** @brief Whether the "cannot write a snapshot" error has already been said. */
        bool autosaveFailureReported_ = false;
    };
}
