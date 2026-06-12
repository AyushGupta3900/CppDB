#include "network/TCPServer.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace cppdb {

namespace {

void setNonBlocking(int fd) {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

[[noreturn]] void throwErrno(const std::string& what) {
    throw std::runtime_error("TCPServer: " + what + ": " + std::strerror(errno));
}

}  // namespace

TCPServer::TCPServer(std::uint16_t port, Database& db, std::size_t workerCount)
    : executor_(db), pool_(std::make_unique<ThreadPool>(workerCount)) {
    listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) throwErrno("socket");

    const int enable = 1;
    ::setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    if (::bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        throwErrno("bind to port " + std::to_string(port));
    }

    socklen_t addrLen = sizeof(addr);
    ::getsockname(listenFd_, reinterpret_cast<sockaddr*>(&addr), &addrLen);
    port_ = ntohs(addr.sin_port);  // the real port when 0 was requested

    if (::listen(listenFd_, SOMAXCONN) < 0) throwErrno("listen");
    setNonBlocking(listenFd_);

    if (::pipe(wakePipe_) < 0) throwErrno("pipe");
    setNonBlocking(wakePipe_[0]);

    kqueueFd_ = ::kqueue();
    if (kqueueFd_ < 0) throwErrno("kqueue");

    std::array<struct kevent, 2> changes{};
    EV_SET(&changes[0], listenFd_, EVFILT_READ, EV_ADD, 0, 0, nullptr);
    EV_SET(&changes[1], wakePipe_[0], EVFILT_READ, EV_ADD, 0, 0, nullptr);
    if (::kevent(kqueueFd_, changes.data(), 2, nullptr, 0, nullptr) < 0) {
        throwErrno("kevent register");
    }
}

TCPServer::~TCPServer() {
    pool_.reset();  // drain in-flight queries first: workers may still write
    for (auto& [fd, conn] : connections_) ::close(fd);
    if (listenFd_ >= 0) ::close(listenFd_);
    if (kqueueFd_ >= 0) ::close(kqueueFd_);
    if (wakePipe_[0] >= 0) ::close(wakePipe_[0]);
    if (wakePipe_[1] >= 0) ::close(wakePipe_[1]);
}

void TCPServer::stop() {
    running_.store(false);
    const char byte = 'x';
    (void)::write(wakePipe_[1], &byte, 1);  // wake the kevent wait
}

void TCPServer::run() {
    running_.store(true);
    std::array<struct kevent, 64> events;

    while (running_.load()) {
        const int n = ::kevent(kqueueFd_, nullptr, 0, events.data(),
                               static_cast<int>(events.size()), nullptr);
        if (n < 0) {
            if (errno == EINTR) continue;
            throwErrno("kevent wait");
        }
        for (int i = 0; i < n; ++i) {
            const int fd = static_cast<int>(events[i].ident);
            if (fd == wakePipe_[0]) {
                std::array<char, 64> drain;
                while (::read(wakePipe_[0], drain.data(), drain.size()) > 0) {
                }
                continue;  // running_ is re-checked by the loop condition
            }
            if (fd == listenFd_) {
                acceptClients();
                continue;
            }
            if (auto it = connections_.find(fd); it != connections_.end()) {
                handleReadable(it->second);
            }
        }
    }
}

void TCPServer::acceptClients() {
    while (true) {
        const int clientFd = ::accept(listenFd_, nullptr, nullptr);
        if (clientFd < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) return;
            if (errno == EINTR) continue;
            return;  // transient accept failure; keep serving
        }
        setNonBlocking(clientFd);
        const int enable = 1;  // write to a dead peer -> EPIPE, not SIGPIPE
        ::setsockopt(clientFd, SOL_SOCKET, SO_NOSIGPIPE, &enable, sizeof(enable));

        struct kevent change;
        EV_SET(&change, clientFd, EVFILT_READ, EV_ADD, 0, 0, nullptr);
        if (::kevent(kqueueFd_, &change, 1, nullptr, 0, nullptr) < 0) {
            ::close(clientFd);
            continue;
        }
        connections_.emplace(clientFd, std::make_shared<Connection>(clientFd));
    }
}

