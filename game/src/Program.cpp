#include "CopperBoots/CopperBootsGame.hpp"

#include <iostream>
#include <string>
#include <string_view>

int main(const int argc, char* argv[])
{
    bool smokeTest = false;
    bool audioEnabled = true;
    bool settingsEnabled = true;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--smoke-test")
            smokeTest = true;
        else if (argument == "--no-audio")
            audioEnabled = false;
        else if (argument == "--no-settings")
            settingsEnabled = false;
        else {
            std::cerr << "Unknown argument: " << argument << '\n'
                      << "Usage: " << argv[0]
                      << " [--smoke-test] [--no-audio] [--no-settings]\n";
            return 2;
        }
    }

    CopperBoots::CopperBootsGame game(
        smokeTest, audioEnabled, settingsEnabled);
    game.Run();

    if (smokeTest)
        std::cout << "Copper Boots: smoke test completed\n";
    return 0;
}
