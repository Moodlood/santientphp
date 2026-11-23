#include "websocket.h"
#include "../pal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket(s) close(s)
#endif

// ============================================================================
// Internal Client Structure
// ============================================================================

struct sph_ws_client {
    SOCKET fd;
    int32_t is_server;
    int32_t is_open;
    char* last_text;
    sph_byte_buffer* last_binary;
};

// ============================================================================
// Cryptographic Primitives: SHA-1 & Base64 (RFC 6455 Handshake)
// ============================================================================

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t buffer[64];
} sph_sha1_ctx;

#define SPH_SHA1_ROL(val, bits) (((val) << (bits)) | ((val) >> (32 - (bits))))

static void sph_sha1_transform(uint32_t state[5], const uint8_t buffer[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
    uint32_t block[80];

    for (int i = 0; i < 16; ++i) {
        block[i] = ((uint32_t)buffer[i * 4] << 24) |
                   ((uint32_t)buffer[i * 4 + 1] << 16) |
                   ((uint32_t)buffer[i * 4 + 2] << 8) |
                   ((uint32_t)buffer[i * 4 + 3]);
    }
    for (int i = 16; i < 80; ++i) {
        block[i] = SPH_SHA1_ROL(block[i - 3] ^ block[i - 8] ^ block[i - 14] ^ block[i - 16], 1);
    }

    for (int i = 0; i < 20; ++i) {
        uint32_t t = SPH_SHA1_ROL(a, 5) + ((b & c) | (~b & d)) + e + block[i] + 0x5A827999;
        e = d; d = c; c = SPH_SHA1_ROL(b, 30); b = a; a = t;
    }
    for (int i = 20; i < 40; ++i) {
        uint32_t t = SPH_SHA1_ROL(a, 5) + (b ^ c ^ d) + e + block[i] + 0x6ED9EBA1;
        e = d; d = c; c = SPH_SHA1_ROL(b, 30); b = a; a = t;
    }
    for (int i = 40; i < 60; ++i) {
        uint32_t t = SPH_SHA1_ROL(a, 5) + ((b & c) | (b & d) | (c & d)) + e + block[i] + 0x8F1BBCDC;
        e = d; d = c; c = SPH_SHA1_ROL(b, 30); b = a; a = t;
    }
    for (int i = 60; i < 80; ++i) {
        uint32_t t = SPH_SHA1_ROL(a, 5) + (b ^ c ^ d) + e + block[i] + 0xCA62C1D6;
        e = d; d = c; c = SPH_SHA1_ROL(b, 30); b = a; a = t;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

static void sph_sha1_init(sph_sha1_ctx* ctx) {
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
    ctx->count[0] = ctx->count[1] = 0;
}

static void sph_sha1_update(sph_sha1_ctx* ctx, const uint8_t* data, size_t len) {
    uint32_t i, j = (ctx->count[0] >> 3) & 63;
    if ((ctx->count[0] += (uint32_t)(len << 3)) < (len << 3)) ctx->count[1]++;
    ctx->count[1] += (uint32_t)(len >> 29);
    if ((j + len) > 63) {
        memcpy(&ctx->buffer[j], data, (i = 64 - j));
        sph_sha1_transform(ctx->state, ctx->buffer);
        for (; i + 63 < len; i += 64) {
            sph_sha1_transform(ctx->state, &data[i]);
        }
        j = 0;
    } else {
        i = 0;
    }
    memcpy(&ctx->buffer[j], &data[i], len - i);
}

static void sph_sha1_final(uint8_t digest[20], sph_sha1_ctx* ctx) {
    uint8_t finalcount[8];
    for (int i = 0; i < 8; ++i) {
        finalcount[i] = (uint8_t)((ctx->count[(i >= 4 ? 0 : 1)] >> ((3 - (i & 3)) * 8)) & 255);
    }
    uint8_t c = 0200;
    sph_sha1_update(ctx, &c, 1);
    while ((ctx->count[0] & 504) != 448) {
        c = 0000;
        sph_sha1_update(ctx, &c, 1);
    }
    sph_sha1_update(ctx, finalcount, 8);
    for (int i = 0; i < 20; ++i) {
        digest[i] = (uint8_t)((ctx->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255);
    }
}

static const char kBase64Table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void sph_base64_encode(const uint8_t* src, size_t len, char* dst) {
    size_t i = 0, j = 0;
    for (; i + 2 < len; i += 3) {
        dst[j++] = kBase64Table[(src[i] >> 2) & 0x3F];
        dst[j++] = kBase64Table[((src[i] & 0x3) << 4) | ((src[i + 1] >> 4) & 0x0F)];
        dst[j++] = kBase64Table[((src[i + 1] & 0xF) << 2) | ((src[i + 2] >> 6) & 0x03)];
        dst[j++] = kBase64Table[src[i + 2] & 0x3F];
    }
    if (i < len) {
        dst[j++] = kBase64Table[(src[i] >> 2) & 0x3F];
        if (i + 1 < len) {
            dst[j++] = kBase64Table[((src[i] & 0x3) << 4) | ((src[i + 1] >> 4) & 0x0F)];
            dst[j++] = kBase64Table[(src[i + 1] & 0xF) << 2];
        } else {
            dst[j++] = kBase64Table[(src[i] & 0x3) << 4];
            dst[j++] = '=';
        }
        dst[j++] = '=';
    }
    dst[j] = '\0';
}

static void sph_compute_ws_accept(const char* key, char* out_accept) {
    static const char kGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char combined[256];
    snprintf(combined, sizeof(combined), "%s%s", key, kGuid);

    sph_sha1_ctx ctx;
    sph_sha1_init(&ctx);
    sph_sha1_update(&ctx, (const uint8_t*)combined, strlen(combined));
    uint8_t hash[20];
    sph_sha1_final(hash, &ctx);

    sph_base64_encode(hash, 20, out_accept);
}

// ============================================================================
// Network I/O Helpers
// ============================================================================

#if defined(_WIN32)
static void sph_ws_ensure_wsa(void) {
    static int initialized = 0;
    if (!initialized) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        initialized = 1;
    }
}
#else
static void sph_ws_ensure_wsa(void) {}
#endif

static int sph_ws_send_all(SOCKET s, const uint8_t* buf, int64_t len) {
    int64_t total = 0;
    while (total < len) {
        int sent = send(s, (const char*)(buf + total), (int)(len - total), 0);
        if (sent <= 0) return -1;
        total += sent;
    }
    return 0;
}

static int sph_ws_recv_exact(SOCKET s, uint8_t* buf, int64_t len) {
    int64_t total = 0;
    while (total < len) {
        int r = recv(s, (char*)(buf + total), (int)(len - total), 0);
        if (r <= 0) return -1;
        total += r;
    }
    return 0;
}

// ============================================================================
// Frame Transmission & Reception
// ============================================================================

static int32_t sph_ws_send_frame(sph_ws_client* ws, int opcode, int is_masked, const uint8_t* data, int64_t len) {
    if (!ws || !ws->is_open || ws->fd == INVALID_SOCKET) return -1;

    uint8_t header[14];
    int header_len = 0;

    header[0] = (uint8_t)(0x80 | (opcode & 0x0F)); // FIN=1

    uint8_t mask_bit = is_masked ? 0x80 : 0x00;
    if (len <= 125) {
        header[1] = (uint8_t)(mask_bit | len);
        header_len = 2;
    } else if (len <= 65535) {
        header[1] = (uint8_t)(mask_bit | 126);
        header[2] = (uint8_t)((len >> 8) & 0xFF);
        header[3] = (uint8_t)(len & 0xFF);
        header_len = 4;
    } else {
        header[1] = (uint8_t)(mask_bit | 127);
        for (int i = 0; i < 8; ++i) {
            header[2 + i] = (uint8_t)((len >> ((7 - i) * 8)) & 0xFF);
        }
        header_len = 10;
    }

    uint8_t mask_key[4] = { 0 };
    if (is_masked) {
        uint32_t rnd = (uint32_t)rand() ^ (uint32_t)time(NULL);
        mask_key[0] = (uint8_t)(rnd & 0xFF);
        mask_key[1] = (uint8_t)((rnd >> 8) & 0xFF);
        mask_key[2] = (uint8_t)((rnd >> 16) & 0xFF);
        mask_key[3] = (uint8_t)((rnd >> 24) & 0xFF);
        memcpy(header + header_len, mask_key, 4);
        header_len += 4;
    }

    if (sph_ws_send_all(ws->fd, header, header_len) != 0) {
        ws->is_open = 0;
        return -1;
    }

    if (len > 0) {
        if (is_masked) {
            uint8_t* masked_payload = (uint8_t*)malloc((size_t)len);
            if (!masked_payload) return -1;
            for (int64_t i = 0; i < len; ++i) {
                masked_payload[i] = data[i] ^ mask_key[i % 4];
            }
            int res = sph_ws_send_all(ws->fd, masked_payload, len);
            free(masked_payload);
            if (res != 0) {
                ws->is_open = 0;
                return -1;
            }
        } else {
            if (sph_ws_send_all(ws->fd, data, len) != 0) {
                ws->is_open = 0;
                return -1;
            }
        }
    }
    return 0;
}

// ============================================================================
// Public WebSocket API
// ============================================================================

sph_ws_client* sph_ws_connect(const char* host, int32_t port, const char* path) {
    if (!host || port <= 0) return NULL;
    sph_ws_ensure_wsa();

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return NULL;

    struct hostent* he = gethostbyname(host);
    if (!he) {
        closesocket(s);
        return NULL;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], (size_t)he->h_length);

    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(s);
        return NULL;
    }

    // Generate random 16-byte key
    uint8_t random_bytes[16];
    for (int i = 0; i < 16; ++i) random_bytes[i] = (uint8_t)(rand() & 0xFF);
    char ws_key[32];
    sph_base64_encode(random_bytes, 16, ws_key);

    const char* req_path = (path && *path) ? path : "/";
    char req[512];
    int req_len = snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n",
        req_path, host, port, ws_key);

    if (sph_ws_send_all(s, (const uint8_t*)req, req_len) != 0) {
        closesocket(s);
        return NULL;
    }

    // Read Handshake Response
    char resp[1024];
    int total_read = 0;
    while (total_read < (int)sizeof(resp) - 1) {
        int r = recv(s, resp + total_read, (int)(sizeof(resp) - 1 - total_read), 0);
        if (r <= 0) break;
        total_read += r;
        resp[total_read] = '\0';
        if (strstr(resp, "\r\n\r\n")) break;
    }

    if (!strstr(resp, "101 Switching Protocols")) {
        closesocket(s);
        return NULL;
    }

    sph_ws_client* client = (sph_ws_client*)calloc(1, sizeof(sph_ws_client));
    if (!client) {
        closesocket(s);
        return NULL;
    }
    client->fd = s;
    client->is_server = 0;
    client->is_open = 1;
    return client;
}

