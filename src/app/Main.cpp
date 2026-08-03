// SPDX-License-Identifier: MS-PL
/**
 * @file Main.cpp
 * @brief The cna-editor entry point.
 *
 * Only this file decides which concrete EditorUi and EditorViewport the application gets. Keeping
 * that decision in exactly one place is what lets the same EditorApplication run under a real
 * toolkit, under the null UI in CI, and under a future Qt implementation.
 */

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>

#include "CNA/Editor/EditorApplication.hpp"

#if defined(CNA_EDITOR_HAS_IMGUI)
#    include "CNA/Editor/Ui/ImGuiEditorUi.hpp"
#endif

// The window host needs both CNA and Dear ImGui. Without it the editor still runs headless, which
// is what CI and `--headless` use.
#if defined(CNA_EDITOR_HAS_HOST)
#    include "CNA/Editor/Viewport/CnaEditorHost.hpp"
#endif

namespace
{
    /** @brief Prints every log message to stdout. Used by headless runs. */
    class ConsoleEditorUi final : public CNA::Editor::NullEditorUi
    {
    public:
        void log(CNA::Editor::LogSeverity severity, const std::string& message) override
        {
            NullEditorUi::log(severity, message);
            std::ostream& stream =
                severity == CNA::Editor::LogSeverity::Error ? std::cerr : std::cout;
            stream << "[" << CNA::Editor::toString(severity) << "] " << message << "\n";
        }
    };

#if defined(CNA_EDITOR_HAS_HOST)
    /**
     * @brief Returns where ImGui's dock layout `.ini` should live.
     *
     * Under the user's config directory, not the project: where somebody puts their panels is a
     * property of the person, not of the game they happen to be editing.
     */
    std::string resolveLayoutPath()
    {
        const char* configHome = std::getenv("XDG_CONFIG_HOME");
        const char* home = std::getenv("HOME");
        const char* appData = std::getenv("APPDATA");

        std::filesystem::path base;
        if (configHome != nullptr && *configHome != '\0') { base = configHome; }
        else if (appData != nullptr && *appData != '\0') { base = appData; }
        else if (home != nullptr && *home != '\0') { base = std::filesystem::path{home} / ".config"; }
        else { return {}; }

        const std::filesystem::path directory = base / "cna-editor";
        std::error_code errorCode;
        std::filesystem::create_directories(directory, errorCode);
        if (errorCode) { return {}; }

        return (directory / "layout.ini").generic_string();
    }
#endif

    void printBackends()
    {
        std::cout << "CNA graphics backends known to this editor:\n\n";
        for (const CNA::Editor::BackendInfo& backend : CNA::Editor::getKnownBackends())
        {
            const char* support = "runtime-only  ";
            switch (backend.support)
            {
                case CNA::Editor::BackendEditorSupport::EditorSupported: support = "editor        "; break;
                case CNA::Editor::BackendEditorSupport::PreviewOnly: support = "preview-only  "; break;
                case CNA::Editor::BackendEditorSupport::RuntimeOnly: support = "runtime-only  "; break;
            }
            std::cout << "  " << support << backend.commandLineName << "  (" << backend.cmakeName << ")\n"
                      << "      " << backend.displayName << " -- " << backend.note << "\n";
        }
        std::cout << "\nThese are the backends a cna-player build can use. The editor's own backend\n"
                     "is fixed at compile time by CNA_GRAPHICS_BACKEND.\n";
    }
}

