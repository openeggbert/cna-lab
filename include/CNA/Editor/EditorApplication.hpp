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

    private:
        void drawMainMenu();
        void drawSceneHierarchyPanel();
        void drawInspectorPanel();
        void drawAssetBrowserPanel();
        void drawViewportPanel();
        void drawConsolePanel();

        /** @brief Recursively draws @p entityId and its children in the hierarchy tree. */
        void drawHierarchyNode(const Uuid& entityId);

        EditorContext context_;
        std::unique_ptr<EditorUi> ui_;
        std::unique_ptr<EditorViewport> viewport_;
        GizmoMode gizmoMode_ = GizmoMode::Translate;
        int frameLimit_ = 0;
        int framesRendered_ = 0;
    };
}
