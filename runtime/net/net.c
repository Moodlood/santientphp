#if !defined(SPH_CORE_INCLUDED)
#include "../core/core.h"
#include "../pal.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#if defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

static int g_wsa_initialized = 0;
static void sph_ensure_wsa(void) {
    if (!g_wsa_initialized) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        g_wsa_initialized = 1;
    }
}

int32_t sph_net_listen(int32_t port) {
    if (port <= 0 || port > 65535) return -1;
    sph_ensure_wsa();
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return -1;

    BOOL opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(s);
        return -1;
    }

    if (listen(s, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(s);
        return -1;
    }

    return (int32_t)s;
}

void sph_net_close(int32_t fd) {
    if (fd >= 0) {
        closesocket((SOCKET)fd);
    }
}

int32_t sph_server_poll_read(int32_t fd, int32_t timeout_ms) {
    if (fd < 0) return 0;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET((SOCKET)fd, &fds);
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    int res = select(fd + 1, &fds, NULL, NULL, (timeout_ms >= 0) ? &tv : NULL);
    return res > 0 ? 1 : 0;
}

int32_t sph_server_accept(int32_t server_fd, int32_t timeout_ms) {
    if (server_fd < 0) return -1;
    if (timeout_ms >= 0) {
        if (!sph_server_poll_read(server_fd, timeout_ms)) return -1;
    }
    struct sockaddr_in client_addr;
    int addr_len = sizeof(client_addr);
    SOCKET client_sock = accept((SOCKET)server_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (client_sock == INVALID_SOCKET) return -1;
    return (int32_t)client_sock;
}

char* sph_server_read_request(int32_t client_fd, int32_t timeout_ms) {
    if (client_fd < 0) return strdup("");
    if (timeout_ms >= 0) {
        if (!sph_server_poll_read(client_fd, timeout_ms)) return strdup("");
    }
    char buf[4096];
    int total = 0;
    while (total < (int)sizeof(buf) - 1) {
        int r = recv((SOCKET)client_fd, buf + total, sizeof(buf) - 1 - total, 0);
        if (r <= 0) break;
        total += r;
        buf[total] = '\0';
        if (strstr(buf, "\r\n\r\n")) break;
    }
    return strdup(buf);
}

int32_t sph_server_write_response(int32_t client_fd, int32_t status_code, const char* content_type, const char* body) {
    if (client_fd < 0) return -1;
    if (!content_type) content_type = "text/plain";
    if (!body) body = "";
    int64_t body_len = (int64_t)strlen(body);
    char header[512];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lld\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n\r\n",
        status_code, content_type, (long long)body_len);
    send((SOCKET)client_fd, header, header_len, 0);
    if (body_len > 0) {
        send((SOCKET)client_fd, body, (int)body_len, 0);
    }
    return 0;
}

int32_t sph_server_write_raw(int32_t client_fd, const uint8_t* data, int64_t len) {
    if (client_fd < 0 || !data || len <= 0) return -1;
    int64_t total = 0;
    while (total < len) {
        int s = send((SOCKET)client_fd, (const char*)(data + total), (int)(len - total), 0);
        if (s <= 0) return -1;
        total += s;
    }
    return 0;
}

