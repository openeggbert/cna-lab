#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "System/Net/Sockets/Socket.hpp"

// Newline-framed ASCII text over a sharp-runtime TCP Socket -- the transport
// under both GameClient (this dir) and CnaCraftServer's per-client handlers
// (MULTIPLAYER_PLAN.md §5/§6). Mirrors Craft's own model exactly: raw bytes
// accumulate in a buffer, complete '\n'-terminated lines split out on the
// consumer side (client.c's queue + client_recv), partial trailing lines
// stay buffered until the rest arrives.
//
// Concurrency contract: ReadLines is called by exactly ONE reader thread;
// SendLine may be called from any thread (internally serialized). Close is
// safe to call from a second thread to stop a blocked reader -- it uses
// shutdown() first (which wakes a pending Poll/Receive with EOF but keeps
// the fd allocated, avoiding the close-during-recv fd-reuse race), leaving
// the actual close() to the owner after the reader has been joined.
namespace CnaCraft::Net {

class LineSocket {
public:
    explicit LineSocket(std::shared_ptr<System::Net::Sockets::Socket> socket);

    // Waits up to timeoutMicros for readability, then appends every complete
    // line received (terminators stripped) to `out`. Returns false once the
    // connection is over -- peer closed, socket error, or Shutdown() from
    // another thread -- and true otherwise (including "timeout, no data").
    bool ReadLines(std::vector<std::string>& out, long timeoutMicros);

    // Appends '\n' and sends the whole line. Returns false on any send
    // error (connection is then effectively over). Thread-safe.
    bool SendLine(const std::string& line);

    // Wakes a blocked reader with EOF (shutdown both directions). Idempotent.
    void ShutdownBoth();

    // Actually closes the fd. Call only after the reader thread is done.
    void Close();

private:
    std::shared_ptr<System::Net::Sockets::Socket> socket_;
    std::string recvBuffer_;
    std::vector<SharpRuntime::bytecs> scratch_;
    std::mutex sendMutex_;
};

}
