/*
 * USB Over Network - Authentication (preshared token)
 *
 * Lightweight access control for trusted LANs. NOT TLS, NOT strong crypto:
 * the token crosses the wire in cleartext. Its only purpose is to stop casual
 * access from uninvited hosts on the same subnet.
 *
 * Wire framing (sent immediately after TCP connect, before USB/IP ops):
 *   [4 bytes big-endian magic 0x55424F4E "UBON"]
 *   [1 byte token length N]
 *   [N bytes token, no NUL]
 * An empty server token disables auth entirely (legacy/USB/IP interop).
 */

#ifndef AUTH_H
#define AUTH_H

#include "types.h"
#include "error.h"
#include "config.h"

/* Resolve the effective token: explicit arg > USBIP_AUTH_TOKEN env > default. */
const char* auth_get_token(const char *cli_arg);

/* True when a non-empty token should be enforced. */
bool auth_is_enabled(const char *token);

/* Server side: read and validate the client's handshake. ERR_SUCCESS on match,
 * ERR_PERMISSION_DENIED on mismatch, ERR_TIMEOUT if peer is silent. */
error_code_t auth_server_handshake(socket_t fd, const char *expected_token);

/* Client side: send the handshake. ERR_SUCCESS if sent and acknowledged. */
error_code_t auth_client_handshake(socket_t fd, const char *token);

#endif /* AUTH_H */
