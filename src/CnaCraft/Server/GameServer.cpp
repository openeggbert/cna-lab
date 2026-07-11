#include "GameServer.hpp"

#include <chrono>
#include <cstdio>

#include "System/Net/IPAddress.hpp"
#include "System/Net/IPEndPoint.hpp"
#include "System/Net/Sockets/AddressFamily.hpp"
#include "System/Net/Sockets/ProtocolType.hpp"
#include "System/Net/Sockets/SocketShutdown.hpp"
#include "System/Net/Sockets/SocketType.hpp"
#include "System/Threading/Thread.hpp"

namespace CnaCraft::Server {

namespace {

// Service threads poll at 20ms: inbound latency cap AND outbox flush cadence
// -- both far under the 100ms position-send interval the protocol is built
// around (MULTIPLAYER_PLAN.md §12).
constexpr long kServicePollMicros = 20'000;
// The model thread naps briefly when its command queue runs dry.
constexpr int kModelIdleSleepMs = 2;
constexpr std::size_t kMaxNickLength = 32;  // Craft's MAX_NAME_LENGTH

std::int64_t NowTicks() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// Nicks travel inside comma-separated lines and next to '>' in chat --
// strip anything that could confuse either, cap at Craft's name length.
std::string SanitizeNick(const std::string& raw) {
    std::string nick;
    for (const char c : raw) {
        if (c == ',' || c == '\n' || c == '\r' || c == '>') continue;
        if (c == ' ' || c == '\t') continue;
        nick.push_back(c);
        if (nick.size() >= kMaxNickLength) break;
    }
    return nick;
}

}  // namespace

GameServer::GameServer(ServerConfig config) : config_(config) {}

GameServer::~GameServer() { Stop(); }

bool GameServer::Start() {
    if (running_) return true;
    try {
        listener_ = std::make_unique<System::Net::Sockets::Socket>(
            System::Net::Sockets::AddressFamily::InterNetwork, System::Net::Sockets::SocketType::Stream,
            System::Net::Sockets::ProtocolType::Tcp);
        listener_->Bind(System::Net::IPEndPoint(System::Net::IPAddress::Any, config_.port));
        listener_->Listen();
        const auto local = listener_->getLocalEndPointProperty();
        if (const auto* ip = dynamic_cast<System::Net::IPEndPoint*>(local.get())) port_ = ip->getPortProperty();
    } catch (const std::exception& e) {
        lastError_ = e.what();
        listener_.reset();
        return false;
    } catch (...) {
        lastError_ = "unknown bind/listen error";
        listener_.reset();
        return false;
    }

    startTicks_ = NowTicks();
    running_ = true;
    acceptThread_ = std::make_unique<System::Threading::Thread>([this]() { AcceptLoop(); });
    acceptThread_->Start();
    modelThread_ = std::make_unique<System::Threading::Thread>([this]() { ModelLoop(); });
    modelThread_->Start();
    return true;
}

void GameServer::Stop() {
    if (!running_ && !acceptThread_ && !modelThread_) return;
    running_ = false;

    // Wake the accept thread. close() alone does NOT unblock a thread
    // sitting in accept() on Linux (it can stay blocked until the next
    // real connection) -- shutdown() the listener first (wakes accept with
    // an error on Linux), then poke it with a throwaway self-connection as
    // a portable fallback, and only then close.
    if (listener_) {
        try {
            listener_->Shutdown(System::Net::Sockets::SocketShutdown::Both);
        } catch (...) {
        }
        try {
            System::Net::Sockets::Socket poke(System::Net::Sockets::AddressFamily::InterNetwork,
                                              System::Net::Sockets::SocketType::Stream,
                                              System::Net::Sockets::ProtocolType::Tcp);
            poke.Connect("127.0.0.1", port_);
            poke.Close();
        } catch (...) {
        }
    }
    if (acceptThread_) {
        acceptThread_->Join();
        acceptThread_.reset();
    }
    if (listener_) {
        try {
            listener_->Close();
        } catch (...) {
        }
    }
    if (modelThread_) {
        modelThread_->Join();
        modelThread_.reset();
    }
    // Service threads exit their loops on !running_/shutdown; join them all
    // (clients_ is safe to touch here -- the model thread is gone).
    for (auto& [id, client] : clients_) {
        (void)id;
        client->alive = false;
        if (client->line) client->line->ShutdownBoth();
    }
    for (auto& [id, client] : clients_) {
        (void)id;
        if (client->serviceThread) {
            client->serviceThread->Join();
            client->serviceThread.reset();
        }
        if (client->line) client->line->Close();
    }
    clients_.clear();
    for (auto& zombie : zombies_) {
        if (zombie->serviceThread) {
            zombie->serviceThread->Join();
            zombie->serviceThread.reset();
        }
        if (zombie->line) zombie->line->Close();
    }
    zombies_.clear();
    listener_.reset();
}

void GameServer::AcceptLoop() {
    while (running_) {
        std::shared_ptr<System::Net::Sockets::Socket> accepted;
        try {
            accepted = listener_->Accept();
        } catch (...) {
            break;  // listener closed (Stop) or fatal accept error
        }
        if (!accepted) continue;
        if (!running_) break;

        auto client = std::make_shared<Client>();
        client->id = nextClientId_++;  // BEFORE the service thread exists -- see the member comment
        client->socket = accepted;
        client->line = std::make_unique<Net::LineSocket>(accepted);

        // FIFO guarantee: this Join is enqueued before the service thread
        // starts, so the model always processes the Join before any of the
        // client's Line commands.
        Command join;
        join.kind = Command::Kind::Join;
        join.client = client;
        commands_.Enqueue(join);

        Client* raw = client.get();  // lifetime guaranteed by the never-free-while-running rule
        client->serviceThread = std::make_unique<System::Threading::Thread>([this, raw]() { ServiceLoop(raw); });
        client->serviceThread->Start();
    }
}

void GameServer::ServiceLoop(Client* client) {
    std::vector<std::string> lines;
    bool connectionOver = false;
    while (running_ && client->alive) {
        if (!client->line->ReadLines(lines, kServicePollMicros)) {
            connectionOver = true;
            break;
        }
        for (const std::string& raw : lines) {
            if (auto msg = Net::ParseClientLine(raw)) {
                Command cmd;
                cmd.kind = Command::Kind::Line;
                cmd.clientId = client->id;
                cmd.msg = *msg;
                commands_.Enqueue(cmd);
            }
        }
        lines.clear();

        std::string outLine;
        while (client->outbox.TryDequeue(outLine)) {
            if (!client->line->SendLine(outLine)) {
                connectionOver = true;
                break;
            }
        }
        if (connectionOver) break;
    }
    // Best-effort final flush: e.g. the version-mismatch reject message is
    // enqueued in the same model step that marks the client dead -- without
    // this drain the client would drop before ever seeing why.
    if (!connectionOver) {
        std::string outLine;
        while (client->outbox.TryDequeue(outLine)) {
            if (!client->line->SendLine(outLine)) break;
        }
    }
    client->line->ShutdownBoth();  // end the TCP conversation from our side too
    if (running_) {
        client->alive = false;
        // Leave is enqueued on EVERY service exit while the server runs --
        // both peer-initiated (connectionOver) and model-initiated kills
        // (alive=false, e.g. version rejects) -- so the model always cleans
        // the client out of clients_. HandleLeave on an already-removed id
        // is a no-op, so double enqueue is harmless.
        Command leave;
        leave.kind = Command::Kind::Leave;
        leave.clientId = client->id;
        commands_.Enqueue(leave);
    }
}

void GameServer::ModelLoop() {
    while (running_) {
        Command cmd;
        if (!commands_.TryDequeue(cmd)) {
            System::Threading::Thread::Sleep(kModelIdleSleepMs);
            continue;
        }
        switch (cmd.kind) {
            case Command::Kind::Join:
                HandleJoin(cmd.client);
                break;
            case Command::Kind::Leave:
                HandleLeave(cmd.clientId);
                break;
            case Command::Kind::Line: {
                const auto it = clients_.find(cmd.clientId);
                if (it != clients_.end()) HandleMessage(*it->second, cmd.msg);
                break;
            }
        }
    }
}

void GameServer::HandleJoin(const std::shared_ptr<Client>& client) {
    client->nick = "guest" + std::to_string(client->id);
    clients_[client->id] = client;

    // server.py's on_connect, in its order: your id+spawn (U), the clock
    // (E), then -- cna-craft dialect -- the terrain seed (W), a welcome
    // line, existing players' positions+nicks to the newcomer, and the
    // newcomer's presence to everyone else. Spawn y=0 keeps Craft's own
    // convention: the client computes local ground height when uy == 0.
    Net::ServerMessage you;
    you.op = Net::ServerOpcode::You;
    you.id = client->id;
    SendTo(*client, you);

    Net::ServerMessage time;
    time.op = Net::ServerOpcode::Time;
    time.elapsed = ElapsedSeconds();
    time.dayLength = config_.dayLengthSeconds;
    SendTo(*client, time);

    Net::ServerMessage world;
    world.op = Net::ServerOpcode::WorldInfo;
    world.seed = config_.seed;
    SendTo(*client, world);

    Net::ServerMessage welcome;
    welcome.op = Net::ServerOpcode::Talk;
    welcome.text = "Welcome to CnaCraftServer! Type /help for commands.";
    SendTo(*client, welcome);

    for (const auto& [otherId, other] : clients_) {
        if (otherId == client->id) continue;
        Net::ServerMessage nick;
        nick.op = Net::ServerOpcode::Nick;
        nick.id = otherId;
        nick.text = other->nick;
        SendTo(*client, nick);
        if (other->hasPosition) {
            Net::ServerMessage pos;
            pos.op = Net::ServerOpcode::Position;
            pos.id = otherId;
            pos.x = other->x;
            pos.y = other->y;
            pos.z = other->z;
            pos.rx = other->rx;
            pos.ry = other->ry;
            SendTo(*client, pos);
        }
    }

    Net::ServerMessage announce;
    announce.op = Net::ServerOpcode::Nick;
    announce.id = client->id;
    announce.text = client->nick;
    Broadcast(announce, client->id);

    Net::ServerMessage joined;
    joined.op = Net::ServerOpcode::Talk;
    joined.text = client->nick + " has joined the game.";
    Broadcast(joined, client->id);

    std::printf("[server] client %d connected (%s)\n", client->id, client->nick.c_str());
}

void GameServer::HandleLeave(int clientId) {
    const auto it = clients_.find(clientId);
    if (it == clients_.end()) return;
    std::shared_ptr<Client> client = it->second;
    clients_.erase(it);
    zombies_.push_back(client);  // joined + freed in Stop() -- see the class comment

    Net::ServerMessage gone;
    gone.op = Net::ServerOpcode::Disconnect;
    gone.id = clientId;
    Broadcast(gone);

    Net::ServerMessage talk;
    talk.op = Net::ServerOpcode::Talk;
    talk.text = client->nick + " has disconnected.";
    Broadcast(talk);

    std::printf("[server] client %d disconnected (%s)\n", clientId, client->nick.c_str());
}

void GameServer::HandleMessage(Client& client, const Net::ClientMessage& msg) {
    // Until the version handshake passes, only V is honored -- everything
    // else is silently dropped (server.py refuses unversioned traffic too).
    if (!client.versionOk) {
        if (msg.op != Net::ClientOpcode::Version) return;
        if (msg.version != Net::kProtocolVersion) {
            Net::ServerMessage reject;
            reject.op = Net::ServerOpcode::Talk;
            reject.text = "Protocol version mismatch (server " + std::to_string(Net::kProtocolVersion) +
                          ", client " + std::to_string(msg.version) + ") -- disconnecting.";
            SendTo(client, reject);
            // Give the service thread one flush cycle, then cut the cord.
            client.alive = false;
            return;
        }
        client.versionOk = true;
        return;
    }

    switch (msg.op) {
        case Net::ClientOpcode::Version:
            break;  // idempotent, like server.py's on_version
        case Net::ClientOpcode::Nick: {
            const std::string nick = SanitizeNick(msg.text);
            if (nick.empty()) break;  // keep the guest name
            client.nick = nick;
            Net::ServerMessage announce;
            announce.op = Net::ServerOpcode::Nick;
            announce.id = client.id;
            announce.text = client.nick;
            Broadcast(announce);
            break;
        }
        case Net::ClientOpcode::Position: {
            client.x = msg.x;
            client.y = msg.y;
            client.z = msg.z;
            client.rx = msg.rx;
            client.ry = msg.ry;
            client.hasPosition = true;
            Net::ServerMessage pos;
            pos.op = Net::ServerOpcode::Position;
            pos.id = client.id;
            pos.x = msg.x;
            pos.y = msg.y;
            pos.z = msg.z;
            pos.rx = msg.rx;
            pos.ry = msg.ry;
            Broadcast(pos, client.id);
            break;
        }
        case Net::ClientOpcode::Talk:
            HandleTalk(client, msg.text);
            break;
        case Net::ClientOpcode::ChunkRequest:
        case Net::ClientOpcode::Block:
        case Net::ClientOpcode::Light:
        case Net::ClientOpcode::Sign:
            // World sync lands in M3 (MULTIPLAYER_PLAN.md §10).
            break;
    }
}

void GameServer::HandleTalk(Client& client, const std::string& text) {
    if (text.empty()) return;
    if (text[0] == '/') {
        // Server-side commands, ported from server.py's patterns.
        if (text == "/list") {
            std::string names;
            for (const auto& [id, other] : clients_) {
                (void)id;
                if (!names.empty()) names += ", ";
                names += other->nick;
            }
            Net::ServerMessage reply;
            reply.op = Net::ServerOpcode::Talk;
            reply.text = "Players online: " + names;
            SendTo(client, reply);
            return;
        }
        if (text.rfind("/nick ", 0) == 0) {
            Net::ClientMessage nickMsg;
            nickMsg.op = Net::ClientOpcode::Nick;
            nickMsg.text = text.substr(6);
            HandleMessage(client, nickMsg);
            return;
        }
        if (text == "/help") {
            Net::ServerMessage reply;
            reply.op = Net::ServerOpcode::Talk;
            reply.text = "Commands: /list, /nick NAME, /help";
            SendTo(client, reply);
            return;
        }
        Net::ServerMessage reply;
        reply.op = Net::ServerOpcode::Talk;
        reply.text = "Unknown server command: " + text;
        SendTo(client, reply);
        return;
    }

    // Plain chat -- Craft's "nick> text" format, broadcast to everyone
    // including the sender (server.py's send_talk does the same, which is
    // how the sender sees their own line).
    Net::ServerMessage chat;
    chat.op = Net::ServerOpcode::Talk;
    chat.text = client.nick + "> " + text;
    Broadcast(chat);
}

void GameServer::SendTo(Client& client, const Net::ServerMessage& msg) {
    client.outbox.Enqueue(Net::FormatServerLine(msg));
}

void GameServer::Broadcast(const Net::ServerMessage& msg, int exceptId) {
    const std::string line = Net::FormatServerLine(msg);
    for (auto& [id, client] : clients_) {
        if (id == exceptId) continue;
        client->outbox.Enqueue(line);
    }
}

double GameServer::ElapsedSeconds() const {
    // The shared world clock starts at dayLength/3 (mid-morning), the same
    // offset single-player uses (Craft's glfwSetTime(day_length/3)).
    return static_cast<double>(config_.dayLengthSeconds) / 3.0 +
           static_cast<double>(NowTicks() - startTicks_) / 1000.0;
}

}
