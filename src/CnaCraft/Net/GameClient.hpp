#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "LineSocket.hpp"
#include "Protocol.hpp"
#include "System/Collections/Concurrent/ConcurrentQueue.hpp"
#include "System/Threading/Thread.hpp"

// The game-side network client (MULTIPLAYER_PLAN.md §5): owns the TCP
// connection to a CnaCraftServer, a background reader thread that parses
// inbound lines into ServerMessages, and a thread-safe inbox the game's
// main loop drains once per frame (CnaCraftGame::PollNetworkMessages).
//
// Deliberate departure from Craft: a lost/failed connection never exits the
// process (Craft's client.c calls exit(1) from its recv thread). Instead
// IsDropped() flips and the game surfaces a HUD message and falls back to
// offline behavior -- a documented improvement, not drift.
//
// Threading: exactly one internal reader thread. Send() may be called from
// the main thread at any time (serialized inside LineSocket). Stop() is
// idempotent, wakes the reader via shutdown(), JOINS it, then closes --
// sharp-runtime's Thread only detaches in its destructor, so the explicit
// join is load-bearing (NEXT.md-class constraint, MULTIPLAYER_PLAN.md §12).
namespace CnaCraft::Net {

class GameClient {
public:
    GameClient() = default;
    ~GameClient() { Stop(); }

    GameClient(const GameClient&) = delete;
    GameClient& operator=(const GameClient&) = delete;

    // Connects and starts the reader thread. Returns false (with LastError
    // set) instead of throwing -- a bad --server argument or a server that
    // isn't up yet must not kill the game.
    bool Connect(const std::string& host, int port);

    // Idempotent teardown: wake reader, join it, close the socket.
    void Stop();

    [[nodiscard]] bool IsConnected() const { return connected_ && !dropped_; }

    // True once the reader observed the connection ending (server gone,
    // network error, or a failed Send). The game polls this each frame.
    [[nodiscard]] bool IsDropped() const { return dropped_; }

    [[nodiscard]] const std::string& LastError() const { return lastError_; }

    // Serializes and sends one message. On failure marks the connection
    // dropped and returns false.
    bool Send(const ClientMessage& msg);

    // Non-blocking; the main loop's per-frame poller. Returns false when
    // the inbox is empty.
    bool TryReceive(ServerMessage& out);

private:
    void ReaderLoop();

    std::shared_ptr<System::Net::Sockets::Socket> socket_;
    std::unique_ptr<LineSocket> line_;
    std::unique_ptr<System::Threading::Thread> readerThread_;
    System::Collections::Concurrent::ConcurrentQueue<ServerMessage> inbox_;
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};
    std::atomic<bool> dropped_{false};
    std::string lastError_;
};

}
