// SPDX-License-Identifier: MS-PL
/**
 * @file Main.cpp
 * @brief The cna-player entry point.
 *
 * cna-player runs a CNA game under the editor's control, in its own process. It is built **once
 * per graphics backend** -- `cna-player-easygl`, `cna-player-software`, `cna-player-d3d11` and so
 * on -- because CNA fixes its backend at compile time (ANALYSIS.md finding F-01). The editor
 * discovers which builds are installed and launches the one matching the backend the user asked
 * to preview.
 *
 * That is also why `--graphics` is a *player* option and not an editor one: here it names which
 * binary should have been launched, and the player verifies that the build it is running actually
 * provides it rather than silently rendering with something else.
 */

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include "CNA/Editor/Player/PlayerHost.hpp"
#include "CNA/Editor/RuntimeBridge/MessageChannel.hpp"

#if defined(CNA_EDITOR_HAS_CNA)
#    include "CNA/GraphicsBackendType.hpp"
#endif

namespace
{
    struct PlayerOptions
    {
        std::string projectPath;
        std::string scenePath;
        std::string requestedBackend;
        std::uint16_t editorPort = 0;
        int frameLimit = 0;
        bool headless = false;
        bool showHelp = false;
        bool hasError = false;
        std::string errorMessage;
    };

    bool splitOption(std::string_view argument, std::string& name, std::string& value)
    {
        const std::size_t equals = argument.find('=');
        if (equals == std::string_view::npos) { return false; }
        name = std::string{argument.substr(0, equals)};
        value = std::string{argument.substr(equals + 1)};
        return true;
    }

    PlayerOptions parseOptions(int argc, char** argv)
    {
        PlayerOptions options;

        for (int index = 1; index < argc; ++index)
        {
            const std::string_view argument{argv[index] != nullptr ? argv[index] : ""};
            if (argument.empty()) { continue; }

            if (argument == "--help" || argument == "-h") { options.showHelp = true; continue; }
            if (argument == "--headless") { options.headless = true; continue; }

            std::string name;
            std::string value;
            if (splitOption(argument, name, value))
            {
                if (name == "--project") { options.projectPath = value; continue; }
                if (name == "--scene") { options.scenePath = value; continue; }
                if (name == "--graphics") { options.requestedBackend = value; continue; }
                if (name == "--editor-port")
                {
                    try { options.editorPort = static_cast<std::uint16_t>(std::stoi(value)); }
                    catch (const std::exception&)
                    {
                        options.hasError = true;
                        options.errorMessage = "--editor-port expects a number, got '" + value + "'";
                    }
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
            }

            options.hasError = true;
            options.errorMessage = "unknown option '" + std::string{argument} + "'";
        }

        if (!options.hasError && options.projectPath.empty())
        {
            options.hasError = true;
            options.errorMessage = "--project is required";
        }
        return options;
    }

    const char* usage()
    {
        return
            "cna-player -- runs a CNA game under cna-editor's control\n"
            "\n"
            "Usage:\n"
            "  cna-player --project=PATH [options]\n"
            "\n"
            "Options:\n"
            "  --project=PATH       The .cnaproject to run. Required.\n"
            "  --scene=PATH         Project-relative scene to load, overriding the startup scene.\n"
            "  --graphics=NAME      The backend this build is expected to provide. Verified, not\n"
            "                       selected: CNA fixes its backend at compile time, so each\n"
            "                       cna-player binary supports exactly one.\n"
            "  --editor-port=N      Connect to cna-editor on 127.0.0.1:N. Without it the player\n"
            "                       runs standalone, with no bridge.\n"
            "  --frames=N           Exit after N frames. Used by tests.\n"
            "  --headless           Run with no window.\n"
            "  -h, --help           Print this help and exit.\n";
    }

    /** @brief Returns the CNA backend this binary was compiled against. */
    std::string compiledBackendName()
    {
#if defined(CNA_EDITOR_HAS_CNA)
        return std::string{CNA::getCurrentGraphicsBackendName()};
#else
        // Built without CNA: the protocol and state machine are exercised, nothing is drawn.
        // This is the configuration the editor's own tests run against.
        return "NONE";
#endif
    }
}

int main(int argc, char** argv)
{
    const PlayerOptions options = parseOptions(argc, argv);

    if (options.showHelp) { std::cout << usage(); return 0; }
    if (options.hasError)
    {
        std::cerr << "cna-player: " << options.errorMessage << "\n\n" << usage();
        return 2;
    }

    const std::string backend = compiledBackendName();

    // Verified, not selected. A mismatch means the editor launched the wrong binary, and saying so
    // is far better than rendering with a backend the user did not ask for and never being told.
    if (!options.requestedBackend.empty())
    {
        std::string requestedUpper = options.requestedBackend;
        for (char& character : requestedUpper)
        {
            character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
        }
        if (requestedUpper != backend)
        {
            std::cerr << "cna-player: this build provides the " << backend << " backend, but "
                      << options.requestedBackend << " was requested. CNA selects its graphics "
                         "backend at compile time, so run the cna-player build for that backend "
                         "instead.\n";
            return 3;
        }
    }

    CNA::Editor::PlayerHost host;
    if (!host.openProject(options.projectPath))
    {
        std::cerr << "cna-player: " << host.getError() << "\n";
        return 1;
    }

    std::cout << "cna-player: project '" << host.getProject().getName() << "', backend " << backend
              << ", scene '" << host.getScene().getName() << "' ("
              << host.getScene().getEntityCount() << " entities)\n";

    CNA::Editor::MessageChannel channel;
    if (options.editorPort != 0)
    {
        if (!channel.connect(options.editorPort))
        {
            std::cerr << "cna-player: " << channel.getError() << "\n";
            return 4;
        }
    }

    bool announced = false;
    int framesRun = 0;

    while (!host.shouldExit())
    {
        if (options.editorPort != 0)
        {
            const std::vector<CNA::Editor::EditorMessage> incoming = channel.poll();

            if (channel.isConnected() && !announced)
            {
                channel.send(host.makeReady(backend));
                announced = true;
            }

            CNA::Editor::PlayerHost::Outbox outbox;
            for (const CNA::Editor::EditorMessage& message : incoming) { host.handle(message, outbox); }
            for (const CNA::Editor::EditorMessage& reply : outbox) { channel.send(reply); }

            if (channel.getState() == CNA::Editor::ChannelState::Failed)
            {
                // The editor went away. A player that kept running would be an orphan window the
                // user has to hunt down and close.
                std::cout << "cna-player: editor disconnected (" << channel.getError() << "); exiting\n";
                break;
            }
        }

        if (host.tick())
        {
            // Where the CNA-linked build draws a frame. Without CNA this is the protocol and
            // state machine running on their own, which is exactly what the tests exercise.
            ++framesRun;
            if (options.frameLimit > 0 && framesRun >= options.frameLimit) { break; }
        }
        else if (options.frameLimit > 0 && host.getPlayState() == CNA::Editor::PlayState::Paused)
        {
            // Paused with a frame limit set means a test is driving this; do not spin forever
            // waiting for a step that will never come.
            break;
        }

        // Idling rather than spinning: a paused player must not burn a core while the user reads
        // the inspector.
        std::this_thread::sleep_for(std::chrono::milliseconds(
            host.getPlayState() == CNA::Editor::PlayState::Paused ? 16 : 1));
    }

    std::cout << "cna-player: ran " << host.getFrameCount() << " frames\n";
    return 0;
}
