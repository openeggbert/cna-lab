#include "BlackPineWorld.hpp"

#include "explore2d/CnaGame.hpp"

#include <cstddef>
#include <iostream>
#include <string_view>

int main(const int argc, const char* const argv[]) {
    bool smoke = false;
    if (argc == 2 && std::string_view{argv[1]} == "--smoke-test") smoke = true;
    else if (argc != 1) {
        std::cerr << "Usage: black-pine [--smoke-test]\n";
        return 2;
    }

    explore2d::cna::HostConfig host;
    host.windowTitle = "Black Pine: The Long Silence";
    host.presentationScale = 2;
    host.savePath = "black-pine.e2dsave";
    host.settingsPath = "black-pine.e2dsettings";
    host.exitAfterFrames = smoke ? 2U : 0U;

    explore2d::SessionConfig sessionConfig;
    sessionConfig.turnBeforeWalk = true;
    sessionConfig.walkStep = 10.0F;

    explore2d::cna::AdventureGame game{
        black_pine::buildWorld(),
        sessionConfig,
        black_pine::buildTheme(),
        host};
    game.Run();
    return 0;
}
