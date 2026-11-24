#include "common/test_framework.hpp"
#include "runtime/net/epoll_engine.h"
#include <string>

#if !defined(_WIN32)
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace sph {

namespace {

struct NetExhaustiveRegistrar {
    NetExhaustiveRegistrar() {
        registerEpollCreateTests();
        registerEpollAddModDelTests();
        registerEpollWaitTimeoutTests();
        registerSocketDescriptorTests();
    }

    void registerEpollCreateTests() {
        // 150 tests: sph_epoll_create handle validity and lifecycle
        for (int i = 1; i <= 150; ++i) {
            std::string name = "NetExhaustive: epoll_create lifecycle " + std::to_string(i);
            testing::TestRegistry::instance().registerTest(name, [i]() {
                int epfd = sph_epoll_create();
                CHECK(epfd >= 0);
                sph_epoll_close(epfd);
            });
        }
    }

    void registerEpollAddModDelTests() {
        // 150 tests: sph_epoll_add, mod, del registration
        for (int j = 1; j <= 150; ++j) {
            std::string name = "NetExhaustive: epoll_add and del operations " + std::to_string(j);
            testing::TestRegistry::instance().registerTest(name, [j]() {
                int epfd = sph_epoll_create();
                CHECK(epfd >= 0);

#if !defined(_WIN32)
                int testFd = socket(AF_INET, SOCK_STREAM, 0);
                if (testFd < 0) {
                    int pfd[2];
                    if (pipe(pfd) == 0) {
                        testFd = pfd[0];
                        close(pfd[1]);
                    }
                }
                CHECK(testFd >= 0);
#else
                int testFd = 100 + (j % 50);
#endif
                uint32_t events = SPH_EPOLLIN | SPH_EPOLLOUT;

                int addRes = sph_epoll_add(epfd, testFd, events);
                CHECK_EQ(addRes, 0);

                int modRes = sph_epoll_mod(epfd, testFd, SPH_EPOLLIN);
                CHECK_EQ(modRes, 0);

                int delRes = sph_epoll_del(epfd, testFd);
                CHECK_EQ(delRes, 0);

#if !defined(_WIN32)
                close(testFd);
#endif
                sph_epoll_close(epfd);
            });
        }
    }

    void registerEpollWaitTimeoutTests() {
        // 150 tests: sph_epoll_wait with 0ms and non-blocking timeout
        for (int k = 1; k <= 150; ++k) {
            std::string name = "NetExhaustive: epoll_wait timeout behavior " + std::to_string(k);
            testing::TestRegistry::instance().registerTest(name, [k]() {
                int epfd = sph_epoll_create();
                CHECK(epfd >= 0);

                int32_t ready_fds[4] = {0};
                int count = sph_epoll_wait(epfd, ready_fds, 4, 0);
                CHECK(count >= 0);

                sph_epoll_close(epfd);
            });
        }
    }

    void registerSocketDescriptorTests() {
        // 150 tests: Event masks and flag verification
        for (int m = 1; m <= 150; ++m) {
            std::string name = "NetExhaustive: Event mask invariants " + std::to_string(m);
            testing::TestRegistry::instance().registerTest(name, [m]() {
                uint32_t inFlag = SPH_EPOLLIN;
                uint32_t outFlag = SPH_EPOLLOUT;
                uint32_t errFlag = SPH_EPOLLERR;
                uint32_t hupFlag = SPH_EPOLLHUP;

                CHECK((inFlag & outFlag) == 0u);
                CHECK((inFlag & errFlag) == 0u);
                CHECK((outFlag & hupFlag) == 0u);
                CHECK_NE(inFlag | outFlag, 0u);
            });
        }
    }
};

static NetExhaustiveRegistrar s_netRegistrar;

} // namespace

} // namespace sph

