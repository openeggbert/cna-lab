// SPDX-License-Identifier: MS-PL
/**
 * @file Main.cpp
 * @brief The cna-editor entry point.
 *
 * Only this file decides which concrete EditorUi and EditorViewport the application gets. Keeping
 * that decision in exactly one place is what lets the same EditorApplication run under a real
 * toolkit, under the null UI in CI, and under a future Qt implementation.
 */

#include <iostream>
#include <memory>

#include "CNA/Editor/EditorApplication.hpp"

#if defined(CNA_EDITOR_HAS_IMGUI)
#    include "CNA/Editor/Ui/ImGuiEditorUi.hpp"
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

    if (useImGui)
    {
        // Presenting the geometry needs a window and a CNA graphics device, which is
        // cna-editor-viewport's job (plan.md ED-111). Until that is wired, the ImGui UI runs and
        // produces real draw data -- which is exactly what the headless tests assert on -- but
        // nothing is on screen yet, and saying so plainly beats opening a blank window.
        std::cerr << "cna-editor: the ImGui UI is built, but window creation and presentation are "
                     "not wired up yet (plan.md ED-111).\n"
                     "The renderer that draws it through CNA's public API is implemented in "
                     "cna-editor-viewport; build with -DCNA_EDITOR_WITH_CNA=ON to compile it.\n"
                     "Run with --headless in the meantime.\n";
        return 3;
    }
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
