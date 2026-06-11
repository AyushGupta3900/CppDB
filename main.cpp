// CppDB server entry point.
//
//   ./build/cppdb [port]      (default port 5432)
//
// Talk to it with netcat or telnet:
//   nc localhost 5432
//   CREATE TABLE users (id INT, name TEXT)
//   INSERT INTO users (id, name) VALUES (1, 'Alice')
//   SELECT * FROM users WHERE id = 1

#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include "core/Database.hpp"
#include "network/TCPServer.hpp"

namespace {

cppdb::TCPServer* g_server = nullptr;

// stop() is async-signal-safe (atomic store + pipe write), so Ctrl-C shuts
// the event loop down cleanly instead of killing the process mid-write.
void handleSignal(int) {
    if (g_server != nullptr) g_server->stop();
}

}  // namespace

int main(int argc, char** argv) {
    std::uint16_t port = 5432;
    if (argc > 1) {
        const int parsed = std::atoi(argv[1]);
        if (parsed <= 0 || parsed > 65535) {
            std::cerr << "usage: " << argv[0] << " [port]\n";
            return EXIT_FAILURE;
        }
        port = static_cast<std::uint16_t>(parsed);
    }

    try {
        cppdb::Database db;
        cppdb::TCPServer server(port, db, /*workerCount=*/4);
        g_server = &server;
        std::signal(SIGINT, handleSignal);
        std::signal(SIGTERM, handleSignal);

        std::cout << "CppDB listening on 127.0.0.1:" << server.port()
                  << "  (connect with: nc localhost " << server.port() << ")\n";
        server.run();
        std::cout << "CppDB shut down cleanly.\n";
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
