// Plain-assert smoke test for CnaCraft::Server::GameServer -- MULTIPLAYER_PLAN.md
// phase M2: two real GameClients against an in-process server over loopback
// TCP. Handshake (U/E/W/T), presence (N/P relay/D), chat, /nick, /list, and
// the version-mismatch rejection path.

#include <chrono>
#include <cstdio>
#include <functional>
#include <string>
#include <thread>

#include "CnaCraft/Net/GameClient.hpp"
#include "CnaCraft/Net/Protocol.hpp"
#include "CnaCraft/Server/GameServer.hpp"

using namespace CnaCraft::Net;
using namespace CnaCraft::Server;

namespace {

int g_failures = 0;

void Check(bool condition, const char* label) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", label);
        ++g_failures;
    } else {
        std::printf("ok:   %s\n", label);
    }
}

// Drains the client's inbox until a message matching `pred` arrives (stored
// in `out`) or the deadline passes. Non-matching messages are discarded --
// each wait targets one expected message.
bool WaitForMessage(GameClient& client, const std::function<bool(const ServerMessage&)>& pred, ServerMessage& out,
                    int timeoutMs = 2000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        ServerMessage m;
        while (client.TryReceive(m)) {
            if (pred(m)) {
                out = m;
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}

bool WaitFor(const std::function<bool()>& predicate, int timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return predicate();
}

void RunAll() {
    ServerConfig config;
    config.port = 0;  // ephemeral
    config.seed = 424242;
    GameServer server(config);
    Check(server.Start(), "server starts on an ephemeral port");
    Check(server.Port() > 0, "server reports its real port");

    // --- Client 1: handshake ---
    GameClient c1;
    Check(c1.Connect("127.0.0.1", server.Port()), "client 1 connects");
    c1.Send(MakeVersion(kProtocolVersion));
    c1.Send(MakeNick(""));  // guest

    ServerMessage msg;
    Check(WaitForMessage(c1, [](const ServerMessage& m) { return m.op == ServerOpcode::You; }, msg),
          "client 1 receives U (your id)");
    const int id1 = msg.id;
    Check(id1 > 0, "assigned id is positive");
    Check(WaitForMessage(c1, [](const ServerMessage& m) { return m.op == ServerOpcode::Time; }, msg),
          "client 1 receives E (clock)");
    Check(msg.dayLength == 600.0f && msg.elapsed >= 199.0, "E carries day length 600 and the mid-morning offset");
    Check(WaitForMessage(c1, [](const ServerMessage& m) { return m.op == ServerOpcode::WorldInfo; }, msg),
          "client 1 receives W (terrain seed)");
    Check(msg.seed == 424242u, "W carries the server's configured seed");
    Check(WaitForMessage(c1, [](const ServerMessage& m) { return m.op == ServerOpcode::Talk; }, msg),
          "client 1 receives the welcome message");

    // --- Client 2 joins; presence flows both ways ---
    GameClient c2;
    Check(c2.Connect("127.0.0.1", server.Port()), "client 2 connects");
    c2.Send(MakeVersion(kProtocolVersion));
    c2.Send(MakeNick("karel"));

    Check(WaitForMessage(
              c1, [](const ServerMessage& m) { return m.op == ServerOpcode::Nick && m.text == "karel"; }, msg),
          "client 1 sees client 2's nick change to karel");
    const int id2 = msg.id;
    Check(id2 > 0 && id2 != id1, "client 2 has a distinct id");
    Check(WaitForMessage(c2, [id1](const ServerMessage& m) { return m.op == ServerOpcode::Nick && m.id == id1; },
                         msg),
          "client 2 learned about client 1 at join");

    c2.Send(MakePosition(10.0f, 50.0f, 20.0f, 1.5f, -0.25f));
    Check(WaitForMessage(c1,
                         [id2](const ServerMessage& m) { return m.op == ServerOpcode::Position && m.id == id2; },
                         msg),
          "client 1 receives client 2's position");
    Check(msg.x > 9.99f && msg.x < 10.01f && msg.y > 49.99f && msg.y < 50.01f,
          "relayed position coordinates are intact");

    // --- Chat, /nick, /list ---
    c1.Send(MakeTalk("/nick robert"));
    Check(WaitForMessage(
              c2, [id1](const ServerMessage& m) { return m.op == ServerOpcode::Nick && m.id == id1; }, msg) &&
              msg.text == "robert",
          "/nick broadcasts the new name to everyone");

    c1.Send(MakeTalk("ahoj vsichni"));
    Check(WaitForMessage(c2,
                         [](const ServerMessage& m) {
                             return m.op == ServerOpcode::Talk && m.text == "robert> ahoj vsichni";
                         },
                         msg),
          "plain chat reaches the other client as 'nick> text'");
    Check(WaitForMessage(c1,
                         [](const ServerMessage& m) {
                             return m.op == ServerOpcode::Talk && m.text == "robert> ahoj vsichni";
                         },
                         msg),
          "the sender sees their own chat line too (Craft's send_talk behavior)");

    c1.Send(MakeTalk("/list"));
    Check(WaitForMessage(c1,
                         [](const ServerMessage& m) {
                             return m.op == ServerOpcode::Talk && m.text.find("robert") != std::string::npos &&
                                    m.text.find("karel") != std::string::npos;
                         },
                         msg),
          "/list names both players");

    // --- Disconnect propagates ---
    c2.Stop();
    Check(WaitForMessage(
              c1, [id2](const ServerMessage& m) { return m.op == ServerOpcode::Disconnect && m.id == id2; }, msg),
          "client 1 receives D when client 2 disconnects");
    Check(WaitForMessage(c1,
                         [](const ServerMessage& m) {
                             return m.op == ServerOpcode::Talk && m.text == "karel has disconnected.";
                         },
                         msg),
          "the disconnect is announced in chat");

    // --- Version mismatch is rejected ---
    GameClient c3;
    Check(c3.Connect("127.0.0.1", server.Port()), "client 3 connects (bad version)");
    c3.Send(MakeVersion(1));  // real Craft's version -- deliberately incompatible
    Check(WaitForMessage(c3,
                         [](const ServerMessage& m) {
                             return m.op == ServerOpcode::Talk &&
                                    m.text.find("version mismatch") != std::string::npos;
                         },
                         msg),
          "a version-1 (real Craft) client gets a readable rejection message");
    Check(WaitFor([&]() { return c3.IsDropped(); }, 2000), "the rejected client is disconnected");

    c1.Stop();
    c3.Stop();
    server.Stop();
    Check(true, "server Stop() joined all threads (no hang)");
}

}  // namespace

int main() {
    RunAll();

    if (g_failures == 0) {
        std::printf("\nAll checks passed.\n");
        return 0;
    }
    std::fprintf(stderr, "\n%d check(s) FAILED.\n", g_failures);
    return 1;
}