int main(int argc, char** argv)
{
    const CNA::Editor::EditorOptions options = CNA::Editor::EditorOptions::parse(argc, argv);

    if (options.hasError)
    {
        std::cerr << "cna-editor: " << options.errorMessage << "\n\n"
                  << CNA::Editor::EditorOptions::getUsage();
        return 2;
    }
    if (options.showHelp)
    {
        std::cout << CNA::Editor::EditorOptions::getUsage();
        return 0;
    }
    if (options.showVersion)
    {
        std::cout << "cna-editor " << CNA_EDITOR_VERSION << "\n";
        return 0;
    }
    if (options.listBackends)
    {
        printBackends();
        return 0;
    }

    // This is the one place that decides which concrete EditorUi and EditorViewport the
    // application gets. Everything else -- panels, commands, plugins -- is written against the
    // abstractions and does not change when this does (ANALYSIS.md decision D-02).
    const bool useImGui = !options.headless && options.uiBackend != "null";

#if !defined(CNA_EDITOR_HAS_IMGUI)
    if (useImGui)
    {
        std::cerr << "cna-editor: this binary was built with -DCNA_EDITOR_WITH_IMGUI=OFF, so the "
                     "'" << options.uiBackend << "' UI is unavailable.\n"
                     "Run with --headless to use the console UI.\n";
        return 3;
    }
#else
    if (useImGui && options.uiBackend != "imgui")
    {
        std::cerr << "cna-editor: unknown UI backend '" << options.uiBackend
                  << "'. This binary provides 'imgui' and 'null'.\n";
        return 3;
    }

#    if defined(CNA_EDITOR_HAS_HOST)
    if (useImGui)
    {
        auto application = std::make_unique<CNA::Editor::EditorApplication>(
            std::make_unique<CNA::Editor::ImGuiEditorUi>(),
            std::make_unique<CNA::Editor::NullEditorViewport>());

        if (!application->initialize(options)) { return 1; }

        CNA::Editor::CnaEditorHostOptions hostOptions;
        hostOptions.frameLimit = options.frameLimit;
        hostOptions.layoutPath = resolveLayoutPath();
        hostOptions.screenshotPath = options.screenshotPath;
        hostOptions.windowTitle = application->getContext().hasProject()
                                      ? "CNA Editor -- " + application->getContext().getProject().getName()
                                      : "CNA Editor";

        const CNA::Editor::CnaEditorHostResult result =
            CNA::Editor::runEditorInWindow(hostOptions, std::move(application));

        if (!result.errorMessage.empty()) { std::cerr << "cna-editor: " << result.errorMessage << "\n"; }

        if (!options.screenshotPath.empty() && !result.screenshotWritten)
        {
            std::cerr << "cna-editor: no screenshot was written to '" << options.screenshotPath
                      << "'. --screenshot needs --frames, and the backend must support reading "
                         "back its own back buffer.\n";
            return 4;
        }

        // Printed only for a scripted run. A window that opens, loops and closes having issued
        // zero draw calls looks identical to a working editor from the outside, so a smoke test
        // needs the numbers to assert on.
        if (options.frameLimit > 0)
        {
            std::cout << "cna-editor: backend " << result.backend << ", " << result.frames
                      << " frames, " << result.displayWidth << "x" << result.displayHeight
                      << " display, " << result.drawCalls << " draw calls, " << result.triangles
                      << " triangles, " << result.textures << " textures created, "
                      << result.textureUpdates << " texture updates, " << result.clippedAway
                      << " commands clipped away\n";
        }
        return result.exitCode;
    }
#    else
    if (useImGui)
    {
        // The ImGui UI is built and produces real geometry -- that is what the headless tests
        // assert on -- but presenting it needs a window and a CNA graphics device, which live in
        // cna-editor-viewport. Saying so plainly beats opening a blank window.
        std::cerr << "cna-editor: the ImGui UI is built, but this binary has no window host.\n"
                     "Rebuild with -DCNA_EDITOR_WITH_CNA=ON to get one, or run with --headless.\n";
        return 3;
    }
#    endif
#endif

    CNA::Editor::EditorApplication application{std::make_unique<ConsoleEditorUi>(),
                                               std::make_unique<CNA::Editor::NullEditorViewport>()};

    if (!application.initialize(options)) { return 1; }

    // The null UI never reports "the user closed the window", so an unbounded run() would spin
    // forever with nothing to look at. One frame is the useful default -- and is exactly what
    // makes `--headless` a usable smoke test. `--frames=N` overrides it.
    if (options.frameLimit <= 0)
    {
        application.renderFrame();
        return 0;
    }

    return application.run();
}
