// SPDX-License-Identifier: MS-PL
#pragma once

/**
 * @file CNA/Editor/EditorApplication.hpp
 * @brief Ties the context, the UI and the viewport together and runs the frame loop.
 *
 * The application owns the three things a panel needs and nothing more. It is constructed with an
 * EditorUi and an EditorViewport rather than creating them, so that `--headless` and the unit
 * tests build the exact same application over the null implementations -- there is no separate
 * "test mode" code path that can drift away from the real one.
 */

#include <memory>
#include <string>
#include <vector>

#include "CNA/Editor/EditorContext.hpp"
#include "CNA/Editor/RuntimeBridge/PlayerProcess.hpp"
#include "CNA/Editor/Scene/TranslateGizmo.hpp"
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
     * run() drives frames until the UI reports exit or the frame limit is reached. Each frame
     * draws the standard panel set; a panel is a method here in Phase 0 and becomes its own class
     * in Phase 1, once there is enough of one to be worth separating (plan.md ED-210).
     */
    /** @brief What the editor believes the player process is doing. */
    enum class PlayMode
    {
        Stopped,
        Playing,
        Paused
    };

    class EditorApplication
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

        /** @brief Draws exactly one frame. Exposed so tests can step the editor deterministically. */
        void renderFrame();

        [[nodiscard]] EditorContext& getContext() { return context_; }
        [[nodiscard]] EditorUi& getUi() { return *ui_; }
        [[nodiscard]] EditorViewport& getViewport() { return *viewport_; }

        /**
         * @brief Replaces the viewport.
         *
         * The CNA-backed viewport needs a graphics device and a UI renderer, neither of which
         * exists when the application is constructed. Swapping it in later keeps every panel
         * written against the abstraction and keeps the construction order honest, rather than
         * having the application reach out for a device it does not own.
         */
        void setViewport(std::unique_ptr<EditorViewport> viewport);

        /** @brief Returns the play-mode state the toolbar is showing. */
        [[nodiscard]] PlayMode getPlayMode() const { return playMode_; }

        /** @brief Returns the player builds found next to the editor. */
        [[nodiscard]] const std::vector<PlayerBuild>& getPlayerBuilds() const { return playerBuilds_; }

        /**
         * @brief Replaces the discovered player builds.
         *
         * Discovery scans the editor's own directory, which a unit test has no control over. This
         * lets a test state what is installed instead of depending on how the build tree happens to
         * be laid out on the machine running it.
         */
        void setPlayerBuilds(std::vector<PlayerBuild> builds);

        /** @brief Launches the player on the selected build. */
        void startPlay();

        /** @brief Stops the player, if one is running. */
        void stopPlay();

        /** @brief Pauses or resumes the running player. */
        void setPlayPaused(bool paused);

        /** @brief Asks a paused player to advance exactly one frame. */
        void stepPlayFrame();

    private:
        void drawMainMenu();
        void drawSceneHierarchyPanel();
        void drawInspectorPanel();
        void drawAssetBrowserPanel();
        void drawViewportPanel();
        void drawConsolePanel();

        /** @brief Recursively draws @p entityId and its children in the hierarchy tree. */
        void drawHierarchyNode(const Uuid& entityId);

        /**
         * @brief Applies this frame's keyboard shortcuts.
         *
         * Runs before the panels, so a shortcut and the menu item bound to the same operation both
         * take effect on the frame they are triggered. Each operation lives in a method below and
         * is called from both places -- a shortcut that quietly does something slightly different
         * from its menu item is a bug users report as "undo is broken".
         */
        void handleShortcuts();

        void undo();
        void redo();

        /** @brief Duplicates each selected entity's subtree and selects the copy. */
        void duplicateSelection();

        /** @brief Deletes each selected entity's subtree. */
        void deleteSelection();

        /** @brief Moves and zooms the camera so the selection fills the viewport. */
        void frameSelection();

        /** @brief Switches the active manipulator, reporting when it has no implementation yet. */
        void setGizmoMode(GizmoMode mode);

        /** @brief Draws the play controls at the top of the viewport panel. */
        void drawPlayToolbar();

        /**
         * @brief Reads whatever the player sent this frame and routes it into the console.
         *
         * Also notices the player going away on its own -- the user closing the game window is a
         * perfectly normal way to end play mode, and the toolbar has to follow it back to Stopped
         * rather than keep offering Pause for a process that is gone.
         */
        void pollPlayer();

        /** @brief Turns one frame of viewport pointer input into camera moves and selection. */
        void handleViewportInteraction(const UiImageInteraction& interaction);

        /**
         * @brief Starts a gizmo drag if @p cursor is over a handle of the selected entity's gizmo.
         * @return True when a drag began, in which case the press must not also reach the picker.
         */
        bool beginGizmoDrag(const EditorVector2& cursor);

        /** @brief Applies the in-progress drag to the entity's position as one merged command. */
        void updateGizmoDrag(const EditorVector2& cursor);

        EditorContext context_;
        std::unique_ptr<EditorUi> ui_;
        std::unique_ptr<EditorViewport> viewport_;
        GizmoMode gizmoMode_ = GizmoMode::Translate;

        TranslateGizmoDrag gizmoDrag_;

        /**
         * @brief Whether the current drag has already pushed a command.
         *
         * The first edit of a drag is a *new* undo entry and every later one merges into it. Without
         * this, a second drag of the same entity would merge into the first -- the merge key is
         * entity + component + property and has no notion of where one interaction ends -- and the
         * two moves would undo together as if they had been one.
         */
        bool gizmoDragHasEdited_ = false;

        /**
         * @brief The player builds installed beside the editor, and which one Play will launch.
         *
         * A list rather than a single choice because CNA fixes its backend at compile time
         * (ANALYSIS.md finding F-01): "play this on Vulkan" means "launch `cna-player-vulkan`", so
         * the set of available backends is the set of binaries actually on disk.
         */
        std::vector<PlayerBuild> playerBuilds_;
        std::size_t selectedBuild_ = 0;

        std::unique_ptr<PlayerProcess> player_;
        PlayMode playMode_ = PlayMode::Stopped;

        int frameLimit_ = 0;
        int framesRendered_ = 0;
    };
}
