// Plain-assert smoke test for multiplayer world sync -- MULTIPLAYER_PLAN.md
// phase M3: block/light/sign edits propagate between two real clients via
// GameServer, invalid edits revert with the authoritative value, chunk
// requests stream deltas incrementally by key (rowid), and everything
// survives a server restart on the same database.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "CnaCraft/Net/GameClient.hpp"
#include "CnaCraft/Net/Protocol.hpp"
#include "CnaCraft/Server/GameServer.hpp"
#include "CnaCraft/Worlds/BlockType.hpp"

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

constexpr const char* kTestDbPath = "worldsync_test_server.db";
constexpr int kStone = static_cast<int>(CnaCraft::Worlds::BlockType::Stone);
constexpr int kAir = 0;

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

// Collects a whole chunk response: everything up to and including the C
// (ChunkDone) marker for (p,q).
struct ChunkResponse {
    std::vector<ServerMessage> blocks;
    std::vector<ServerMessage> lights;
    std::vector<ServerMessage> signs;
    std::int64_t key = -1;  // -1 = no K received
    bool redraw = false;
    bool done = false;
};

ChunkResponse CollectChunkResponse(GameClient& client, int p, int q, int timeoutMs = 2000) {
    ChunkResponse response;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline && !response.done) {
        ServerMessage m;
        while (client.TryReceive(m)) {
            if (m.op == ServerOpcode::Block && m.p == p && m.q == q) response.blocks.push_back(m);
            if (m.op == ServerOpcode::Light && m.p == p && m.q == q) response.lights.push_back(m);
            if (m.op == ServerOpcode::Sign && m.p == p && m.q == q) response.signs.push_back(m);
            if (m.op == ServerOpcode::Key && m.p == p && m.q == q) response.key = m.key;
            if (m.op == ServerOpcode::Redraw && m.p == p && m.q == q) response.redraw = true;
            if (m.op == ServerOpcode::ChunkDone && m.p == p && m.q == q) {
                response.done = true;
                break;
            }
        }
        if (!response.done) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return response;
}

void Handshake(GameClient& client, const char* nick) {
    client.Send(MakeVersion(kProtocolVersion));
    client.Send(MakeNick(nick));
}

