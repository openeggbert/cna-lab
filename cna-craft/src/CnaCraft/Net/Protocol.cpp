#include "Protocol.hpp"

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace CnaCraft::Net {

namespace {

// Splits `line` on commas into at most `maxFields` pieces -- the LAST piece
// absorbs everything remaining, commas included. That's how Craft's own
// parsers keep sign/chat text (which may legally contain commas) intact:
// its sscanf formats read the text field as "%[^\n]" after the fixed-count
// numeric prefix. A trailing '\r' (telnet-style peer) is stripped first.
std::vector<std::string> SplitFields(const std::string& line, std::size_t maxFields) {
    std::string s = line;
    if (!s.empty() && s.back() == '\r') s.pop_back();
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (fields.size() + 1 < maxFields) {
        const std::size_t comma = s.find(',', start);
        if (comma == std::string::npos) break;
        fields.push_back(s.substr(start, comma - start));
        start = comma + 1;
    }
    fields.push_back(s.substr(start));
    return fields;
}

bool ParseInt(const std::string& s, int& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    const long v = std::strtol(s.c_str(), &end, 10);
    if (end != s.c_str() + s.size()) return false;
    out = static_cast<int>(v);
    return true;
}

bool ParseInt64(const std::string& s, std::int64_t& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    const long long v = std::strtoll(s.c_str(), &end, 10);
    if (end != s.c_str() + s.size()) return false;
    out = v;
    return true;
}

bool ParseUint32(const std::string& s, std::uint32_t& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    const unsigned long long v = std::strtoull(s.c_str(), &end, 10);
    if (end != s.c_str() + s.size() || v > 0xFFFFFFFFull) return false;
    out = static_cast<std::uint32_t>(v);
    return true;
}

bool ParseFloat(const std::string& s, float& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    const float v = std::strtof(s.c_str(), &end);
    if (end != s.c_str() + s.size()) return false;
    out = v;
    return true;
}

bool ParseDouble(const std::string& s, double& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (end != s.c_str() + s.size()) return false;
    out = v;
    return true;
}

// Craft formats every float field with %.2f (client.c's snprintf formats);
// matched exactly so position traffic stays byte-comparable in tests.
std::string F2(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(v));
    return buf;
}

std::string FInt(int v) { return std::to_string(v); }

}  // namespace

// ---------------------------------------------------------------------------
// Client -> server
// ---------------------------------------------------------------------------

std::string FormatClientLine(const ClientMessage& m) {
    switch (m.op) {
        case ClientOpcode::Version:
            return "V," + FInt(m.version);
        case ClientOpcode::Nick:
            return "A," + m.text;
        case ClientOpcode::Position:
            return "P," + F2(m.x) + "," + F2(m.y) + "," + F2(m.z) + "," + F2(m.rx) + "," + F2(m.ry);
        case ClientOpcode::ChunkRequest:
            return "C," + FInt(m.p) + "," + FInt(m.q) + "," + std::to_string(m.key);
        case ClientOpcode::Block:
            return "B," + FInt(m.bx) + "," + FInt(m.by) + "," + FInt(m.bz) + "," + FInt(m.w);
        case ClientOpcode::Light:
            return "L," + FInt(m.bx) + "," + FInt(m.by) + "," + FInt(m.bz) + "," + FInt(m.w);
        case ClientOpcode::Sign:
            return "S," + FInt(m.bx) + "," + FInt(m.by) + "," + FInt(m.bz) + "," + FInt(m.face) + "," + m.text;
        case ClientOpcode::Talk:
            return "T," + m.text;
    }
    return {};
}

