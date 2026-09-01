// Plain-assert smoke test for CnaCraft::Net's wire protocol (MULTIPLAYER_PLAN.md
// §2/§5, plan.md §12.1 item 18 phase M0). Pure parse/format -- no sockets, no
// CNA/GPU/display -- so it runs in the worlds-only build configuration.

#include <cstdio>
#include <string>

#include "CnaCraft/Net/Protocol.hpp"

using namespace CnaCraft::Net;

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

void TestClientRoundTrips() {
    {
        const auto m = ParseClientLine(FormatClientLine(MakeVersion(kProtocolVersion)));
        Check(m && m->op == ClientOpcode::Version && m->version == 1001, "client V round-trips (version 1001)");
    }
    {
        const auto m = ParseClientLine(FormatClientLine(MakeNick("robert")));
        Check(m && m->op == ClientOpcode::Nick && m->text == "robert", "client A round-trips a nick");
    }
    {
        const auto m = ParseClientLine(FormatClientLine(MakeNick("")));
        Check(m && m->op == ClientOpcode::Nick && m->text.empty(),
              "client A with an empty nick round-trips (guest login)");
    }
    {
        const auto m = ParseClientLine(FormatClientLine(MakePosition(1.234f, -5.678f, 0.0f, 3.14159f, -0.5f)));
        Check(m && m->op == ClientOpcode::Position && m->x > 1.22f && m->x < 1.24f && m->y > -5.69f &&
                  m->y < -5.67f && m->ry > -0.51f && m->ry < -0.49f,
              "client P round-trips within 2dp precision");
    }
    {
        Check(FormatClientLine(MakePosition(1.234f, -5.678f, 0.0f, 3.14159f, -0.5f)) ==
                  "P,1.23,-5.68,0.00,3.14,-0.50",
              "client P formats floats at exactly 2dp (Craft's own %.2f)");
    }
    {
        const auto m = ParseClientLine(FormatClientLine(MakeChunkRequest(-7, 12, 9876543210LL)));
        Check(m && m->op == ClientOpcode::ChunkRequest && m->p == -7 && m->q == 12 && m->key == 9876543210LL,
              "client C round-trips negative column coords and an int64 key");
    }
    {
        const auto m = ParseClientLine(FormatClientLine(MakeBlock(-3, 20, 45, 4)));
        Check(m && m->op == ClientOpcode::Block && m->bx == -3 && m->by == 20 && m->bz == 45 && m->w == 4,
              "client B round-trips");
    }
    {
        const auto m = ParseClientLine(FormatClientLine(MakeLight(1, 2, 3, 15)));
        Check(m && m->op == ClientOpcode::Light && m->w == 15, "client L round-trips Craft's literal 15");
    }
    {
        const auto m = ParseClientLine(FormatClientLine(MakeSign(5, 6, 7, 3, "hello, world, with commas")));
        Check(m && m->op == ClientOpcode::Sign && m->face == 3 && m->text == "hello, world, with commas",
              "client S keeps commas inside sign text intact");
    }
    {
        const auto m = ParseClientLine(FormatClientLine(MakeTalk("/view 12, please")));
        Check(m && m->op == ClientOpcode::Talk && m->text == "/view 12, please",
              "client T keeps commas inside chat text intact");
    }
}

