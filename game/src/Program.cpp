#include "CopperBoots/CopperBootsGame.hpp"

#include <iostream>
#include <string_view>

int main(const int argc, char* argv[])
{
    const bool smokeTest = argc == 2 &&
        std::string_view(argv[1]) == "--smoke-test";

    CopperBoots::CopperBootsGame game(smokeTest);
    game.Run();

    if (smokeTest)
        std::cout << "Copper Boots: smoke test completed\n";
    return 0;
}

