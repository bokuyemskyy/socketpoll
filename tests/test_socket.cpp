#include "socket.hpp"
#include "test_utils.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
using socket_t = SOCKET;
#define CLOSE_SOCKET closesocket
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
#define CLOSE_SOCKET close
#endif

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <thread>

TEST_CASE("Socket: Construction and destruction") {
    SECTION("Default construction") {
        Socket s;
        REQUIRE_FALSE(s.valid());
        REQUIRE(s.fd() == INVALID_SOCKET_FD);
    }

    SECTION("Valid construction") {
        Socket temp;
        temp.create();
        socket_t fd = temp.release();

        Socket s(fd);
        REQUIRE(s.valid());
        REQUIRE(s.fd() == fd);
    }

    SECTION("Socket destuction") {
        socket_t fd;
        {
            Socket s;
            s.create();
            fd = s.fd();
            REQUIRE(s.valid());
        }
#ifdef _WIN32
        REQUIRE(send(fd, nullptr, 0, 0) == SOCKET_ERROR);
        REQUIRE(WSAGetLastError() == WSAENOTSOCK);
#else
        REQUIRE(fcntl(fd, F_GETFD) == -1);
        REQUIRE(errno == EBADF);
#endif
    }
}

TEST_CASE("Socket: Move semantics") {
    SECTION("Move construction") {
        Socket s1;
        s1.create();
        socket_t fd = s1.fd();
        REQUIRE(s1.valid());

        Socket s2(std::move(s1));
        REQUIRE(s2.valid());
        REQUIRE(s2.fd() == fd);
        REQUIRE_FALSE(s1.valid());
    }

    SECTION("Move assignment") {
        Socket s1;
        s1.create();
        socket_t fd = s1.fd();

        Socket s2;
        s2.create();
        socket_t old_fd = s2.fd();

        s2 = std::move(s1);
        REQUIRE(s2.valid());
        REQUIRE(s2.fd() == fd);
        REQUIRE_FALSE(s1.valid());
#ifdef _WIN32
        REQUIRE(send(old_fd, nullptr, 0, 0) == SOCKET_ERROR);
        REQUIRE(WSAGetLastError() == WSAENOTSOCK);
#else
        REQUIRE(fcntl(old_fd, F_GETFD) == -1);
        REQUIRE(errno == EBADF);
#endif
    }

    SECTION("Self-move assignment") {
        Socket s;
        s.create();
        socket_t fd = s.fd();

        s = std::move(s);
        REQUIRE(s.valid());
        REQUIRE(s.fd() == fd);
    }
}
TEST_CASE("Socket: Options") {
    SECTION("SetReuseAddr") {
        Socket s;
        s.create();

        REQUIRE_NOTHROW(s.setReuseAddr(true));

        uint16_t port = findAvailablePort();
        s.bind(port);
        s.close();

        Socket s2;
        s2.create();
        s2.setReuseAddr(true);
        REQUIRE_NOTHROW(s2.bind(port));
    }

    SECTION("SetNonBlocking") {
        Socket s;
        s.create();

        REQUIRE_NOTHROW(s.setNonBlocking(true));
    }
}

TEST_CASE("Socket: Bind and listen") {
    SECTION("Bind and listen with backlog") {
        Socket s;
        s.create();
        s.setReuseAddr(true);

        uint16_t port = findAvailablePort();
        REQUIRE_NOTHROW(s.bind(port));
        REQUIRE_NOTHROW(s.listen(10));
    }
}

TEST_CASE("Socket: Accept and connect") {
    uint16_t port = findAvailablePort();

    SECTION("Server accepts client connection") {
        Socket server;
        server.create();
        server.setReuseAddr(true);
        server.bind(port);
        server.listen();

        bool        client_connected = false;
        std::thread client_thread([&]() {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
            Socket client;
            client.create();
            REQUIRE_NOTHROW(client.connect("127.0.0.1", port));
            client_connected = true;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        });

        Socket accepted = server.accept();
        REQUIRE(accepted.valid());

        client_thread.join();
        REQUIRE(client_connected);
    }
}

TEST_CASE("Socket: Send and receive") {
    uint16_t port = findAvailablePort();

    SECTION("Send and receive raw") {
        Socket server;
        server.create();
        server.setReuseAddr(true);
        server.bind(port);
        server.listen();

        std::thread client_thread([&]() {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
            Socket client;
            client.create();
            client.connect("127.0.0.1", port);

            const char*   msg  = "Hello";
            socket_size_t sent = client.send(msg, strlen(msg));
            REQUIRE(sent == static_cast<socket_size_t>(strlen(msg)));

            std::this_thread::sleep_for(std::chrono::microseconds(100));
        });

        Socket accepted = server.accept();

        char          buffer[256] = {0};
        socket_size_t received    = accepted.recv(buffer, sizeof(buffer) - 1);
        REQUIRE(received > 0);
        REQUIRE(std::string(buffer) == "Hello");

        client_thread.join();
    }

    SECTION("Send and receive strings") {
        Socket server;
        server.create();
        server.setReuseAddr(true);
        server.bind(port);
        server.listen();

        std::thread client_thread([&]() {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
            Socket client;
            client.create();
            client.connect("127.0.0.1", port);

            std::string   msg  = "Hello";
            socket_size_t sent = client.send(msg);
            REQUIRE(sent == static_cast<socket_size_t>(msg.size()));

            std::this_thread::sleep_for(std::chrono::microseconds(100));
        });

        Socket accepted = server.accept();

        std::string   received_data;
        socket_size_t received = accepted.recv(received_data);
        REQUIRE(received > 0);
        REQUIRE(received_data == "Hello");

        client_thread.join();
    }

    SECTION("Bidirectional communication") {
        Socket server;
        server.create();
        server.setReuseAddr(true);
        server.bind(port);
        server.listen();

        std::thread client_thread([&]() {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
            Socket client;
            client.create();
            client.connect("127.0.0.1", port);

            std::string client_msg = "Client says hello";
            client.send(client_msg);

            std::string response;
            client.recv(response);
            REQUIRE(response == "Server says hello");
        });

        Socket accepted = server.accept();

        std::string client_data;
        accepted.recv(client_data);
        REQUIRE(client_data == "Client says hello");

        std::string server_msg = "Server says hello";
        accepted.send(server_msg);

        client_thread.join();
    }
}
