#include "IronGang/Application/IronGangGame.hpp"

#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#ifndef IRON_GANG_DEFAULT_ASSET_DIR
#define IRON_GANG_DEFAULT_ASSET_DIR "assets"
#endif

int main(int argc, char* argv[])
{
    try
    {
        std::string assetRoot = IRON_GANG_DEFAULT_ASSET_DIR;
        std::string profilePath;
        std::optional<IronGang::PerformanceScenario> profileScenario;
        bool verticalSync = true;
        int smokeFrames = -1;

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
            else if (argument == "--profile-scenario" && index + 1 < argc)
            {
                const std::string scenario = argv[++index];
                profileScenario = IronGang::ParsePerformanceScenario(scenario);
                if (!profileScenario)
                {
                    throw std::invalid_argument(
                        "--profile-scenario must be one of: intro, idle, walk, drive, mixed");
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
            else if (argument == "--help" || argument == "-h")
            {
                std::cout
                    << "Iron Gang prototype\n"
                    << "  --assets <path>  Override the source asset root\n"
                    << "  --smoke [frames] Exit after a bounded number of draw frames\n"
                    << "  --profile <path> Write an M12 JSON performance report on exit\n"
                    << "  --profile-scenario <name>  intro, idle, walk, drive, or mixed\n"
                    << "  --vsync on|off  Request synchronized or immediate presentation\n";
                return 0;
            }
            else if (argument == "--profile")
            {
                throw std::invalid_argument("--profile requires an output path");
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
        game.SetSmokeFrames(smokeFrames);
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
