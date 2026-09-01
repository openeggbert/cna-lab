// CnaCraftServer -- standalone headless game server (MULTIPLAYER_PLAN.md §6).
// Usage: CnaCraftServer [port] [--seed N]
// Defaults: port 4080 (Craft's own), seed 1337 (the game's kWorldSeed).

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#include "GameServer.hpp"

namespace {
std::atomic<bool> g_stopRequested{false};
void OnSignal(int) { g_stopRequested = true; }
}

int main(int argc, char** argv) {
    CnaCraft::Server::ServerConfig config;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            config.seed = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        } else {
            config.port = std::atoi(argv[i]);
        }
    }

    CnaCraft::Server::GameServer server(config);
    if (!server.Start()) {
        std::fprintf(stderr, "CnaCraftServer: failed to start on port %d: %s\n", config.port,
                     server.LastError().c_str());
        return 1;
    }
    std::printf("CnaCraftServer listening on port %d (seed %u, day length %.0fs) -- Ctrl+C to stop.\n",
                server.Port(), config.seed, static_cast<double>(config.dayLengthSeconds));

    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);
    while (!g_stopRequested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    std::printf("CnaCraftServer: shutting down.\n");
    server.Stop();
    return 0;
}
