#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Events bitmask
#define SPH_EPOLLIN  0x001
#define SPH_EPOLLPRI 0x002
#define SPH_EPOLLOUT 0x004
#define SPH_EPOLLERR 0x008
#define SPH_EPOLLHUP 0x010
#define SPH_EPOLLET  0x020

int32_t sph_epoll_create(void);
int32_t sph_epoll_add(int32_t epfd, int32_t fd, uint32_t events);
int32_t sph_epoll_mod(int32_t epfd, int32_t fd, uint32_t events);
int32_t sph_epoll_del(int32_t epfd, int32_t fd);
int32_t sph_epoll_wait(int32_t epfd, int32_t* ready_fds, int32_t max_events, int32_t timeout_ms);
void sph_epoll_close(int32_t epfd);

#ifdef __cplusplus
}
#endif

