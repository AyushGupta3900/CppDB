#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "concurrency/ThreadPool.hpp"
#include "core/Database.hpp"
#include "query/QueryExecutor.hpp"

namespace cppdb {

// Exposes the database over TCP with a newline-delimited protocol: every
// query is one line in, every reply is newline-terminated (replies may span
// multiple lines; the row-count footer marks the end of a SELECT). Works
// with `nc localhost 5432` or telnet.
//
// Architecture: one event-loop thread multiplexes all sockets with kqueue
// (the BSD/macOS equivalent of epoll) over non-blocking fds. Complete lines
// are handed to the worker pool; per connection, queries run strictly one at
// a time in arrival order (a "strand"), so concurrent clients spread across
// workers but no client ever sees interleaved or reordered replies.
//
// The constructor binds and listens (throws std::runtime_error on failure);
// run() blocks in the event loop; stop() is thread- and signal-safe. Callers
// must ensure run() has returned before destroying the server.
class TCPServer {
public:
    // port 0 picks an ephemeral port — read it back with port().
    TCPServer(std::uint16_t port, Database& db, std::size_t workerCount = 4);
    ~TCPServer();

    TCPServer(const TCPServer&) = delete;
    TCPServer& operator=(const TCPServer&) = delete;

    void run();
    void stop();  // async-signal-safe: one atomic store + one pipe write

    std::uint16_t port() const noexcept { return port_; }

private:
    struct Connection {
        explicit Connection(int descriptor) : fd(descriptor) {}
        const int fd;
        std::string inbox;                // bytes received, not yet line-split
        std::deque<std::string> pending;  // complete queries awaiting execution
        bool busy = false;                // a worker task is draining pending
        bool closed = false;              // event loop saw EOF / dropped us
        std::mutex mutex;                 // guards pending/busy/closed
    };

    void acceptClients();
    void handleReadable(const std::shared_ptr<Connection>& conn);
    void dispatch(std::shared_ptr<Connection> conn);
    void dropConnection(int fd);

    // Blocking write on a non-blocking fd (polls on EAGAIN). False if the
    // peer is gone.
    static bool writeAll(int fd, const std::string& data);

    QueryExecutor executor_;
    int listenFd_ = -1;
    int kqueueFd_ = -1;
    int wakePipe_[2] = {-1, -1};
    std::uint16_t port_ = 0;
    std::atomic<bool> running_{false};
    std::unordered_map<int, std::shared_ptr<Connection>> connections_;  // loop thread only
    std::unique_ptr<ThreadPool> pool_;  // unique_ptr: destroyed (joined) before fds close
};

}  // namespace cppdb
