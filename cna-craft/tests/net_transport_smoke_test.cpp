// Plain-assert smoke test for CnaCraft::Net's transport layer (LineSocket +
// GameClient) over a real loopback TCP socket -- MULTIPLAYER_PLAN.md phase M1.
// Follows sharp-runtime's own SocketTests loopback-thread pattern. Needs
// SHARP_RUNTIME, so it builds only in the game-enabled configuration.

#include <chrono>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "CnaCraft/Net/GameClient.hpp"
#include "CnaCraft/Net/LineSocket.hpp"
#include "CnaCraft/Net/Protocol.hpp"
#include "System/Net/IPAddress.hpp"
#include "System/Net/IPEndPoint.hpp"
#include "System/Net/Sockets/AddressFamily.hpp"
#include "System/Net/Sockets/ProtocolType.hpp"
#include "System/Net/Sockets/Socket.hpp"
#include "System/Net/Sockets/SocketType.hpp"

using namespace CnaCraft::Net;
using namespace System::Net;
using namespace System::Net::Sockets;

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

bool WaitFor(const std::function<bool()>& predicate, int timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return predicate();
}

// A tiny scripted in-process server: accepts exactly one client on a
// loopback ephemeral port and hands the accepted socket to the test body.
struct LoopbackServer {
    Socket listener{AddressFamily::InterNetwork, SocketType::Stream, ProtocolType::Tcp};
    int port = 0;
    std::shared_ptr<Socket> accepted;
    std::thread acceptThread;

    LoopbackServer() {
        listener.Bind(IPEndPoint(IPAddress::Loopback, 0));
        listener.Listen();
        auto local = listener.getLocalEndPointProperty();
        port = dynamic_cast<IPEndPoint*>(local.get())->getPortProperty();
        acceptThread = std::thread([this]() { accepted = listener.Accept(); });
    }

    ~LoopbackServer() {
        if (acceptThread.joinable()) acceptThread.join();
        try {
            listener.Close();
        } catch (...) {
        }
    }
};

void TestClientToServerLines() {
    LoopbackServer server;
    GameClient client;
    Check(client.Connect("127.0.0.1", server.port), "client connects to the loopback server");
    server.acceptThread.join();
    Check(server.accepted != nullptr, "server accepted the connection");

    Check(client.Send(MakeVersion(kProtocolVersion)), "client sends V");
    Check(client.Send(MakeTalk("hello, with commas")), "client sends T with commas");

    LineSocket serverLine(server.accepted);
    std::vector<std::string> received;
    WaitFor([&]() { return serverLine.ReadLines(received, 50'000) && received.size() >= 2; }, 2000);
    Check(received.size() == 2, "server received exactly two lines");
    if (received.size() == 2) {
        const auto v = ParseClientLine(received[0]);
        const auto t = ParseClientLine(received[1]);
        Check(v && v->op == ClientOpcode::Version && v->version == kProtocolVersion,
              "V arrived intact and parses");
        Check(t && t->op == ClientOpcode::Talk && t->text == "hello, with commas",
              "T arrived intact, commas preserved end-to-end");
    }
    client.Stop();
    Check(true, "client Stop() joined its reader thread (no hang)");
}

void TestServerToClientMessagesAndPartialLines() {
    LoopbackServer server;
    GameClient client;
    Check(client.Connect("127.0.0.1", server.port), "client connects (message-flow test)");
    server.acceptThread.join();

    LineSocket serverLine(server.accepted);
    // Two complete messages back-to-back...
    ServerMessage talk;
    talk.op = ServerOpcode::Talk;
    talk.text = "welcome, player";
    serverLine.SendLine(FormatServerLine(talk));
    ServerMessage nick;
    nick.op = ServerOpcode::Nick;
    nick.id = 3;
    nick.text = "guest3";
    serverLine.SendLine(FormatServerLine(nick));
    // ...then a message deliberately split across two raw sends, to prove
    // the client reassembles partial lines (Craft's own recv-queue model).
    const std::string split = "D,42\n";
    std::vector<SharpRuntime::bytecs> firstHalf(split.begin(), split.begin() + 2);
    std::vector<SharpRuntime::bytecs> secondHalf(split.begin() + 2, split.end());
    server.accepted->Send(firstHalf);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    server.accepted->Send(secondHalf);

    std::vector<ServerMessage> got;
    WaitFor(
        [&]() {
            ServerMessage m;
            while (client.TryReceive(m)) got.push_back(m);
            return got.size() >= 3;
        },
        2000);
    Check(got.size() == 3, "client received all three server messages");
    if (got.size() == 3) {
        Check(got[0].op == ServerOpcode::Talk && got[0].text == "welcome, player",
              "first message in order, text intact");
        Check(got[1].op == ServerOpcode::Nick && got[1].id == 3 && got[1].text == "guest3",
              "second message in order");
        Check(got[2].op == ServerOpcode::Disconnect && got[2].id == 42,
              "split-across-packets line reassembled correctly");
    }
    client.Stop();
}

void TestServerCloseMarksDropped() {
    LoopbackServer server;
    GameClient client;
    Check(client.Connect("127.0.0.1", server.port), "client connects (drop test)");
    server.acceptThread.join();

    Check(!client.IsDropped(), "client not dropped while the connection is up");
    server.accepted->Close();  // server goes away
    Check(WaitFor([&]() { return client.IsDropped(); }, 2000),
          "client marks the connection dropped after the server closes (no exit(1), unlike Craft)");
    Check(!client.IsConnected(), "IsConnected false after the drop");
    client.Stop();
}

void TestConnectFailureIsGraceful() {
    GameClient client;
    // Port 1 on loopback: virtually guaranteed closed -> immediate refusal.
    const bool ok = client.Connect("127.0.0.1", 1);
    Check(!ok, "connecting to a closed port fails gracefully (no throw, no exit)");
    Check(!client.LastError().empty(), "a failed connect records an error message");
    client.Stop();
    Check(true, "Stop() after a failed connect is a safe no-op");
}

}  // namespace

int main() {
    TestClientToServerLines();
    TestServerToClientMessagesAndPartialLines();
    TestServerCloseMarksDropped();
    TestConnectFailureIsGraceful();

    if (g_failures == 0) {
        std::printf("\nAll checks passed.\n");
        return 0;
    }
    std::fprintf(stderr, "\n%d check(s) FAILED.\n", g_failures);
    return 1;
}
