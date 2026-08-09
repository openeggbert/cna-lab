#include "IronGang/Application/IronGangGame.hpp"

#include <exception>
#include <iostream>
#include <string>

#ifndef IRON_GANG_DEFAULT_ASSET_DIR
#define IRON_GANG_DEFAULT_ASSET_DIR "assets"
#endif

int main(int argc, char* argv[])
{
    try
    {
        std::string assetRoot = IRON_GANG_DEFAULT_ASSET_DIR;
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
            else if (argument == "--help" || argument == "-h")
            {
                std::cout
                    << "Iron Gang prototype\n"
                    << "  --assets <path>  Override the source asset root\n"
                    << "  --smoke [frames] Exit after a bounded number of draw frames\n";
                return 0;
            }
        }

        IronGang::IronGangGame game(assetRoot);
        game.SetSmokeFrames(smokeFrames);
        game.Run();
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Iron Gang terminated: " << exception.what() << '\n';
        return 1;
    }
}
