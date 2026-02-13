#pragma once

#include "socket.hpp"

#include <cstdint>
#include <memory>
#include <vector>

enum PollEvent : uint8_t {
    NONE  = 0,
    READ  = 1 << 0,
    WRITE = 1 << 1,
    ERR   = 1 << 2
};

class EventPoll {
  public:
    struct PollEventEntry {
        socket_t  m_fd;
        PollEvent m_events;
    };

    EventPoll(int max_events = 256);
    ~EventPoll();

    EventPoll(const EventPoll&)            = delete;
    EventPoll& operator=(const EventPoll&) = delete;

    EventPoll(EventPoll&&)            = delete;
    EventPoll& operator=(EventPoll&&) = delete;

    void addFd(socket_t fd, PollEvent event);
    void modifyFd(socket_t fd, PollEvent event);
    void removeFd(socket_t fd);
    void wait(int timeout_ms = -1);

    [[nodiscard]] const std::vector<PollEventEntry>& events() const;

  private:
    int m_max_events;

    struct Impl;
    std::unique_ptr<Impl> m_pimpl;
};