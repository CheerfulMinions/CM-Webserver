#ifndef EPOLLER_H
#define EPOLLER_H

#include <sys/epoll.h>
#include <vector>
#include <unistd.h> // close()
#include <assert.h>
#include <errno.h>

class Epoller {
public:
    explicit Epoller(int maxEvent = 1024) : m_epollFd{epoll_create1(EPOLL_CLOEXEC)}, m_events(maxEvent) {
        assert(m_epollFd >= 0 && !m_events.empty());
    }

    ~Epoller() {
        close(m_epollFd);
    }
    
    bool AddFd(int fd, uint32_t events) {
        if (fd < 0) return false;
        epoll_event ev = {0};
        ev.data.fd = fd;
        ev.events = events;
        return epoll_ctl(m_epollFd, EPOLL_CTL_ADD, fd, &ev) == 0;
    }

    bool ModFd(int fd, uint32_t events) {
        if (fd < 0) return false;
        epoll_event ev = {0};
        ev.data.fd = fd;
        ev.events = events;
        return epoll_ctl(m_epollFd, EPOLL_CTL_MOD, fd, &ev) == 0;
    }

    bool DelFd(int fd) {
        if (fd < 0) return false;
        epoll_event ev{0};
        return epoll_ctl(m_epollFd, EPOLL_CTL_DEL, fd, &ev) == 0;
    }

    int Wait(int timeoutMs = -1) {
        return epoll_wait(m_epollFd, &m_events[0], static_cast<int>(m_events.size()), timeoutMs);
    }

    int GetEventFd(size_t i) const {
        assert(i < m_events.size());
        return m_events[i].data.fd;
    }

    uint32_t GetEvents(size_t i) const {
        assert(i < m_events.size());
        return m_events[i].events;
    }

private:
    int m_epollFd;
    std::vector<epoll_event> m_events; // 使用vector保存就绪事件
};

#endif //EPOLLER_H