void TCPServer::handleReadable(const std::shared_ptr<Connection>& conn) {
    std::array<char, 4096> buffer;
    bool peerClosed = false;

    while (true) {
        const ssize_t got = ::read(conn->fd, buffer.data(), buffer.size());
        if (got > 0) {
            conn->inbox.append(buffer.data(), static_cast<std::size_t>(got));
            continue;
        }
        if (got == 0) peerClosed = true;                       // EOF
        if (got < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) break;
        if (got < 0 && errno == EINTR) continue;
        if (got < 0 && !peerClosed) peerClosed = true;          // hard error
        break;
    }

    // Split complete lines out of the inbox and queue them on the strand.
    bool shouldDispatch = false;
    bool overLimit = false;
    std::size_t newline;
    while ((newline = conn->inbox.find('\n')) != std::string::npos) {
        std::string line = conn->inbox.substr(0, newline);
        conn->inbox.erase(0, newline + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();  // telnet
        if (line.empty()) continue;
        if (line.size() > kMaxLineBytes) {
            overLimit = true;
            break;
        }
        std::lock_guard<std::mutex> guard(conn->mutex);
        if (conn->pending.size() >= kMaxPendingQueries) {
            overLimit = true;
            break;
        }
        conn->pending.push_back(std::move(line));
        if (!conn->busy) {
            conn->busy = true;
            shouldDispatch = true;
        }
    }
    // A newline-free buffer past the limit can never become a legal query.
    if (conn->inbox.size() > kMaxLineBytes) overLimit = true;

    if (shouldDispatch) dispatch(conn);
    if (peerClosed || overLimit) dropConnection(conn->fd);
}

// One worker task drains a connection's queue in order; busy ensures at most
// one task per connection is ever in flight (per-connection serialization).
void TCPServer::dispatch(std::shared_ptr<Connection> conn) {
    pool_->submit([this, conn = std::move(conn)] {
        while (true) {
            std::string query;
            {
                std::lock_guard<std::mutex> guard(conn->mutex);
                if (conn->pending.empty()) {
                    conn->busy = false;
                    if (conn->closed) ::close(conn->fd);  // deferred close
                    return;
                }
                query = std::move(conn->pending.front());
                conn->pending.pop_front();
            }
            const std::string reply = executor_.execute(query) + "\n";
            if (!writeAll(conn->fd, reply)) {
                std::lock_guard<std::mutex> guard(conn->mutex);
                conn->busy = false;
                conn->pending.clear();
                if (conn->closed) ::close(conn->fd);
                return;  // peer is gone; the loop will (or did) drop us
            }
        }
    });
}

void TCPServer::dropConnection(int fd) {
    const auto it = connections_.find(fd);
    if (it == connections_.end()) return;
    std::shared_ptr<Connection> conn = it->second;
    connections_.erase(it);

    struct kevent change;
    EV_SET(&change, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    ::kevent(kqueueFd_, &change, 1, nullptr, 0, nullptr);

    // If a worker is mid-reply, let it finish and close the fd itself —
    // closing now could race the write (and the fd number could be reused).
    std::lock_guard<std::mutex> guard(conn->mutex);
    conn->closed = true;
    if (!conn->busy) ::close(fd);
}

bool TCPServer::writeAll(int fd, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, 0);
        if (n > 0) {
            sent += static_cast<std::size_t>(n);
            continue;
        }
        if (n < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
            pollfd waiter{fd, POLLOUT, 0};
            if (::poll(&waiter, 1, 5000) <= 0) return false;  // stuck peer
            continue;
        }
        if (n < 0 && errno == EINTR) continue;
        return false;  // EPIPE, ECONNRESET, ...
    }
    return true;
}

}  // namespace cppdb
