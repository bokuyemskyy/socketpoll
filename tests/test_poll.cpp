#include "event_poll.hpp"
#include "socket.hpp"

#include <chrono>
#include <netinet/in.h>
#include <thread>
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <catch2/catch_test_macros.hpp>

uint16_t findAvailablePort();

TEST_CASE("EventPoll: Construction") {
    SECTION("Default construction") {
        REQUIRE_NOTHROW(EventPoll());
    }

    SECTION("Construction with max events") {
        REQUIRE_NOTHROW(EventPoll(100));
    }
}

TEST_CASE("EventPoll: Add, Modify, Remove") {
    EventPoll poll;
    Socket    s;
    s.create();

    SECTION("Add fd for reading") {
        REQUIRE_NOTHROW(poll.addFd(s.fd(), PollEvent::READ));
    }

    SECTION("Add fd for writing") {
        REQUIRE_NOTHROW(poll.addFd(s.fd(), PollEvent::WRITE));
    }

    SECTION("Add fd for read and write") {
        PollEvent events = static_cast<PollEvent>(PollEvent::READ | PollEvent::WRITE);
        REQUIRE_NOTHROW(poll.addFd(s.fd(), events));
    }

    SECTION("Modify fd events") {
        poll.addFd(s.fd(), PollEvent::READ);
        REQUIRE_NOTHROW(poll.modifyFd(s.fd(), PollEvent::WRITE));
    }

    SECTION("Remove fd") {
        poll.addFd(s.fd(), PollEvent::READ);
        REQUIRE_NOTHROW(poll.removeFd(s.fd()));
    }
}

TEST_CASE("EventPoll: Events") {
    uint16_t port = findAvailablePort();

    SECTION("Wait for read event") {
        Socket server;
        server.create();
        server.setReuseAddr(true);
        server.bind(port);
        server.listen();

        EventPoll poll;
        poll.addFd(server.fd(), PollEvent::READ);

        std::thread client_thread([&]() {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
            Socket client;
            client.create();
            client.connect("127.0.0.1", port);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        });

        poll.wait(1000);

        const auto& events = poll.events();
        REQUIRE(events.size() > 0);
        REQUIRE(events[0].m_fd == server.fd());
        REQUIRE((events[0].m_events & PollEvent::READ) != 0);

        client_thread.join();
    }

    SECTION("Wait with timeout") {
        Socket s;
        s.create();

        EventPoll poll;
        poll.addFd(s.fd(), PollEvent::READ);

        auto start = std::chrono::steady_clock::now();
        poll.wait(100);
        auto end = std::chrono::steady_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        // should wait for approximately 100 ms
        REQUIRE(duration.count() >= 80);
        REQUIRE(duration.count() <= 200);
    }

    SECTION("Multiple fds in poll") {
        Socket server;
        server.create();
        server.setReuseAddr(true);
        server.bind(port);
        server.listen();

        Socket s2;
        s2.create();
        s2.setNonBlocking(true);

        // nonblocking throws EINPROGRESS (WSAEWOULDBLOCK on Windows)
        REQUIRE_THROWS(s2.connect("127.0.0.1", port));

        Socket s1 = server.accept();
        s1.setNonBlocking(true);

        EventPoll poll;
        poll.addFd(s1.fd(), PollEvent::READ);
        poll.addFd(s2.fd(), PollEvent::WRITE);

        // s2 should be writable immediately
        poll.wait(50);

        const auto& events = poll.events();

        REQUIRE(events.size() > 0);

        bool found_s2 = false;
        for (const auto& event : events) {
            if (event.m_fd == s2.fd()) {
                found_s2 = true;
                REQUIRE((event.m_events & PollEvent::WRITE) != 0);
            }
        }
        REQUIRE(found_s2);
    }
}

TEST_CASE("EventPoll: Integration with socket") {
    uint16_t port = findAvailablePort();

    SECTION("Detect incoming connection") {
        Socket server;
        server.create();
        server.setReuseAddr(true);
        server.bind(port);
        server.listen();

        EventPoll poll;
        poll.addFd(server.fd(), PollEvent::READ);

        std::thread client_thread([&]() {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
            Socket client;
            client.create();
            client.connect("127.0.0.1", port);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        });

        poll.wait(1000);

        const auto& events = poll.events();
        REQUIRE(events.size() > 0);

        Socket accepted = server.accept();
        REQUIRE(accepted.valid());

        client_thread.join();
    }

    SECTION("Detect data ready to read") {
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
            std::this_thread::sleep_for(std::chrono::microseconds(50));
            client.send("Test");
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        });

        Socket accepted = server.accept();

        EventPoll poll;
        poll.addFd(accepted.fd(), PollEvent::READ);

        poll.wait(1000);

        const auto& events = poll.events();
        REQUIRE(events.size() > 0);
        REQUIRE(events[0].m_fd == accepted.fd());

        std::string   data;
        socket_size_t received = accepted.recv(data);
        REQUIRE(received > 0);
        REQUIRE(data == "Test");

        client_thread.join();
    }
}