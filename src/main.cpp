#include "IronGang/Application/IronGangGame.hpp"
#include "IronGang/Core/Log.hpp"

#include <array>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>

#if defined(__linux__) && !defined(__EMSCRIPTEN__)
#include <unistd.h>
#endif

#ifndef IRON_GANG_DEFAULT_ASSET_DIR
#define IRON_GANG_DEFAULT_ASSET_DIR "assets"
#endif

namespace
{
#if !defined(__EMSCRIPTEN__)
    std::filesystem::path GetExecutablePath(const char* argumentZero)
    {
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
        std::array<char, 4096> pathBuffer{};
        const auto length = ::readlink("/proc/self/exe", pathBuffer.data(), pathBuffer.size());
        if (length > 0 && static_cast<std::size_t>(length) < pathBuffer.size())
        {
            return std::filesystem::path(
                std::string(pathBuffer.data(), static_cast<std::size_t>(length)));
        }
#endif
        if (argumentZero == nullptr || *argumentZero == '\0')
        {
            return {};
        }
        std::error_code error;
        const auto absolutePath = std::filesystem::absolute(argumentZero, error);
        return error ? std::filesystem::path(argumentZero) : absolutePath;
    }
#endif

    std::string ResolveDefaultAssetRoot(const char* argumentZero)
    {
#if defined(__EMSCRIPTEN__)
        (void)argumentZero;
        return IRON_GANG_DEFAULT_ASSET_DIR;
#else
        const auto executablePath = GetExecutablePath(argumentZero);
        if (!executablePath.empty())
        {
            const auto executableDirectory = executablePath.parent_path();
            const std::array candidates{
                executableDirectory / ".." / "share" / "iron-gang" / "assets",
                executableDirectory / ".." / "assets",
            };
            for (const auto& candidate : candidates)
            {
                std::error_code error;
                if (std::filesystem::is_directory(candidate, error) && !error)
                {
                    return candidate.lexically_normal().string();
                }
            }
        }
        return IRON_GANG_DEFAULT_ASSET_DIR;
#endif
    }
}

int main(int argc, char* argv[])
{
    try
    {
        std::string assetRoot = ResolveDefaultAssetRoot(argc > 0 ? argv[0] : nullptr);
        std::string profilePath;
        std::optional<IronGang::PerformanceScenario> profileScenario;
        bool verticalSync = true;
        int smokeFrames = -1;
        std::string screenshotPath;
        int screenshotFrame = 1;
        std::optional<IronGang::LogSeverity> logSeverity;

        for (int index = 1; index < argc; ++index)
        {
            const std::string argument = argv[index];
            if (argument == "--assets" && index + 1 < argc)
            {
                assetRoot = argv[++index];
            }
            else if (argument == "--smoke")
            {
                smokeFrames = 3;
                if (index + 1 < argc && std::string(argv[index + 1]).find_first_not_of("0123456789") == std::string::npos)
                {
                    smokeFrames = std::stoi(argv[++index]);
                }
            }
            else if (argument == "--profile" && index + 1 < argc)
            {
                profilePath = argv[++index];
            }
            else if (argument == "--screenshot" && index + 1 < argc)
            {
                screenshotPath = argv[++index];
            }
            else if (argument == "--screenshot-frame" && index + 1 < argc)
            {
                const std::string value = argv[++index];
                if (value.find_first_not_of("0123456789") != std::string::npos || value.empty())
                {
                    throw std::invalid_argument("--screenshot-frame requires a positive frame number");
                }
                screenshotFrame = std::stoi(value);
                if (screenshotFrame < 1)
                {
                    throw std::invalid_argument("--screenshot-frame requires a positive frame number");
                }
            }
            else if (argument == "--profile-scenario" && index + 1 < argc)
            {
                const std::string scenario = argv[++index];
                profileScenario = IronGang::ParsePerformanceScenario(scenario);
                if (!profileScenario)
                {
                    throw std::invalid_argument(
                        "--profile-scenario must be one of: intro, idle, walk, drive, mixed, mission");
                }
            }
            else if (argument == "--vsync" && index + 1 < argc)
            {
                const std::string value = argv[++index];
                if (value != "on" && value != "off")
                {
                    throw std::invalid_argument("--vsync must be 'on' or 'off'");
                }
                verticalSync = value == "on";
            }
            else if (argument == "--log-level" && index + 1 < argc)
            {
                const std::string value = argv[++index];
                IronGang::LogSeverity parsed{};
                if (!IronGang::ParseLogSeverity(value, parsed))
                {
                    throw std::invalid_argument("--log-level must be debug, info, warning, or error");
                }
                logSeverity = parsed;
            }
            else if (argument == "--log-level")
            {
                throw std::invalid_argument("--log-level requires debug, info, warning, or error");
            }
            else if (argument == "--help" || argument == "-h")
            {
                std::cout
                    << "Iron Gang prototype\n"
                    << "  --assets <path>  Override the source asset root\n"
                    << "  --smoke [frames] Exit after a bounded number of draw frames\n"
                    << "  --profile <path> Write an M12 JSON performance report on exit\n"
                    << "  --profile-scenario <name>  intro, idle, walk, drive, mixed, or mission\n"
                    << "  --screenshot <path>  Write one frame as a PNG (plus a summary sidecar)\n"
                    << "  --screenshot-frame <n>  Which draw frame to capture (1-based, default 1)\n"
                    << "  --vsync on|off  Request synchronized or immediate presentation\n"
                    << "  --log-level <name>  debug, info, warning, or error (overrides game.json)\n";
                return 0;
            }
            else if (argument == "--profile")
            {
                throw std::invalid_argument("--profile requires an output path");
            }
            else if (argument == "--screenshot")
            {
                throw std::invalid_argument("--screenshot requires an output path");
            }
            else if (argument == "--screenshot-frame")
            {
                throw std::invalid_argument("--screenshot-frame requires a positive frame number");
            }
            else if (argument == "--profile-scenario")
            {
                throw std::invalid_argument("--profile-scenario requires a scenario name");
            }
            else if (argument == "--vsync")
            {
                throw std::invalid_argument("--vsync requires 'on' or 'off'");
            }
        }

        IronGang::IronGangGame game(assetRoot);
        if (logSeverity)
        {
            game.SetLogSeverityOverride(*logSeverity);
        }
        game.SetSmokeFrames(smokeFrames);
        if (!screenshotPath.empty())
        {
            game.RequestScreenshot(screenshotPath, screenshotFrame);
        }
        else if (screenshotFrame != 1)
        {
            throw std::invalid_argument("--screenshot-frame requires --screenshot");
        }
        game.SetVerticalSync(verticalSync);
        if (!profilePath.empty())
        {
            game.EnablePerformanceProfile(profilePath);
        }
        if (profileScenario)
        {
            if (profilePath.empty())
            {
                throw std::invalid_argument("--profile-scenario requires --profile");
            }
            game.SetPerformanceScenario(*profileScenario);
        }
        game.Run();
        std::string reportError;
        if (!game.WritePerformanceReport(reportError))
        {
            std::cerr << reportError << '\n';
            return 1;
        }
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Iron Gang terminated: " << exception.what() << '\n';
        return 1;
    }
}
