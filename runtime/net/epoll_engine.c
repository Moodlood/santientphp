#include "epoll_engine.h"
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>

static inline uint32_t sph_map_epoll_events(uint32_t events) {
    uint32_t ev = 0;
    if (events & SPH_EPOLLIN)  ev |= EPOLLIN;
    if (events & SPH_EPOLLPRI) ev |= EPOLLPRI;
    if (events & SPH_EPOLLOUT) ev |= EPOLLOUT;
    if (events & SPH_EPOLLERR) ev |= EPOLLERR;
    if (events & SPH_EPOLLHUP) ev |= EPOLLHUP;
    if (events & SPH_EPOLLET)  ev |= EPOLLET;
    return ev;
}

int32_t sph_epoll_create(void) {
    return epoll_create1(0);
}

int32_t sph_epoll_add(int32_t epfd, int32_t fd, uint32_t events) {
    if (epfd < 0 || fd < 0) return -1;
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = sph_map_epoll_events(events);
    ev.data.fd = fd;
    return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

int32_t sph_epoll_mod(int32_t epfd, int32_t fd, uint32_t events) {
    if (epfd < 0 || fd < 0) return -1;
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = sph_map_epoll_events(events);
    ev.data.fd = fd;
    return epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
}

int32_t sph_epoll_del(int32_t epfd, int32_t fd) {
    if (epfd < 0 || fd < 0) return -1;
    return epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
}

int32_t sph_epoll_wait(int32_t epfd, int32_t* ready_fds, int32_t max_events, int32_t timeout_ms) {
    if (epfd < 0 || !ready_fds || max_events <= 0) return -1;
    struct epoll_event events[64];
    int count = (max_events > 64) ? 64 : max_events;
    int n = epoll_wait(epfd, events, count, timeout_ms);
    if (n < 0) return -1;
    for (int i = 0; i < n; ++i) {
        ready_fds[i] = events[i].data.fd;
    }
    return n;
}

void sph_epoll_close(int32_t epfd) {
    if (epfd >= 0) {
        close(epfd);
    }
}

#else
// Windows fallback emulation via poll/select structure
#include <winsock2.h>
#include <windows.h>

#define MAX_EMULATED_EPOLL_FDS 256

typedef struct {
    int in_use;
    int count;
    int fds[MAX_EMULATED_EPOLL_FDS];
    uint32_t events[MAX_EMULATED_EPOLL_FDS];
} sph_win_epoll_ctx;

static sph_win_epoll_ctx g_win_epoll_table[16] = {0};

int32_t sph_epoll_create(void) {
    for (int i = 0; i < 16; ++i) {
        if (!g_win_epoll_table[i].in_use) {
            memset(&g_win_epoll_table[i], 0, sizeof(sph_win_epoll_ctx));
            g_win_epoll_table[i].in_use = 1;
            return i + 1000;
        }
    }
    return -1;
}

int32_t sph_epoll_add(int32_t epfd, int32_t fd, uint32_t events) {
    int idx = epfd - 1000;
    if (idx < 0 || idx >= 16 || !g_win_epoll_table[idx].in_use) return -1;
    sph_win_epoll_ctx* ctx = &g_win_epoll_table[idx];
    if (ctx->count >= MAX_EMULATED_EPOLL_FDS) return -1;
    for (int i = 0; i < ctx->count; ++i) {
        if (ctx->fds[i] == fd) return -1; // Already exists
    }
    ctx->fds[ctx->count] = fd;
    ctx->events[ctx->count] = events;
    ctx->count++;
    return 0;
}

int32_t sph_epoll_mod(int32_t epfd, int32_t fd, uint32_t events) {
    int idx = epfd - 1000;
    if (idx < 0 || idx >= 16 || !g_win_epoll_table[idx].in_use) return -1;
    sph_win_epoll_ctx* ctx = &g_win_epoll_table[idx];
    for (int i = 0; i < ctx->count; ++i) {
        if (ctx->fds[i] == fd) {
            ctx->events[i] = events;
            return 0;
        }
    }
    return -1;
}

int32_t sph_epoll_del(int32_t epfd, int32_t fd) {
    int idx = epfd - 1000;
    if (idx < 0 || idx >= 16 || !g_win_epoll_table[idx].in_use) return -1;
    sph_win_epoll_ctx* ctx = &g_win_epoll_table[idx];
    for (int i = 0; i < ctx->count; ++i) {
        if (ctx->fds[i] == fd) {
            ctx->fds[i] = ctx->fds[ctx->count - 1];
            ctx->events[i] = ctx->events[ctx->count - 1];
            ctx->count--;
            return 0;
        }
    }
    return -1;
}

int32_t sph_epoll_wait(int32_t epfd, int32_t* ready_fds, int32_t max_events, int32_t timeout_ms) {
    int idx = epfd - 1000;
    if (idx < 0 || idx >= 16 || !g_win_epoll_table[idx].in_use || !ready_fds || max_events <= 0) return -1;
    sph_win_epoll_ctx* ctx = &g_win_epoll_table[idx];
    if (ctx->count == 0) return 0;

    fd_set read_set, write_set;
    FD_ZERO(&read_set);
    FD_ZERO(&write_set);

    for (int i = 0; i < ctx->count; ++i) {
        if (ctx->events[i] & SPH_EPOLLIN) {
            FD_SET((SOCKET)ctx->fds[i], &read_set);
        }
        if (ctx->events[i] & SPH_EPOLLOUT) {
            FD_SET((SOCKET)ctx->fds[i], &write_set);
        }
    }

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int res = select(0, &read_set, &write_set, NULL, (timeout_ms >= 0) ? &tv : NULL);
    if (res <= 0) return res;

    int ready_count = 0;
    for (int i = 0; i < ctx->count && ready_count < max_events; ++i) {
        if (FD_ISSET((SOCKET)ctx->fds[i], &read_set) || FD_ISSET((SOCKET)ctx->fds[i], &write_set)) {
            ready_fds[ready_count++] = ctx->fds[i];
        }
    }
    return ready_count;
}

void sph_epoll_close(int32_t epfd) {
    int idx = epfd - 1000;
    if (idx >= 0 && idx < 16) {
        g_win_epoll_table[idx].in_use = 0;
        g_win_epoll_table[idx].count = 0;
    }
}
#endif

