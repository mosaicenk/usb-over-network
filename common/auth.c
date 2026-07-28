/*
 * USB Over Network - Authentication Implementation
 */

#include "auth.h"
#include "network.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>

const char* auth_get_token(const char *cli_arg) {
    if (cli_arg != NULL && cli_arg[0] != '\0') {
        return cli_arg;
    }
    const char *env = getenv("USBIP_AUTH_TOKEN");
    if (env != NULL && env[0] != '\0') {
        return env;
    }
    return DEFAULT_AUTH_TOKEN;
}

bool auth_is_enabled(const char *token) {
    return token != NULL && token[0] != '\0';
}

/* Write the framed handshake into buf (header + token). Returns total length,
 * or 0 if token is empty (no handshake needed). */
static size_t build_handshake(uint8_t *buf, size_t buf_len, const char *token) {
    if (!auth_is_enabled(token)) {
        return 0;
    }
    size_t tlen = strlen(token);
    if (tlen > AUTH_TOKEN_MAX_LEN || AUTH_WIRE_HEADER_LEN + tlen > buf_len) {
        return 0;
    }
    /* Big-endian magic so any endianness mismatch shows as a bad magic. */
    buf[0] = (uint8_t)(AUTH_WIRE_MAGIC >> 24);
    buf[1] = (uint8_t)(AUTH_WIRE_MAGIC >> 16);
    buf[2] = (uint8_t)(AUTH_WIRE_MAGIC >> 8);
    buf[3] = (uint8_t)(AUTH_WIRE_MAGIC);
    buf[4] = (uint8_t)tlen;
    memcpy(buf + AUTH_WIRE_HEADER_LEN, token, tlen);
    return AUTH_WIRE_HEADER_LEN + tlen;
}

error_code_t auth_server_handshake(socket_t fd, const char *expected_token) {
    if (!auth_is_enabled(expected_token)) {
        return ERR_SUCCESS;  /* auth disabled: behave like plain USB/IP */
    }

    uint8_t header[AUTH_WIRE_HEADER_LEN];
    ssize_t got = net_recv_all_timeout(fd, header, sizeof(header), AUTH_TIMEOUT_MS);
    if (got != (ssize_t)sizeof(header)) {
        LOG_WARN("auth: client sent no/short handshake");
        return ERR_PERMISSION_DENIED;
    }

    uint32_t magic = ((uint32_t)header[0] << 24) | ((uint32_t)header[1] << 16) |
                     ((uint32_t)header[2] << 8) | (uint32_t)header[3];
    if (magic != AUTH_WIRE_MAGIC) {
        LOG_WARN("auth: bad magic 0x%08X (plain USB/IP client on a secured server?)", magic);
        return ERR_PERMISSION_DENIED;
    }

    uint8_t tlen = header[4];
    if (tlen == 0 || tlen > AUTH_TOKEN_MAX_LEN) {
        LOG_WARN("auth: implausible token length %u", tlen);
        return ERR_PERMISSION_DENIED;
    }

    char got_token[AUTH_TOKEN_MAX_LEN + 1];
    got = net_recv_all_timeout(fd, got_token, tlen, AUTH_TIMEOUT_MS);
    if (got != (ssize_t)tlen) {
        return ERR_PERMISSION_DENIED;
    }
    got_token[tlen] = '\0';

    /* Constant-time compare to avoid trivial timing oracles. */
    const char *exp = expected_token;
    size_t exp_len = strlen(exp);
    volatile uint8_t diff = (uint8_t)(exp_len ^ tlen);
    for (size_t i = 0; i < exp_len && i < tlen; i++) {
        diff |= (uint8_t)(exp[i] ^ got_token[i]);
    }
    if (diff != 0) {
        LOG_WARN("auth: token mismatch from peer");
        return ERR_PERMISSION_DENIED;
    }
    return ERR_SUCCESS;
}

error_code_t auth_client_handshake(socket_t fd, const char *token) {
    if (!auth_is_enabled(token)) {
        return ERR_SUCCESS;  /* server may also have auth disabled */
    }

    uint8_t buf[AUTH_WIRE_HEADER_LEN + AUTH_TOKEN_MAX_LEN];
    size_t len = build_handshake(buf, sizeof(buf), token);
    if (len == 0) {
        return ERR_INVALID_PARAM;
    }

    ssize_t sent = net_send_all(fd, buf, len);
    if (sent != (ssize_t)len) {
        LOG_ERROR("auth: failed to send handshake");
        return ERR_SOCKET_SEND;
    }
    return ERR_SUCCESS;
}