void RunAll() {
    std::remove(kTestDbPath);

    ServerConfig config;
    config.port = 0;
    config.seed = 77;
    config.dbPath = kTestDbPath;
    auto server = std::make_unique<GameServer>(config);
    Check(server->Start(), "server starts with a fresh world db");

    GameClient c1, c2;
    Check(c1.Connect("127.0.0.1", server->Port()), "client 1 connects");
    Check(c2.Connect("127.0.0.1", server->Port()), "client 2 connects");
    Handshake(c1, "alice");
    Handshake(c2, "bob");
    ServerMessage msg;
    Check(WaitForMessage(c1, [](const ServerMessage& m) { return m.op == ServerOpcode::WorldInfo; }, msg) &&
              msg.seed == 77u,
          "client 1 handshake completes (W seed 77)");
    Check(WaitForMessage(c2, [](const ServerMessage& m) { return m.op == ServerOpcode::WorldInfo; }, msg),
          "client 2 handshake completes");

    // --- An empty column's chunk response is just the done marker ---
    c1.Send(MakeChunkRequest(0, 0, 0));
    ChunkResponse empty = CollectChunkResponse(c1, 0, 0);
    Check(empty.done, "empty-column chunk response arrives (C marker)");
    Check(empty.blocks.empty() && empty.key == -1 && !empty.redraw,
          "empty column sends no blocks, no key, no redraw");

    // --- A valid edit propagates to the other client ---
    // y=60 is far above seed-77 terrain, guaranteed Air.
    c1.Send(MakeBlock(5, 60, 5, kStone));
    Check(WaitForMessage(c2,
                         [](const ServerMessage& m) {
                             return m.op == ServerOpcode::Block && m.bx == 5 && m.by == 60 && m.bz == 5;
                         },
                         msg),
          "client 2 receives client 1's block placement");
    Check(msg.w == kStone && msg.p == 0 && msg.q == 0, "the broadcast block carries type + column coords");
    Check(WaitForMessage(
              c2, [](const ServerMessage& m) { return m.op == ServerOpcode::Redraw && m.p == 0 && m.q == 0; },
              msg),
          "the broadcast is followed by R (redraw)");

    // --- Chunk requests stream the edit, incrementally by key ---
    c2.Send(MakeChunkRequest(0, 0, 0));
    ChunkResponse full = CollectChunkResponse(c2, 0, 0);
    Check(full.done && full.blocks.size() == 1 && full.blocks[0].bx == 5 && full.blocks[0].w == kStone,
          "a key-0 chunk request returns the stored edit");
    Check(full.key >= 1, "the response carries a K key (rowid) after the blocks");
    Check(full.redraw, "a non-empty chunk response ends with R before C");

    c2.Send(MakeChunkRequest(0, 0, full.key));
    ChunkResponse incremental = CollectChunkResponse(c2, 0, 0);
    Check(incremental.done && incremental.blocks.empty() && incremental.key == -1,
          "re-requesting with the returned key streams nothing new (incremental sync works)");

    // --- Invalid edits revert with the authoritative value ---
    c1.Send(MakeBlock(5, 60, 5, kStone));  // place onto the now-occupied cell
    Check(WaitForMessage(c1,
                         [](const ServerMessage& m) {
                             return m.op == ServerOpcode::Block && m.bx == 5 && m.by == 60 && m.bz == 5 &&
                                    m.w == kStone;
                         },
                         msg),
          "placing onto an occupied cell echoes the authoritative value back (revert)");
    Check(WaitForMessage(c1, [](const ServerMessage& m) { return m.op == ServerOpcode::Talk; }, msg) &&
              msg.text.find("already a block") != std::string::npos,
          "the revert comes with a readable reason");

    c1.Send(MakeBlock(7, 60, 7, kAir));  // break thin air
    Check(WaitForMessage(c1,
                         [](const ServerMessage& m) {
                             return m.op == ServerOpcode::Block && m.bx == 7 && m.by == 60 && m.bz == 7 &&
                                    m.w == kAir;
                         },
                         msg),
          "breaking empty space reverts with Air");

    c1.Send(MakeBlock(3, 0, 3, kAir));  // y=0: out of editable bounds (Craft's y>0 rule)
    Check(WaitForMessage(c1,
                         [](const ServerMessage& m) {
                             return m.op == ServerOpcode::Talk && m.text.find("height") != std::string::npos;
                         },
                         msg),
          "editing at y=0 is rejected with the height message");

    // --- Breaking the placed block propagates ---
    c1.Send(MakeBlock(5, 60, 5, kAir));
    Check(WaitForMessage(c2,
                         [](const ServerMessage& m) {
                             return m.op == ServerOpcode::Block && m.bx == 5 && m.by == 60 && m.bz == 5 &&
                                    m.w == kAir;
                         },
                         msg),
          "client 2 sees the block being broken");

    // --- Lights: valid on a solid block, revert on air ---
    c1.Send(MakeBlock(8, 60, 8, kStone));
    Check(WaitForMessage(c2,
                         [](const ServerMessage& m) { return m.op == ServerOpcode::Block && m.bx == 8; }, msg),
          "support block for the light propagates");
    c1.Send(MakeLight(8, 60, 8, 15));
    Check(WaitForMessage(c2,
                         [](const ServerMessage& m) {
                             return m.op == ServerOpcode::Light && m.bx == 8 && m.by == 60 && m.bz == 8 &&
                                    m.w == 15;
                         },
                         msg),
          "a light toggle on a solid block propagates");
    c1.Send(MakeLight(9, 60, 9, 15));  // air cell
    Check(WaitForMessage(c1,
                         [](const ServerMessage& m) {
                             return m.op == ServerOpcode::Light && m.bx == 9 && m.by == 60 && m.bz == 9 &&
                                    m.w == 0;
                         },
                         msg),
          "a light toggle on empty space reverts to off");

    // --- Signs: placement propagates, empty text deletes ---
    c1.Send(MakeSign(8, 60, 8, 4, "vitejte, poutnici"));
    Check(WaitForMessage(c2,
                         [](const ServerMessage& m) {
                             return m.op == ServerOpcode::Sign && m.bx == 8 && m.face == 4 &&
                                    m.text == "vitejte, poutnici";
                         },
                         msg),
          "a sign propagates with its comma-bearing text intact");
    c2.Send(MakeChunkRequest(0, 0, 0));
    ChunkResponse withSign = CollectChunkResponse(c2, 0, 0);
    Check(withSign.done && withSign.signs.size() == 1 && withSign.signs[0].text == "vitejte, poutnici" &&
              withSign.lights.size() == 1 && withSign.lights[0].w == 15,
          "a fresh chunk request returns the sign and the light");

    c1.Send(MakeSign(8, 60, 8, 4, ""));
    Check(WaitForMessage(c2,
                         [](const ServerMessage& m) {
                             return m.op == ServerOpcode::Sign && m.bx == 8 && m.face == 4 && m.text.empty();
                         },
                         msg),
          "an empty-text sign (deletion) propagates");

    // --- Persistence across a server restart ---
    c1.Stop();
    c2.Stop();
    server->Stop();
    server = std::make_unique<GameServer>(config);
    Check(server->Start(), "server restarts on the same world db");

    GameClient c3;
    Check(c3.Connect("127.0.0.1", server->Port()), "client 3 connects to the restarted server");
    Handshake(c3, "cyril");
    Check(WaitForMessage(c3, [](const ServerMessage& m) { return m.op == ServerOpcode::WorldInfo; }, msg),
          "client 3 handshake completes");
    c3.Send(MakeChunkRequest(0, 0, 0));
    ChunkResponse persisted = CollectChunkResponse(c3, 0, 0);
    bool sawSupportBlock = false;
    for (const auto& b : persisted.blocks) {
        if (b.bx == 8 && b.by == 60 && b.bz == 8 && b.w == kStone) sawSupportBlock = true;
    }
    Check(persisted.done && sawSupportBlock, "edits survive the restart (support block still streamed)");
    Check(persisted.lights.size() == 1 && persisted.lights[0].w == 15, "the toggled light survives the restart");
    Check(persisted.signs.empty(), "the deleted sign stays deleted after the restart");

    // A break attempt on the persisted support block must validate against
    // GENERATED terrain + stored edits, proving EnsureColumnLoaded works
    // on a cold world: breaking it succeeds (no revert Talk).
    c3.Send(MakeBlock(8, 60, 8, kAir));
    c3.Send(MakeChunkRequest(0, 0, 0));
    ChunkResponse afterBreak = CollectChunkResponse(c3, 0, 0);
    bool supportNowAir = false;
    for (const auto& b : afterBreak.blocks) {
        if (b.bx == 8 && b.by == 60 && b.bz == 8 && b.w == kAir) supportNowAir = true;
    }
    Check(afterBreak.done && supportNowAir,
          "breaking a persisted block on the restarted server validates and sticks");

    c3.Stop();
    server->Stop();
    std::remove(kTestDbPath);
    Check(true, "server restart cycle shut down cleanly");
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