static char* sph_http_request(const char* url, const char* post_body) {
    if (!url) return strdup("");
    if (strncmp(url, "mock://", 7) == 0 || strncmp(url, "http://mock/", 12) == 0) {
        if (post_body) {
            char buf[512];
            snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"action\":\"post\",\"data\":\"%s\"}", post_body);
            return strdup(buf);
        }
        return strdup("{\"status\":\"ok\",\"service\":\"SantientPHP Net\"}");
    }

    HMODULE hWinHttp = LoadLibraryA("winhttp.dll");
    if (hWinHttp) {
        typedef void* (*pfn_WinHttpOpen)(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD);
        typedef void* (*pfn_WinHttpConnect)(void*, LPCWSTR, unsigned short, DWORD);
        typedef void* (*pfn_WinHttpOpenRequest)(void*, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR*, DWORD);
        typedef BOOL (*pfn_WinHttpSendRequest)(void*, LPCWSTR, DWORD, LPVOID, DWORD, DWORD, DWORD_PTR);
        typedef BOOL (*pfn_WinHttpReceiveResponse)(void*, LPVOID);
        typedef BOOL (*pfn_WinHttpReadData)(void*, LPVOID, DWORD, LPDWORD);
        typedef BOOL (*pfn_WinHttpCloseHandle)(void*);

        pfn_WinHttpOpen fn_Open = (pfn_WinHttpOpen)(void*)GetProcAddress(hWinHttp, "WinHttpOpen");
        pfn_WinHttpConnect fn_Connect = (pfn_WinHttpConnect)(void*)GetProcAddress(hWinHttp, "WinHttpConnect");
        pfn_WinHttpOpenRequest fn_OpenRequest = (pfn_WinHttpOpenRequest)(void*)GetProcAddress(hWinHttp, "WinHttpOpenRequest");
        pfn_WinHttpSendRequest fn_SendRequest = (pfn_WinHttpSendRequest)(void*)GetProcAddress(hWinHttp, "WinHttpSendRequest");
        pfn_WinHttpReceiveResponse fn_ReceiveResponse = (pfn_WinHttpReceiveResponse)(void*)GetProcAddress(hWinHttp, "WinHttpReceiveResponse");
        pfn_WinHttpReadData fn_ReadData = (pfn_WinHttpReadData)(void*)GetProcAddress(hWinHttp, "WinHttpReadData");
        pfn_WinHttpCloseHandle fn_CloseHandle = (pfn_WinHttpCloseHandle)(void*)GetProcAddress(hWinHttp, "WinHttpCloseHandle");

        if (fn_Open && fn_Connect && fn_OpenRequest && fn_SendRequest && fn_ReceiveResponse && fn_ReadData && fn_CloseHandle) {
            int is_https = (strncmp(url, "https://", 8) == 0);
            const char* host_start = url + (is_https ? 8 : (strncmp(url, "http://", 7) == 0 ? 7 : 0));
            const char* slash = strchr(host_start, '/');
            char host[256];
            char path[1024];
            if (slash) {
                size_t host_len = (size_t)(slash - host_start);
                if (host_len >= sizeof(host)) host_len = sizeof(host) - 1;
                strncpy(host, host_start, host_len);
                host[host_len] = '\0';
                strncpy(path, slash, sizeof(path) - 1);
                path[sizeof(path) - 1] = '\0';
            } else {
                strncpy(host, host_start, sizeof(host) - 1);
                host[sizeof(host) - 1] = '\0';
                strcpy(path, "/");
            }

            wchar_t wHost[256];
            wchar_t wPath[1024];
            MultiByteToWideChar(CP_UTF8, 0, host, -1, wHost, 256);
            MultiByteToWideChar(CP_UTF8, 0, path, -1, wPath, 1024);

            void* hSession = fn_Open(L"SantientPHP/1.0", 1, NULL, NULL, 0);
            if (hSession) {
                void* hConnect = fn_Connect(hSession, wHost, is_https ? 443 : 80, 0);
                if (hConnect) {
                    LPCWSTR verb = post_body ? L"POST" : L"GET";
                    DWORD flags = is_https ? 0x00800000 : 0;
                    void* hRequest = fn_OpenRequest(hConnect, verb, wPath, NULL, NULL, NULL, flags);
                    if (hRequest) {
                        LPCWSTR hdrs = post_body ? L"Content-Type: application/x-www-form-urlencoded\r\n" : NULL;
                        DWORD hdr_len = hdrs ? (DWORD)wcslen(hdrs) : 0;
                        DWORD body_len = post_body ? (DWORD)strlen(post_body) : 0;
                        if (fn_SendRequest(hRequest, hdrs, hdr_len, (LPVOID)post_body, body_len, body_len, 0)) {
                            if (fn_ReceiveResponse(hRequest, NULL)) {
                                size_t total_read = 0;
                                size_t cap = 4096;
                                char* resp_buf = (char*)malloc(cap);
                                DWORD bytes_read = 0;
                                while (fn_ReadData(hRequest, resp_buf + total_read, (DWORD)(cap - total_read - 1), &bytes_read) && bytes_read > 0) {
                                    total_read += bytes_read;
                                    if (cap - total_read < 1024) {
                                        cap *= 2;
                                        char* new_b = (char*)realloc(resp_buf, cap);
                                        if (!new_b) break;
                                        resp_buf = new_b;
                                    }
                                }
                                resp_buf[total_read] = '\0';
                                fn_CloseHandle(hRequest);
                                fn_CloseHandle(hConnect);
                                fn_CloseHandle(hSession);
                                FreeLibrary(hWinHttp);
                                return resp_buf;
                            }
                        }
                        fn_CloseHandle(hRequest);
                    }
                    fn_CloseHandle(hConnect);
                }
                fn_CloseHandle(hSession);
            }
            FreeLibrary(hWinHttp);
        }
    }

    return strdup("");
}

