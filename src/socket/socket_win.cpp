#ifdef _WIN32

#include <array>
#include <stdexcept>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#include <stdexcept>

struct WSAInit {
    WSAInit() {
        WSADATA data;
        WORD    version = MAKEWORD(2, 2);
        int     err     = WSAStartup(version, &data);
        if (err != 0) {
            throw std::runtime_error("WSAStartup failed: " + std::to_string(err));
        }
    }
    ~WSAInit() { WSACleanup(); }
};

static WSAInit wsa_init;

#include "socket.hpp"

Socket::Socket() : m_fd(INVALID_SOCKET) {}
Socket::Socket(socket_t fd) : m_fd(fd) {}
Socket::~Socket() {
    close();
}

Socket::Socket(Socket&& other) noexcept : m_fd(other.m_fd) {
    other.m_fd = INVALID_SOCKET_FD;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        m_fd       = other.m_fd;
        other.m_fd = INVALID_SOCKET_FD;
    }
    return *this;
}

void Socket::create() {
    m_fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_fd == INVALID_SOCKET_FD)
        throw std::runtime_error("socket creation failed: " + std::to_string(WSAGetLastError()));
}

void Socket::close() {
    if (m_fd != INVALID_SOCKET_FD) {
        closesocket(m_fd);
        m_fd = INVALID_SOCKET_FD;
    }
}

bool Socket::valid() const {
    return m_fd != INVALID_SOCKET_FD;
}

socket_t Socket::fd() const {
    return m_fd;
}

socket_t Socket::release() {
    int fd = m_fd;
    m_fd   = INVALID_SOCKET_FD;
    return fd;
}

void Socket::setReuseAddr(bool enable) {
    BOOL opt = enable ? TRUE : FALSE;

    setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
}

void Socket::setNonBlocking(bool enable) {
    u_long mode = enable ? 1 : 0;
    ioctlsocket(m_fd, FIONBIO, &mode);
}

void Socket::bind(uint16_t port) {
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if (::bind(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error("bind failed");
}

void Socket::listen(int backlog) {
    if (::listen(m_fd, backlog) < 0)
        throw std::runtime_error("listen failed");
}

Socket Socket::accept() {
    socket_t client_fd = ::accept(m_fd, nullptr, nullptr);
    if (client_fd == INVALID_SOCKET) {
        throw std::runtime_error("accept failed: " + std::to_string(WSAGetLastError()));
    }
    return Socket(client_fd);
}

void Socket::connect(const std::string& host, uint16_t port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0)
        throw std::runtime_error("invalid address");
    if (::connect(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error("connect failed");
}

socket_size_t Socket::recv(void* buffer, size_t size) {
    if (m_fd == INVALID_SOCKET_FD)
        throw std::runtime_error("recv on invalid socket");

    socket_size_t bytes = ::recv(m_fd, static_cast<char*>(buffer), static_cast<int>(size), 0);
    if (bytes < 0) {
        if (WSAGetLastError() == WSAEWOULDBLOCK)
            return 0; // not an error, just no data
        throw std::runtime_error("recv failed");
    }
    return bytes;
}

socket_size_t Socket::recv(std::string& out, size_t max_size) {
    std::vector<char> buffer(max_size);
    socket_size_t     bytes = recv(buffer.data(), buffer.size());
    if (bytes > 0) {
        out.assign(buffer.data(), bytes);
    }
    return bytes;
}
socket_size_t Socket::send(const void* data, size_t size) {
    if (m_fd == INVALID_SOCKET_FD)
        throw std::runtime_error("send on invalid socket");

    socket_size_t sent = ::send(m_fd, static_cast<const char*>(data), static_cast<int>(size), 0);
    if (sent < 0) {
        if (WSAGetLastError() == WSAEWOULDBLOCK) {
            return 0;
        }
        throw std::runtime_error("send failed");
    }
    return sent;
}
socket_size_t Socket::send(const std::string& data) {
    return send(data.data(), data.size());
}

#endif