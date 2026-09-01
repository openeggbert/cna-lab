#pragma once

#include <cstdint>
#include <optional>
#include <string>

// cna-craft's multiplayer wire protocol (MULTIPLAYER_PLAN.md §2/§4/§5).
//
// Shape-identical to Craft's ASCII line protocol -- comma-separated fields,
// one message per '\n'-terminated line, same opcodes and semantics
// (client.c / server.py) -- with the deliberate, documented dialect deltas
// decided 2026-07-11 (MULTIPLAYER_PLAN.md §11):
//
//   - protocol version 1001 (not Craft's 1), so a real Craft client/server
//     pairing fails fast at the version check instead of desyncing on the
//     three world-model differences below;
//   - `p`/`q` are cna-craft 16-block COLUMN coords (Craft: 32-block chunks);
//   - `w` carries cna-craft BlockType ordinals (Craft: its own item ids);
//   - a new `W,seed` server->client message at connect: cna-craft's terrain
//     generator is per-seed (NoiseGenerator), so remote clients need the
//     server's seed to generate identical base terrain -- Craft never needs
//     this because its generation is unseeded-global;
//   - `A` carries just an optional nick (no auth -- user decision: guest
//     names + /nick only; Craft's identity-token flow is unported).
//
// This module is PURE parse/format -- no sockets, no threads, no I/O, no
// CNA/SDL/sharp-runtime includes -- so it builds and unit-tests in the
// engine-agnostic worlds-only configuration (-DCNA_CRAFT_BUILD_GAME=OFF),
// same discipline as Worlds/ (plan.md §2/§8).
namespace CnaCraft::Net {

// Version 1001: "Craft protocol v1, cna-craft dialect 001".
inline constexpr int kProtocolVersion = 1001;
// Craft's own default port (client.h / server.py) -- kept for familiarity.
inline constexpr int kDefaultPort = 4080;

// ---------------------------------------------------------------------------
// Client -> server
// ---------------------------------------------------------------------------

enum class ClientOpcode {
    Version,       // V,version
    Nick,          // A,nick            (nick may be empty -> server assigns guest name)
    Position,      // P,x,y,z,rx,ry     (floats, 2dp, Craft's own precision)
    ChunkRequest,  // C,p,q,key         (key = highest block rowid already cached, 0 = none)
    Block,         // B,x,y,z,w
    Light,         // L,x,y,z,w         (w: 15 = on, 0 = off -- Craft's literal values)
    Sign,          // S,x,y,z,face,text (text may contain commas; empty text deletes)
    Talk,          // T,text            (chat or /command forwarded to the server)
};

struct ClientMessage {
    ClientOpcode op = ClientOpcode::Version;
    int version = 0;                   // Version
    float x = 0, y = 0, z = 0;         // Position (player) -- also reused via bx/by/bz for blocks
    float rx = 0, ry = 0;              // Position (yaw/pitch, radians)
    int p = 0, q = 0;                  // ChunkRequest (column coords)
    std::int64_t key = 0;              // ChunkRequest
    int bx = 0, by = 0, bz = 0, w = 0; // Block/Light/Sign (block coords + value)
    int face = 0;                      // Sign
    std::string text;                  // Nick/Sign/Talk
};

std::string FormatClientLine(const ClientMessage& msg);
std::optional<ClientMessage> ParseClientLine(const std::string& line);

// Convenience builders for the handful of messages the game sends hot.
ClientMessage MakeVersion(int version);
ClientMessage MakeNick(const std::string& nick);
ClientMessage MakePosition(float x, float y, float z, float rx, float ry);
ClientMessage MakeChunkRequest(int p, int q, std::int64_t key);
ClientMessage MakeBlock(int x, int y, int z, int w);
ClientMessage MakeLight(int x, int y, int z, int w);
ClientMessage MakeSign(int x, int y, int z, int face, const std::string& text);
ClientMessage MakeTalk(const std::string& text);

// ---------------------------------------------------------------------------
// Server -> client
// ---------------------------------------------------------------------------

enum class ServerOpcode {
    You,        // U,id,x,y,z,rx,ry   (your id + authoritative position)
    Time,       // E,elapsed,dayLength (server world-clock seconds; client sets fmod(elapsed, dayLength))
    WorldInfo,  // W,seed              (terrain seed -- cna-craft dialect addition)
    Block,      // B,p,q,x,y,z,w       (chunk delta, broadcast edit, or rejected-edit REVERT)
    Light,      // L,p,q,x,y,z,w
    Sign,       // S,p,q,x,y,z,face,text
    Key,        // K,p,q,key           (new cache version after a chunk's block deltas)
    Redraw,     // R,p,q               (force remesh)
    ChunkDone,  // C,p,q               (end-of-chunk-response marker)
    Position,   // P,id,x,y,z,rx,ry    (another player moved)
    Nick,       // N,id,nick
    Disconnect, // D,id
    Talk,       // T,text
};

struct ServerMessage {
    ServerOpcode op = ServerOpcode::Talk;
    int id = 0;                        // You/Position/Nick/Disconnect
    float x = 0, y = 0, z = 0;         // You/Position
    float rx = 0, ry = 0;              // You/Position
    double elapsed = 0;                // Time
    float dayLength = 0;               // Time
    std::uint32_t seed = 0;            // WorldInfo
    int p = 0, q = 0;                  // Block/Light/Sign/Key/Redraw/ChunkDone
    int bx = 0, by = 0, bz = 0, w = 0; // Block/Light/Sign
    int face = 0;                      // Sign
    std::int64_t key = 0;              // Key
    std::string text;                  // Talk/Nick/Sign
};

std::string FormatServerLine(const ServerMessage& msg);
std::optional<ServerMessage> ParseServerLine(const std::string& line);

}
