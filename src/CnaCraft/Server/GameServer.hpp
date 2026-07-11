#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../Net/LineSocket.hpp"
#include "../Net/Protocol.hpp"
#include "../Persistence/WorldStore.hpp"
#include "../Worlds/World.hpp"
#include "System/Collections/Concurrent/ConcurrentQueue.hpp"
#include "System/Net/Sockets/Socket.hpp"
#include "System/Threading/Thread.hpp"

// CnaCraftServer (MULTIPLAYER_PLAN.md §6) -- the authoritative game server,
// a direct architectural port of Craft's server.py:
//
//   - one ACCEPT thread blocking on Accept();
//   - one SERVICE thread per client that both drains inbound lines into the
//     model's command queue and flushes the client's outbox (single thread
//     per client instead of server.py's reader+writer pair -- the 20ms poll
//     bounds outbound latency far below the 100ms position cadence);
//   - one MODEL thread owning ALL world/player state and (from M3) the
//     SQLite store, so world access is serialized exactly like server.py's
//     Model thread -- no fine-grained locking anywhere.
//
// Lifetime rule (load-bearing): a Client object is NEVER destroyed while
// the server runs -- dead clients move to zombies_ and are joined+freed in
// Stop(). Per-client threads hold raw Client pointers; destroying a Client
// early would leave a detached thread (sharp-runtime Thread detaches in its
// destructor) touching freed memory. The cost is a few hundred bytes per
// past connection for the process lifetime -- the same order as server.py's
// own lingering handler threads.
namespace CnaCraft::Server {

struct ServerConfig {
    int port = Net::kDefaultPort;  // 0 = ephemeral (tests); Port() reports the real one
    std::uint32_t seed = 1337;     // sent to every client via W (MULTIPLAYER_PLAN.md §4)
    float dayLengthSeconds = 600.0f;
    std::string dbPath = "server_world.db";  // the authoritative edit store (Craft's craft.db role)
};

class GameServer {
public:
    explicit GameServer(ServerConfig config);
    ~GameServer();

    GameServer(const GameServer&) = delete;
    GameServer& operator=(const GameServer&) = delete;

    // Binds, listens, and starts the accept + model threads. Returns false
    // (with LastError set) if the port can't be bound.
    bool Start();
    void Stop();  // idempotent; joins every thread

    [[nodiscard]] bool IsRunning() const { return running_; }
    [[nodiscard]] int Port() const { return port_; }
    [[nodiscard]] const std::string& LastError() const { return lastError_; }

private:
    struct Client {
        int id = 0;
        std::shared_ptr<System::Net::Sockets::Socket> socket;
        std::unique_ptr<Net::LineSocket> line;
        std::unique_ptr<System::Threading::Thread> serviceThread;
        System::Collections::Concurrent::ConcurrentQueue<std::string> outbox;
        std::atomic<bool> alive{true};

        // Model-thread-owned state (never touched by service threads):
        bool versionOk = false;
        std::string nick;
        float x = 0, y = 0, z = 0, rx = 0, ry = 0;
        bool hasPosition = false;
    };

    struct Command {
        enum class Kind { Join, Line, Leave };
        Kind kind = Kind::Line;
        int clientId = 0;
        std::shared_ptr<Client> client;  // Join only
        Net::ClientMessage msg;          // Line only
    };

    void AcceptLoop();
    void ServiceLoop(Client* client);
    void ModelLoop();

    // --- model-thread-only helpers ---
    void HandleJoin(const std::shared_ptr<Client>& client);
    void HandleLeave(int clientId);
    void HandleMessage(Client& client, const Net::ClientMessage& msg);
    void HandleTalk(Client& client, const std::string& text);
    void HandleChunkRequest(Client& client, int p, int q, std::int64_t sinceKey);
    void HandleBlock(Client& client, int x, int y, int z, int w);
    void HandleLight(Client& client, int x, int y, int z, int w);
    void HandleSign(Client& client, int x, int y, int z, int face, const std::string& text);
    // Generates + overlays stored edits/lights for the column holding an
    // edit, so validation sees the same terrain the editing client does
    // (the whole reason the server reuses Worlds/ -- MULTIPLAYER_PLAN.md §6).
    void EnsureColumnLoaded(int cx, int cz);
    void RejectEdit(Client& client, int x, int y, int z, const std::string& reason);
    void SendTo(Client& client, const Net::ServerMessage& msg);
    void Broadcast(const Net::ServerMessage& msg, int exceptId = -1);
    double ElapsedSeconds() const;

    ServerConfig config_;
    int port_ = 0;
    std::string lastError_;
    std::atomic<bool> running_{false};

    std::unique_ptr<System::Net::Sockets::Socket> listener_;
    std::unique_ptr<System::Threading::Thread> acceptThread_;
    std::unique_ptr<System::Threading::Thread> modelThread_;

    System::Collections::Concurrent::ConcurrentQueue<Command> commands_;

    // Model-thread-owned (except Stop(), which runs after the model joined):
    // the authoritative world -- generated on demand per column, edits
    // overlaid from the store, never unloaded (bounded by explored area;
    // ~32 KB per column, fine for a LAN server's lifetime).
    Worlds::World world_;
    std::unique_ptr<Persistence::WorldStore> store_;
    std::unordered_map<int, std::shared_ptr<Client>> clients_;
    std::vector<std::shared_ptr<Client>> zombies_;
    // Atomic and assigned in the ACCEPT thread, before the client's service
    // thread starts: the service thread stamps client->id into every Line
    // command, so the id must exist before the first line can be read. (An
    // earlier version assigned it in HandleJoin on the model thread -- the
    // service thread then raced ahead with id 0 and every early message,
    // including the version handshake, was silently dropped.)
    std::atomic<int> nextClientId_{1};

    std::int64_t startTicks_ = 0;  // steady-clock epoch for the shared world clock
};

}