void TestServerRoundTrips() {
    {
        ServerMessage s;
        s.op = ServerOpcode::You;
        s.id = 3;
        s.x = 17.5f;
        s.y = 46.8f;
        s.z = 28.0f;
        s.rx = 0.0f;
        s.ry = -0.12f;
        const auto m = ParseServerLine(FormatServerLine(s));
        Check(m && m->op == ServerOpcode::You && m->id == 3 && m->y > 46.79f && m->y < 46.81f,
              "server U round-trips");
    }
    {
        ServerMessage s;
        s.op = ServerOpcode::Time;
        s.elapsed = 12345.67;
        s.dayLength = 600.0f;
        const auto m = ParseServerLine(FormatServerLine(s));
        Check(m && m->op == ServerOpcode::Time && m->elapsed > 12345.66 && m->elapsed < 12345.68 &&
                  m->dayLength == 600.0f,
              "server E round-trips");
    }
    {
        ServerMessage s;
        s.op = ServerOpcode::WorldInfo;
        s.seed = 0xFFFFFFFFu;
        const auto m = ParseServerLine(FormatServerLine(s));
        Check(m && m->op == ServerOpcode::WorldInfo && m->seed == 0xFFFFFFFFu,
              "server W round-trips a max uint32 seed");
    }
    {
        ServerMessage s;
        s.op = ServerOpcode::Block;
        s.p = -2;
        s.q = 5;
        s.bx = -31;
        s.by = 20;
        s.bz = 85;
        s.w = 14;
        const auto m = ParseServerLine(FormatServerLine(s));
        Check(m && m->op == ServerOpcode::Block && m->p == -2 && m->bx == -31 && m->w == 14,
              "server B round-trips (6 int fields)");
    }
    {
        ServerMessage s;
        s.op = ServerOpcode::Sign;
        s.p = 0;
        s.q = 1;
        s.bx = 12;
        s.by = 45;
        s.bz = 22;
        s.face = 2;
        s.text = "ahoj, svete";
        const auto m = ParseServerLine(FormatServerLine(s));
        Check(m && m->op == ServerOpcode::Sign && m->face == 2 && m->text == "ahoj, svete",
              "server S keeps commas in sign text");
    }
    {
        ServerMessage s;
        s.op = ServerOpcode::Key;
        s.p = 1;
        s.q = -1;
        s.key = 1234567890123LL;
        const auto m = ParseServerLine(FormatServerLine(s));
        Check(m && m->op == ServerOpcode::Key && m->key == 1234567890123LL, "server K round-trips an int64 key");
    }
    {
        ServerMessage s;
        s.op = ServerOpcode::Redraw;
        s.p = 3;
        s.q = 4;
        const auto m = ParseServerLine(FormatServerLine(s));
        Check(m && m->op == ServerOpcode::Redraw && m->p == 3 && m->q == 4, "server R round-trips");
    }
    {
        ServerMessage s;
        s.op = ServerOpcode::ChunkDone;
        s.p = -9;
        s.q = 9;
        const auto m = ParseServerLine(FormatServerLine(s));
        Check(m && m->op == ServerOpcode::ChunkDone && m->p == -9, "server C (chunk-done marker) round-trips");
    }
    {
        ServerMessage s;
        s.op = ServerOpcode::Position;
        s.id = 7;
        s.x = -1.5f;
        s.y = 50.0f;
        s.z = 2.25f;
        s.rx = 1.57f;
        s.ry = 0.0f;
        const auto m = ParseServerLine(FormatServerLine(s));
        Check(m && m->op == ServerOpcode::Position && m->id == 7 && m->rx > 1.56f && m->rx < 1.58f,
              "server P round-trips");
    }
    {
        ServerMessage s;
        s.op = ServerOpcode::Nick;
        s.id = 2;
        s.text = "guest2";
        const auto m = ParseServerLine(FormatServerLine(s));
        Check(m && m->op == ServerOpcode::Nick && m->id == 2 && m->text == "guest2", "server N round-trips");
    }
    {
        ServerMessage s;
        s.op = ServerOpcode::Disconnect;
        s.id = 5;
        const auto m = ParseServerLine(FormatServerLine(s));
        Check(m && m->op == ServerOpcode::Disconnect && m->id == 5, "server D round-trips");
    }
    {
        ServerMessage s;
        s.op = ServerOpcode::Talk;
        s.text = "robert has joined, welcome";
        const auto m = ParseServerLine(FormatServerLine(s));
        Check(m && m->op == ServerOpcode::Talk && m->text == "robert has joined, welcome",
              "server T keeps commas in chat text");
    }
}

void TestMalformedLinesRejected() {
    Check(!ParseServerLine(""), "empty line rejected");
    Check(!ParseServerLine("B"), "opcode without comma rejected");
    Check(!ParseServerLine("X,1,2"), "unknown server opcode rejected");
    Check(!ParseClientLine("Z,1"), "unknown client opcode rejected");
    Check(!ParseServerLine("B,1,2,3,4,5"), "server B with too few fields rejected");
    Check(!ParseServerLine("B,1,2,3,4,5,abc"), "server B with a non-numeric field rejected");
    Check(!ParseClientLine("P,1,2,3,4"), "client P with too few fields rejected");
    Check(!ParseClientLine("C,1,2,notakey"), "client C with a non-numeric key rejected");
    Check(!ParseServerLine("R,1,2,3"), "server R with too many fields rejected (strict field count)");
    Check(!ParseServerLine("W,4294967296"), "server W with an out-of-range seed rejected");
    // Craft's own quirk-tolerance: a trailing '\r' from a telnet-style peer
    // must not break parsing.
    const auto m = ParseServerLine("D,4\r");
    Check(m && m->op == ServerOpcode::Disconnect && m->id == 4, "trailing carriage return tolerated");
}

}  // namespace

int main() {
    TestClientRoundTrips();
    TestServerRoundTrips();
    TestMalformedLinesRejected();

    if (g_failures == 0) {
        std::printf("\nAll checks passed.\n");
        return 0;
    }
    std::fprintf(stderr, "\n%d check(s) FAILED.\n", g_failures);
    return 1;
}
