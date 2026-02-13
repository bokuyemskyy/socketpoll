#ifndef _WIN32

#include "socket.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

Socket::Socket() : m_fd(INVALID_SOCKET_FD) {}
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
    m_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_fd < 0)
        throw std::runtime_error("socket creation failed");
}

void Socket::close() {
    if (m_fd != INVALID_SOCKET_FD) {
        ::close(m_fd);
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
    int opt = enable ? 1 : 0;
    if (::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        throw std::runtime_error("setsockopt(SO_REUSEADDR) failed");
}

void Socket::setNonBlocking(bool enable) {
    int flags = fcntl(m_fd, F_GETFL, 0);
    if (flags == -1)
        throw std::runtime_error("fcntl(F_GETFL) failed");
    if (enable)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;
    if (fcntl(m_fd, F_SETFL, flags) == -1)
        throw std::runtime_error("fcntl(F_SETFL) failed");
}

void Socket::bind(uint16_t port) {
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if (::bind(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error("bind failed: " + std::string(strerror(errno)));
}

void Socket::listen(int backlog) {
    if (::listen(m_fd, backlog) < 0)
        throw std::runtime_error("listen failed: " + std::string(strerror(errno)));
}

Socket Socket::accept() {
    int client_fd = ::accept(m_fd, nullptr, nullptr);
    if (client_fd < 0)
        throw std::runtime_error("accept failed: " + std::string(strerror(errno)));
    return Socket(client_fd);
}

void Socket::connect(const std::string& host, uint16_t port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0)
        throw std::runtime_error("invalid address");
    if (::connect(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error("connect failed: " + std::string(strerror(errno)));
}

socket_size_t Socket::recv(void* buffer, size_t size) {
    if (m_fd == INVALID_SOCKET_FD)
        throw std::runtime_error("recv on invalid socket");

    ssize_t bytes = ::recv(m_fd, buffer, size, 0);
    if (bytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0; // not an error, just no data
        throw std::runtime_error("recv failed: " + std::string(strerror(errno)));
    }
    return bytes;
}

socket_size_t Socket::recv(std::string& out, size_t max_size) {
    std::vector<char> buffer(max_size);
    ssize_t           bytes = recv(buffer.data(), buffer.size());
    if (bytes > 0) {
        out.assign(buffer.data(), bytes);
    }
    return bytes;
}
socket_size_t Socket::send(const void* data, size_t size) {
    if (m_fd == INVALID_SOCKET_FD)
        throw std::runtime_error("send on invalid socket");

    ssize_t sent = ::send(m_fd, data, size, 0);
    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        throw std::runtime_error("send failed: " + std::string(strerror(errno)));
    }
    return sent;
}
socket_size_t Socket::send(const std::string& data) {
    return send(data.data(), data.size());
}

#endif