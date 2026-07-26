#include "CnaTamagotchi/Application/CnaTamagotchiGame.hpp"

#include <string_view>

int main(const int argc, char* argv[])
{
    const bool smokeTest = argc == 2 && std::string_view(argv[1]) == "--smoke-test";

    CnaTamagotchi::Application::CnaTamagotchiGame game(smokeTest);
    game.Run();
    return 0;
}
