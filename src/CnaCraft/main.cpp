#include <string>

#include "CnaCraftGame.hpp"
#include "Net/Protocol.hpp"

int main(int argc, char* argv[]) {
    CnaCraft::CnaCraftGame game;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--smoke" && i + 1 < argc) {
            game.SetSmokeFrames(std::stoi(argv[++i]));
        } else if (arg == "--smoke") {
            game.SetSmokeFrames(3);
        } else if (arg == "--server" && i + 1 < argc) {
            // Multiplayer client mode (MULTIPLAYER_PLAN.md §5):
            // --server HOST [PORT], Craft's own argv shape. Port defaults
            // to Craft's classic 4080.
            const std::string host = argv[++i];
            int port = CnaCraft::Net::kDefaultPort;
            if (i + 1 < argc && argv[i + 1][0] != '-') port = std::stoi(argv[++i]);
            game.SetServer(host, port);
        }
    }

    game.Run();
    return 0;
}