int32_t sph_ws_is_open(const sph_ws_client* ws) {
    return (ws && ws->is_open && ws->fd != INVALID_SOCKET) ? 1 : 0;
}

void sph_ws_close(sph_ws_client* ws) {
    if (!ws) return;
    if (ws->is_open && ws->fd != INVALID_SOCKET) {
        sph_ws_send_frame(ws, 0x8, !ws->is_server, NULL, 0); // Close frame
        closesocket(ws->fd);
        ws->fd = INVALID_SOCKET;
        ws->is_open = 0;
    }
    if (ws->last_text) {
        free(ws->last_text);
        ws->last_text = NULL;
    }
    if (ws->last_binary) {
        sph_buffer_free(ws->last_binary);
        ws->last_binary = NULL;
    }
    free(ws);
}

int32_t sph_ws_send_text(sph_ws_client* ws, const char* text) {
    if (!text) return -1;
    return sph_ws_send_frame(ws, 0x1, !ws->is_server, (const uint8_t*)text, (int64_t)strlen(text));
}

int32_t sph_ws_send_binary(sph_ws_client* ws, const uint8_t* data, int64_t len) {
    return sph_ws_send_frame(ws, 0x2, !ws->is_server, data, len);
}

int32_t sph_ws_send_ping(sph_ws_client* ws) {
    return sph_ws_send_frame(ws, 0x9, !ws->is_server, NULL, 0);
}

