#include <string>

#include "CnaCraftGame.hpp"

int main(int argc, char* argv[]) {
    CnaCraft::CnaCraftGame game;

    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--smoke" && i + 1 < argc) {
            game.SetSmokeFrames(std::stoi(argv[++i]));
        } else if (std::string(argv[i]) == "--smoke") {
            game.SetSmokeFrames(3);
        }
    }

    game.Run();
    return 0;
}