char* sph_http_get(const char* url) {
    return sph_http_request(url, NULL);
}

char* sph_http_post(const char* url, const char* body) {
    return sph_http_request(url, body ? body : "");
}
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <poll.h>

int32_t sph_net_listen(int32_t port) {
    if (port <= 0 || port > 65535) return -1;
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;
    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);
    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(s);
        return -1;
    }
    if (listen(s, SOMAXCONN) < 0) {
        close(s);
        return -1;
    }
    return (int32_t)s;
}

void sph_net_close(int32_t fd) {
    if (fd >= 0) {
        close(fd);
    }
}

int32_t sph_server_poll_read(int32_t fd, int32_t timeout_ms) {
    if (fd < 0) return 0;
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    int res = poll(&pfd, 1, timeout_ms);
    return res > 0 && (pfd.revents & POLLIN) ? 1 : 0;
}

int32_t sph_server_accept(int32_t server_fd, int32_t timeout_ms) {
    if (server_fd < 0) return -1;
    if (timeout_ms >= 0) {
        if (!sph_server_poll_read(server_fd, timeout_ms)) return -1;
    }
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_sock = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (client_sock < 0) return -1;
    return client_sock;
}

char* sph_server_read_request(int32_t client_fd, int32_t timeout_ms) {
    if (client_fd < 0) return strdup("");
    if (timeout_ms >= 0) {
        if (!sph_server_poll_read(client_fd, timeout_ms)) return strdup("");
    }
    char buf[4096];
    int total = 0;
    while (total < (int)sizeof(buf) - 1) {
        int r = recv(client_fd, buf + total, sizeof(buf) - 1 - total, 0);
        if (r <= 0) break;
        total += r;
        buf[total] = '\0';
        if (strstr(buf, "\r\n\r\n")) break;
    }
    return strdup(buf);
}

int32_t sph_server_write_response(int32_t client_fd, int32_t status_code, const char* content_type, const char* body) {
    if (client_fd < 0) return -1;
    if (!content_type) content_type = "text/plain";
    if (!body) body = "";
    int64_t body_len = (int64_t)strlen(body);
    char header[512];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lld\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n\r\n",
        status_code, content_type, (long long)body_len);
    send(client_fd, header, header_len, 0);
    if (body_len > 0) {
        send(client_fd, body, (size_t)body_len, 0);
    }
    return 0;
}

int32_t sph_server_write_raw(int32_t client_fd, const uint8_t* data, int64_t len) {
    if (client_fd < 0 || !data || len <= 0) return -1;
    int64_t total = 0;
    while (total < len) {
        ssize_t s = send(client_fd, (const char*)(data + total), (size_t)(len - total), 0);
        if (s <= 0) return -1;
        total += s;
    }
    return 0;
}

char* sph_http_get(const char* url) {
    if (!url) return strdup("");
    if (strncmp(url, "mock://", 7) == 0 || strncmp(url, "http://mock/", 12) == 0) {
        return strdup("{\"status\":\"ok\",\"service\":\"SantientPHP Net\"}");
    }
    return strdup("");
}

char* sph_http_post(const char* url, const char* body) {
    if (!url) return strdup("");
    if (strncmp(url, "mock://", 7) == 0 || strncmp(url, "http://mock/", 12) == 0) {
        char buf[512];
        snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"action\":\"post\",\"data\":\"%s\"}", body ? body : "");
        return strdup(buf);
    }
    return strdup("");
}
#endif

