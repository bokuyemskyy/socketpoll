#pragma once

#include "socket.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#endif

inline uint16_t findAvailablePort() {
    Socket s;
    s.create();
    s.setReuseAddr(true);
    s.bind(0);

    sockaddr_in addr{};
#ifdef _WIN32
    int len = sizeof(addr);
#else
    socklen_t len = sizeof(addr);
#endif
    getsockname(s.fd(), reinterpret_cast<sockaddr*>(&addr), &len);
    return ntohs(addr.sin_port);
}