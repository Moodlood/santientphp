#pragma once

#include <stdint.h>
#include <stddef.h>
#include "../core/core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sph_ws_client sph_ws_client;

// Client Connection
sph_ws_client* sph_ws_connect(const char* host, int32_t port, const char* path);
int32_t sph_ws_is_open(const sph_ws_client* ws);
void sph_ws_close(sph_ws_client* ws);

// Frame Transmission
int32_t sph_ws_send_text(sph_ws_client* ws, const char* text);
int32_t sph_ws_send_binary(sph_ws_client* ws, const uint8_t* data, int64_t len);
int32_t sph_ws_send_ping(sph_ws_client* ws);
int32_t sph_ws_send_pong(sph_ws_client* ws, const uint8_t* payload, int64_t len);

// Frame Reception (returns opcode: 1=text, 2=binary, 8=close, 0=no_data/timeout, -1=error)
int32_t sph_ws_recv(sph_ws_client* ws, int32_t timeout_ms);
const char* sph_ws_get_message(const sph_ws_client* ws);
sph_byte_buffer* sph_ws_get_binary(const sph_ws_client* ws);

// Server-side WebSocket Handshake
sph_ws_client* sph_ws_accept_upgrade(int32_t client_fd, const char* http_request);

#ifdef __cplusplus
}
#endif

