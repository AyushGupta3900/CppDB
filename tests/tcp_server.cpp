// End-to-end test: real sockets against a live server on an ephemeral port.

#include "network/TCPServer.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string>
#include <thread>
#include <vector>

#include "core/Database.hpp"
#include "test_framework.hpp"

using cppdb::Database;
using cppdb::TCPServer;

namespace {

// Minimal blocking client speaking the newline protocol.
class Client {
public:
    explicit Client(std::uint16_t port) {
        fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(port);
        connected_ =
            ::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
    }

    ~Client() {
        if (fd_ >= 0) ::close(fd_);
    }

    bool connected() const { return connected_; }

    void send(const std::string& line) {
        const std::string framed = line + "\n";
        (void)::send(fd_, framed.data(), framed.size(), 0);
    }

    // Reads one '\n'-terminated line (without the newline).
    std::string readLine() {
        std::string line;
        char c = 0;
        while (::read(fd_, &c, 1) == 1) {
            if (c == '\n') return line;
            line += c;
        }
        return line;  // connection closed
    }

private:
    int fd_ = -1;
    bool connected_ = false;
};

}  // namespace

int main() {
    Database db;
    TCPServer server(/*port=*/0, db, /*workerCount=*/4);  // ephemeral port
    CHECK(server.port() != 0);
    std::thread serverThread([&server] { server.run(); });

    {
        // Basic round trips on one connection
        Client client(server.port());
        CHECK(client.connected());
        client.send("CREATE TABLE users (id INT, name TEXT)");
        CHECK_EQ(client.readLine(), "OK");
        client.send("INSERT INTO users (id, name) VALUES (1, 'Alice')");
        CHECK_EQ(client.readLine(), "OK");
        client.send("SELECT * FROM users WHERE id = 1");
        CHECK_EQ(client.readLine(), "id=1, name=Alice");
        CHECK_EQ(client.readLine(), "(1 row)");

        // Errors come back as lines, the connection survives
        client.send("SELECT * FROM missing");
        CHECK_EQ(client.readLine(), "ERROR: no such table 'missing'");
        client.send("not even sql");
        CHECK(client.readLine().rfind("ERROR:", 0) == 0);
        client.send("SELECT * FROM users");
        CHECK_EQ(client.readLine(), "id=1, name=Alice");
        CHECK_EQ(client.readLine(), "(1 row)");

        // Pipelining: replies must come back in submission order (strand)
        client.send("INSERT INTO users (id, name) VALUES (2, 'Bob')");
        client.send("INSERT INTO users (id, name) VALUES (2, 'Bob')");  // dup
        client.send("SELECT name FROM users WHERE id = 2");
        CHECK_EQ(client.readLine(), "OK");
        CHECK(client.readLine().rfind("ERROR: duplicate id 2", 0) == 0);
        CHECK_EQ(client.readLine(), "name=Bob");
        CHECK_EQ(client.readLine(), "(1 row)");
    }  // client disconnects; server must keep running

    {
        // A second client sees the same data (shared Database)
        Client other(server.port());
        CHECK(other.connected());
        other.send("SELECT name FROM users WHERE id = 1");
        CHECK_EQ(other.readLine(), "name=Alice");
        CHECK_EQ(other.readLine(), "(1 row)");
    }

    {
        // Many concurrent clients writing disjoint rows
        const int clientCount = 8;
        std::vector<std::thread> threads;
        for (int c = 0; c < clientCount; ++c) {
            threads.emplace_back([&server, c] {
                Client worker(server.port());
                if (!worker.connected()) {
                    CHECK(false);
                    return;
                }
                for (int i = 0; i < 25; ++i) {
                    const int id = 100 + c * 25 + i;
                    worker.send("INSERT INTO users (id, name) VALUES (" +
                                std::to_string(id) + ", 'u" + std::to_string(id) +
                                "')");
                    if (worker.readLine() != "OK") {
                        CHECK(false);
                        return;
                    }
                }
            });
        }
        for (auto& t : threads) t.join();

        Client checker(server.port());
        checker.send("SELECT * FROM users WHERE id >= 100");
        int lines = 0;
        std::string last;
        while (true) {
            last = checker.readLine();
            if (last.rfind("(", 0) == 0 || last.empty()) break;
            ++lines;
        }
        CHECK_EQ(lines, clientCount * 25);
        CHECK_EQ(last, "(200 rows)");
    }

    {
        // A line over the limit gets the connection dropped...
        Client flooder(server.port());
        CHECK(flooder.connected());
        const std::string giant(TCPServer::kMaxLineBytes + 1024, 'x');
        flooder.send(giant);
        CHECK_EQ(flooder.readLine(), "");  // server hung up on us

        // ...but the server itself keeps serving other clients
        Client survivor(server.port());
        survivor.send("SELECT name FROM users WHERE id = 1");
        CHECK_EQ(survivor.readLine(), "name=Alice");
        CHECK_EQ(survivor.readLine(), "(1 row)");
    }

    server.stop();
    serverThread.join();
    CHECK(true);  // clean shutdown reached
    return testfw::testSummary("tcp_server");
}