std::optional<ClientMessage> ParseClientLine(const std::string& line) {
    if (line.size() < 2 || line[1] != ',') return std::nullopt;
    ClientMessage m;
    switch (line[0]) {
        case 'V': {
            const auto f = SplitFields(line, 2);
            if (f.size() != 2 || !ParseInt(f[1], m.version)) return std::nullopt;
            m.op = ClientOpcode::Version;
            return m;
        }
        case 'A': {
            const auto f = SplitFields(line, 2);
            m.op = ClientOpcode::Nick;
            m.text = f[1];
            return m;
        }
        case 'P': {
            const auto f = SplitFields(line, 6);
            if (f.size() != 6 || !ParseFloat(f[1], m.x) || !ParseFloat(f[2], m.y) || !ParseFloat(f[3], m.z) ||
                !ParseFloat(f[4], m.rx) || !ParseFloat(f[5], m.ry))
                return std::nullopt;
            m.op = ClientOpcode::Position;
            return m;
        }
        case 'C': {
            const auto f = SplitFields(line, 4);
            if (f.size() != 4 || !ParseInt(f[1], m.p) || !ParseInt(f[2], m.q) || !ParseInt64(f[3], m.key))
                return std::nullopt;
            m.op = ClientOpcode::ChunkRequest;
            return m;
        }
        case 'B':
        case 'L': {
            const auto f = SplitFields(line, 5);
            if (f.size() != 5 || !ParseInt(f[1], m.bx) || !ParseInt(f[2], m.by) || !ParseInt(f[3], m.bz) ||
                !ParseInt(f[4], m.w))
                return std::nullopt;
            m.op = line[0] == 'B' ? ClientOpcode::Block : ClientOpcode::Light;
            return m;
        }
        case 'S': {
            const auto f = SplitFields(line, 6);
            if (f.size() != 6 || !ParseInt(f[1], m.bx) || !ParseInt(f[2], m.by) || !ParseInt(f[3], m.bz) ||
                !ParseInt(f[4], m.face))
                return std::nullopt;
            m.op = ClientOpcode::Sign;
            m.text = f[5];
            return m;
        }
        case 'T': {
            const auto f = SplitFields(line, 2);
            m.op = ClientOpcode::Talk;
            m.text = f[1];
            return m;
        }
        default:
            return std::nullopt;
    }
}

ClientMessage MakeVersion(int version) {
    ClientMessage m;
    m.op = ClientOpcode::Version;
    m.version = version;
    return m;
}

ClientMessage MakeNick(const std::string& nick) {
    ClientMessage m;
    m.op = ClientOpcode::Nick;
    m.text = nick;
    return m;
}

ClientMessage MakePosition(float x, float y, float z, float rx, float ry) {
    ClientMessage m;
    m.op = ClientOpcode::Position;
    m.x = x;
    m.y = y;
    m.z = z;
    m.rx = rx;
    m.ry = ry;
    return m;
}

ClientMessage MakeChunkRequest(int p, int q, std::int64_t key) {
    ClientMessage m;
    m.op = ClientOpcode::ChunkRequest;
    m.p = p;
    m.q = q;
    m.key = key;
    return m;
}

ClientMessage MakeBlock(int x, int y, int z, int w) {
    ClientMessage m;
    m.op = ClientOpcode::Block;
    m.bx = x;
    m.by = y;
    m.bz = z;
    m.w = w;
    return m;
}

ClientMessage MakeLight(int x, int y, int z, int w) {
    ClientMessage m;
    m.op = ClientOpcode::Light;
    m.bx = x;
    m.by = y;
    m.bz = z;
    m.w = w;
    return m;
}

ClientMessage MakeSign(int x, int y, int z, int face, const std::string& text) {
    ClientMessage m;
    m.op = ClientOpcode::Sign;
    m.bx = x;
    m.by = y;
    m.bz = z;
    m.face = face;
    m.text = text;
    return m;
}

ClientMessage MakeTalk(const std::string& text) {
    ClientMessage m;
    m.op = ClientOpcode::Talk;
    m.text = text;
    return m;
}

// ---------------------------------------------------------------------------
// Server -> client
// ---------------------------------------------------------------------------

std::string FormatServerLine(const ServerMessage& m) {
    switch (m.op) {
        case ServerOpcode::You:
            return "U," + FInt(m.id) + "," + F2(m.x) + "," + F2(m.y) + "," + F2(m.z) + "," + F2(m.rx) + "," +
                   F2(m.ry);
        case ServerOpcode::Time: {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "E,%.2f,%.2f", m.elapsed, static_cast<double>(m.dayLength));
            return buf;
        }
        case ServerOpcode::WorldInfo:
            return "W," + std::to_string(m.seed);
        case ServerOpcode::Block:
            return "B," + FInt(m.p) + "," + FInt(m.q) + "," + FInt(m.bx) + "," + FInt(m.by) + "," + FInt(m.bz) +
                   "," + FInt(m.w);
        case ServerOpcode::Light:
            return "L," + FInt(m.p) + "," + FInt(m.q) + "," + FInt(m.bx) + "," + FInt(m.by) + "," + FInt(m.bz) +
                   "," + FInt(m.w);
        case ServerOpcode::Sign:
            return "S," + FInt(m.p) + "," + FInt(m.q) + "," + FInt(m.bx) + "," + FInt(m.by) + "," + FInt(m.bz) +
                   "," + FInt(m.face) + "," + m.text;
        case ServerOpcode::Key:
            return "K," + FInt(m.p) + "," + FInt(m.q) + "," + std::to_string(m.key);
        case ServerOpcode::Redraw:
            return "R," + FInt(m.p) + "," + FInt(m.q);
        case ServerOpcode::ChunkDone:
            return "C," + FInt(m.p) + "," + FInt(m.q);
        case ServerOpcode::Position:
            return "P," + FInt(m.id) + "," + F2(m.x) + "," + F2(m.y) + "," + F2(m.z) + "," + F2(m.rx) + "," +
                   F2(m.ry);
        case ServerOpcode::Nick:
            return "N," + FInt(m.id) + "," + m.text;
        case ServerOpcode::Disconnect:
            return "D," + FInt(m.id);
        case ServerOpcode::Talk:
            return "T," + m.text;
    }
    return {};
}