int32_t sph_ws_send_pong(sph_ws_client* ws, const uint8_t* payload, int64_t len) {
    return sph_ws_send_frame(ws, 0xA, !ws->is_server, payload, len);
}

int32_t sph_ws_recv(sph_ws_client* ws, int32_t timeout_ms) {
    if (!ws || !ws->is_open || ws->fd == INVALID_SOCKET) return -1;

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(ws->fd, &read_fds);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ready = select((int)(ws->fd + 1), &read_fds, NULL, NULL, (timeout_ms >= 0) ? &tv : NULL);
    if (ready <= 0) return 0; // 0 = no data / timeout

    uint8_t header[2];
    if (sph_ws_recv_exact(ws->fd, header, 2) != 0) {
        ws->is_open = 0;
        return -1;
    }

    int opcode = header[0] & 0x0F;
    int is_masked = (header[1] & 0x80) != 0;
    int64_t payload_len = header[1] & 0x7F;

    if (payload_len == 126) {
        uint8_t ext[2];
        if (sph_ws_recv_exact(ws->fd, ext, 2) != 0) { ws->is_open = 0; return -1; }
        payload_len = ((int64_t)ext[0] << 8) | ext[1];
    } else if (payload_len == 127) {
        uint8_t ext[8];
        if (sph_ws_recv_exact(ws->fd, ext, 8) != 0) { ws->is_open = 0; return -1; }
        payload_len = 0;
        for (int i = 0; i < 8; ++i) {
            payload_len = (payload_len << 8) | ext[i];
        }
    }

    uint8_t mask_key[4] = { 0 };
    if (is_masked) {
        if (sph_ws_recv_exact(ws->fd, mask_key, 4) != 0) { ws->is_open = 0; return -1; }
    }

    uint8_t* payload = NULL;
    if (payload_len > 0) {
        payload = (uint8_t*)malloc((size_t)payload_len + 1);
        if (!payload) return -1;
        if (sph_ws_recv_exact(ws->fd, payload, payload_len) != 0) {
            free(payload);
            ws->is_open = 0;
            return -1;
        }
        if (is_masked) {
            for (int64_t i = 0; i < payload_len; ++i) {
                payload[i] ^= mask_key[i % 4];
            }
        }
        payload[payload_len] = '\0';
    }

    // Handle Opcodes
    if (opcode == 0x8) { // Close
        if (payload) free(payload);
        ws->is_open = 0;
        return 8;
    } else if (opcode == 0x9) { // Ping -> Auto Pong
        sph_ws_send_pong(ws, payload, payload_len);
        if (payload) free(payload);
        return sph_ws_recv(ws, timeout_ms); // continue receiving next user frame
    } else if (opcode == 0xA) { // Pong
        if (payload) free(payload);
        return 0;
    } else if (opcode == 0x1) { // Text
        if (ws->last_text) free(ws->last_text);
        ws->last_text = (char*)payload;
        return 1;
    } else if (opcode == 0x2) { // Binary
        if (ws->last_binary) sph_buffer_free(ws->last_binary);
        ws->last_binary = sph_buffer_from_raw(payload, payload_len);
        if (payload) free(payload);
        return 2;
    }

    if (payload) free(payload);
    return opcode;
}

