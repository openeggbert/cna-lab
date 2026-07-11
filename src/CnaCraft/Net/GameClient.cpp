#include "GameClient.hpp"

#include <exception>

#include "System/Net/Sockets/AddressFamily.hpp"
#include "System/Net/Sockets/ProtocolType.hpp"
#include "System/Net/Sockets/SocketType.hpp"

namespace CnaCraft::Net {

namespace {
// Reader wake-up cadence while idle. 200ms keeps shutdown latency well
// under human perception without busy-spinning; actual message latency is
// unaffected (Poll returns the moment data arrives).
constexpr long kReaderPollMicros = 200'000;
}

bool GameClient::Connect(const std::string& host, int port) {
    Stop();  // re-Connect after a drop reuses this object cleanly
    dropped_ = false;
    lastError_.clear();
    try {
        socket_ = std::make_shared<System::Net::Sockets::Socket>(
            System::Net::Sockets::AddressFamily::InterNetwork, System::Net::Sockets::SocketType::Stream,
            System::Net::Sockets::ProtocolType::Tcp);
        socket_->Connect(host, port);
    } catch (const std::exception& e) {
        lastError_ = e.what();
        socket_.reset();
        return false;
    } catch (...) {
        lastError_ = "unknown connect error";
        socket_.reset();
        return false;
    }
    line_ = std::make_unique<LineSocket>(socket_);
    running_ = true;
    connected_ = true;
    readerThread_ = std::make_unique<System::Threading::Thread>([this]() { ReaderLoop(); });
    readerThread_->Start();
    return true;
}

void GameClient::Stop() {
    running_ = false;
    if (line_) line_->ShutdownBoth();  // wakes a blocked Poll/Receive with EOF
    if (readerThread_) {
        readerThread_->Join();  // Thread's destructor only detaches -- join explicitly
        readerThread_.reset();
    }
    if (line_) {
        line_->Close();  // safe now: the reader is gone
        line_.reset();
    }
    socket_.reset();
    connected_ = false;
}

bool GameClient::Send(const ClientMessage& msg) {
    if (!line_ || dropped_) return false;
    if (line_->SendLine(FormatClientLine(msg))) return true;
    dropped_ = true;
    if (lastError_.empty()) lastError_ = "send failed (connection lost)";
    return false;
}

bool GameClient::TryReceive(ServerMessage& out) { return inbox_.TryDequeue(out); }

void GameClient::ReaderLoop() {
    std::vector<std::string> lines;
    while (running_) {
        if (!line_->ReadLines(lines, kReaderPollMicros)) {
            // Peer closed or socket error. If we initiated the stop this is
            // just the expected wake-up; otherwise it's a real drop.
            if (running_) {
                dropped_ = true;
                if (lastError_.empty()) lastError_ = "connection closed by server";
            }
            break;
        }
        for (const std::string& raw : lines) {
            if (auto msg = ParseServerLine(raw)) inbox_.Enqueue(*msg);
            // Unparseable lines are dropped silently -- same tolerance as
            // Craft's sscanf-based parse_buffer, which skips non-matching
            // lines rather than treating them as fatal.
        }
        lines.clear();
    }
}

}
