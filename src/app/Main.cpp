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

    // Phase 0 ships only the null/console UI. The Dear ImGui implementation registers here once
    // it exists (plan.md ED-110); everything above and below this line stays unchanged.
    if (!options.headless && options.uiBackend != "null")
    {
        std::cerr << "cna-editor: UI backend '" << options.uiBackend
                  << "' is not built into this binary yet (see plan.md ED-110).\n"
                     "Run with --headless to use the console UI.\n";
        return 3;
    }

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