const char* sph_ws_get_message(const sph_ws_client* ws) {
    return (ws && ws->last_text) ? ws->last_text : "";
}

sph_byte_buffer* sph_ws_get_binary(const sph_ws_client* ws) {
    return ws ? ws->last_binary : NULL;
}

// Server Upgrade Handshake
sph_ws_client* sph_ws_accept_upgrade(int32_t client_fd, const char* http_request) {
    if (client_fd < 0 || !http_request) return NULL;

    const char* key_header = strstr(http_request, "Sec-WebSocket-Key:");
    if (!key_header) key_header = strstr(http_request, "sec-websocket-key:");
    if (!key_header) return NULL;

    key_header += 18;
    while (*key_header == ' ') key_header++;
    char key[64] = { 0 };
    int k = 0;
    while (*key_header && *key_header != '\r' && *key_header != '\n' && k < 63) {
        key[k++] = *key_header++;
    }
    key[k] = '\0';

    char accept_key[64];
    sph_compute_ws_accept(key, accept_key);

    char resp[512];
    int resp_len = snprintf(resp, sizeof(resp),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n\r\n",
        accept_key);

    if (sph_ws_send_all((SOCKET)client_fd, (const uint8_t*)resp, resp_len) != 0) {
        return NULL;
    }

    sph_ws_client* client = (sph_ws_client*)calloc(1, sizeof(sph_ws_client));
    if (!client) return NULL;
    client->fd = (SOCKET)client_fd;
    client->is_server = 1;
    client->is_open = 1;
    return client;
}

