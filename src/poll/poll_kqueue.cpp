#if defined(__APPLE__) || defined(__FreeBSD__)

#include "event_poll.hpp"

#include <mutex>
#include <sys/event.h>
#include <unistd.h>

struct EventPoll::Impl {
    socket_t m_kqueue_fd;

    std::vector<PollEventEntry> m_active_events;
    std::vector<struct kevent>  m_kernel_events;
    std::mutex                  m_mutex;

    Impl(int max_events) : m_kqueue_fd(kqueue()) { m_kernel_events.reserve(max_events); }

    ~Impl() {
        if (m_kqueue_fd != INVALID_SOCKET_FD)
            close(m_kqueue_fd);
    }

    static short toNative(PollEvent event) {
        if ((event & PollEvent::READ) != 0)
            return EVFILT_READ;
        if ((event & PollEvent::WRITE) != 0)
            return EVFILT_WRITE;
        throw std::runtime_error("kqueue event " + std::to_string(event) + " is not implemented");
    }
    static PollEvent fromNative(short native) {
        if (native == EVFILT_READ)
            return PollEvent::READ;
        if (native == EVFILT_WRITE)
            return PollEvent::WRITE;
        throw std::runtime_error("kqueue event " + std::to_string(native) + " is not implemented");
    }
};

EventPoll::EventPoll(int max_events) : m_pimpl(std::make_unique<Impl>(max_events)), m_max_events(max_events) {}
EventPoll::~EventPoll() = default;

void EventPoll::addFd(socket_t fd, PollEvent event) {
    std::unique_lock<std::mutex> lock(m_pimpl->m_mutex);
    struct kevent                changes[2];
    int                          n = 0;

    if ((event & PollEvent::READ) != 0) {
        EV_SET(&changes[n++], fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, (void*)(intptr_t)fd);
    }
    if ((event & PollEvent::WRITE) != 0) {
        EV_SET(&changes[n++], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, (void*)(intptr_t)fd);
    }

    if (n > 0 && kevent(m_pimpl->m_kqueue_fd, changes, n, NULL, 0, NULL) == -1) {
        throw std::runtime_error(strerror(errno));
    }
}

void EventPoll::modifyFd(socket_t fd, PollEvent event) {
    std::unique_lock<std::mutex> lock(m_pimpl->m_mutex);

    struct kevent changes[2];
    int           n = 0;

    if (event & PollEvent::READ) {
        EV_SET(&changes[n++], fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, (void*)(intptr_t)fd);
    } else {
        EV_SET(&changes[n++], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    }

    if (event & PollEvent::WRITE) {
        EV_SET(&changes[n++], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, (void*)(intptr_t)fd);
    } else {
        EV_SET(&changes[n++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    }

    kevent(m_pimpl->m_kqueue_fd, changes, n, NULL, 0, NULL);
}

void EventPoll::removeFd(socket_t fd) {
    std::unique_lock<std::mutex> lock(m_pimpl->m_mutex);

    struct kevent changes[2];

    // try to remove both events
    EV_SET(&changes[0], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    EV_SET(&changes[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);

    // dont throw an exception because the fd might have had only one of two filters active
    kevent(m_pimpl->m_kqueue_fd, changes, 2, NULL, 0, NULL);
}

void EventPoll::wait(int timeout_ms) {
    struct timespec  timeout_spec;
    struct timespec* timeout_ptr = nullptr;

    if (timeout_ms >= 0) {
        timeout_spec.tv_sec  = timeout_ms / 1000;
        timeout_spec.tv_nsec = (timeout_ms % 1000) * 1000000;
        timeout_ptr          = &timeout_spec;
    }

    int n = kevent(m_pimpl->m_kqueue_fd, NULL, 0, m_pimpl->m_kernel_events.data(), m_max_events, timeout_ptr);
    if (n == -1) {
        if (errno == EINTR)
            return;
        throw std::runtime_error(strerror(errno));
    }

    std::unique_lock<std::mutex> lock(m_pimpl->m_mutex);
    m_pimpl->m_active_events.clear();
    for (int i = 0; i < n; i++) {
        socket_t  fd    = static_cast<socket_t>(m_pimpl->m_kernel_events[i].ident);
        PollEvent event = PollEvent::NONE;

        if (m_pimpl->m_kernel_events[i].flags & EV_ERROR) {
            event = PollEvent::ERR;
        } else {
            event = Impl::fromNative(m_pimpl->m_kernel_events[i].filter);
        }

        m_pimpl->m_active_events.push_back({fd, event});
    }
}

const std::vector<EventPoll::PollEventEntry>& EventPoll::events() const {
    return m_pimpl->m_active_events;
}

#endif