std::optional<ServerMessage> ParseServerLine(const std::string& line) {
    if (line.size() < 2 || line[1] != ',') return std::nullopt;
    ServerMessage m;
    switch (line[0]) {
        case 'U': {
            const auto f = SplitFields(line, 7);
            if (f.size() != 7 || !ParseInt(f[1], m.id) || !ParseFloat(f[2], m.x) || !ParseFloat(f[3], m.y) ||
                !ParseFloat(f[4], m.z) || !ParseFloat(f[5], m.rx) || !ParseFloat(f[6], m.ry))
                return std::nullopt;
            m.op = ServerOpcode::You;
            return m;
        }
        case 'E': {
            const auto f = SplitFields(line, 3);
            if (f.size() != 3 || !ParseDouble(f[1], m.elapsed) || !ParseFloat(f[2], m.dayLength))
                return std::nullopt;
            m.op = ServerOpcode::Time;
            return m;
        }
        case 'W': {
            const auto f = SplitFields(line, 2);
            if (f.size() != 2 || !ParseUint32(f[1], m.seed)) return std::nullopt;
            m.op = ServerOpcode::WorldInfo;
            return m;
        }
        case 'B':
        case 'L': {
            const auto f = SplitFields(line, 7);
            if (f.size() != 7 || !ParseInt(f[1], m.p) || !ParseInt(f[2], m.q) || !ParseInt(f[3], m.bx) ||
                !ParseInt(f[4], m.by) || !ParseInt(f[5], m.bz) || !ParseInt(f[6], m.w))
                return std::nullopt;
            m.op = line[0] == 'B' ? ServerOpcode::Block : ServerOpcode::Light;
            return m;
        }
        case 'S': {
            const auto f = SplitFields(line, 8);
            if (f.size() != 8 || !ParseInt(f[1], m.p) || !ParseInt(f[2], m.q) || !ParseInt(f[3], m.bx) ||
                !ParseInt(f[4], m.by) || !ParseInt(f[5], m.bz) || !ParseInt(f[6], m.face))
                return std::nullopt;
            m.op = ServerOpcode::Sign;
            m.text = f[7];
            return m;
        }
        case 'K': {
            const auto f = SplitFields(line, 4);
            if (f.size() != 4 || !ParseInt(f[1], m.p) || !ParseInt(f[2], m.q) || !ParseInt64(f[3], m.key))
                return std::nullopt;
            m.op = ServerOpcode::Key;
            return m;
        }
        case 'R':
        case 'C': {
            const auto f = SplitFields(line, 3);
            if (f.size() != 3 || !ParseInt(f[1], m.p) || !ParseInt(f[2], m.q)) return std::nullopt;
            m.op = line[0] == 'R' ? ServerOpcode::Redraw : ServerOpcode::ChunkDone;
            return m;
        }
        case 'P': {
            const auto f = SplitFields(line, 7);
            if (f.size() != 7 || !ParseInt(f[1], m.id) || !ParseFloat(f[2], m.x) || !ParseFloat(f[3], m.y) ||
                !ParseFloat(f[4], m.z) || !ParseFloat(f[5], m.rx) || !ParseFloat(f[6], m.ry))
                return std::nullopt;
            m.op = ServerOpcode::Position;
            return m;
        }
        case 'N': {
            const auto f = SplitFields(line, 3);
            if (f.size() != 3 || !ParseInt(f[1], m.id)) return std::nullopt;
            m.op = ServerOpcode::Nick;
            m.text = f[2];
            return m;
        }
        case 'D': {
            const auto f = SplitFields(line, 2);
            if (f.size() != 2 || !ParseInt(f[1], m.id)) return std::nullopt;
            m.op = ServerOpcode::Disconnect;
            return m;
        }
        case 'T': {
            const auto f = SplitFields(line, 2);
            m.op = ServerOpcode::Talk;
            m.text = f[1];
            return m;
        }
        default:
            return std::nullopt;
    }
}

}
