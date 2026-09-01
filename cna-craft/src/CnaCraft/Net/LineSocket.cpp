#include "LineSocket.hpp"

#include "System/Net/Sockets/SelectMode.hpp"
#include "System/Net/Sockets/SocketShutdown.hpp"

namespace CnaCraft::Net {

namespace {
// Craft's own RECV_SIZE (client.c) -- one recv's worth of protocol text.
constexpr int kRecvChunk = 4096;
}

LineSocket::LineSocket(std::shared_ptr<System::Net::Sockets::Socket> socket)
    : socket_(std::move(socket)), scratch_(kRecvChunk) {}

bool LineSocket::ReadLines(std::vector<std::string>& out, long timeoutMicros) {
    try {
        if (!socket_->Poll(timeoutMicros, System::Net::Sockets::SelectMode::SelectRead)) {
            return true;  // timeout -- no data yet, connection still fine
        }
        const auto received = socket_->Receive(scratch_);
        if (received <= 0) return false;  // orderly close (or shutdown() from our own stopper)
        recvBuffer_.append(reinterpret_cast<const char*>(scratch_.data()), static_cast<std::size_t>(received));
    } catch (...) {
        return false;  // socket error -- connection is over
    }

    std::size_t start = 0;
    for (;;) {
        const std::size_t newline = recvBuffer_.find('\n', start);
        if (newline == std::string::npos) break;
        std::string line = recvBuffer_.substr(start, newline - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) out.push_back(std::move(line));
        start = newline + 1;
    }
    if (start > 0) recvBuffer_.erase(0, start);
    return true;
}

bool LineSocket::SendLine(const std::string& line) {
    std::lock_guard<std::mutex> lock(sendMutex_);
    std::vector<SharpRuntime::bytecs> bytes(line.begin(), line.end());
    bytes.push_back(static_cast<SharpRuntime::bytecs>('\n'));
    try {
        // Socket::Send loops internally per call but may still report a
        // short send on a full kernel buffer -- loop until everything went.
        std::size_t sentTotal = 0;
        while (sentTotal < bytes.size()) {
            const auto sent = socket_->Send(bytes, static_cast<int>(sentTotal),
                                            static_cast<int>(bytes.size() - sentTotal),
                                            System::Net::Sockets::SocketFlags::None);
            if (sent <= 0) return false;
            sentTotal += static_cast<std::size_t>(sent);
        }
        return true;
    } catch (...) {
        return false;
    }
}

void LineSocket::ShutdownBoth() {
    try {
        socket_->Shutdown(System::Net::Sockets::SocketShutdown::Both);
    } catch (...) {
        // Already closed/reset -- shutting down was the goal anyway.
    }
}

void LineSocket::Close() {
    try {
        socket_->Close();
    } catch (...) {
    }
}

}
