/*
 * USB Over Network - Server Discovery Implementation
 * Windows-only implementation
 */

#include "discovery.h"
#include "../common/network.h"
#include "../common/log.h"
#include <string.h>
#include <stdio.h>

/* Discovery message format */
#define DISCOVERY_REQUEST   DISCOVERY_MAGIC
#define DISCOVERY_RESPONSE  SERVER_RESPONSE_MAGIC

/*
 * Discover servers on LAN using UDP broadcast
 */
error_code_t discovery_broadcast(discovery_result_t *result, uint32_t timeout_ms) {
    if (result == NULL) {
        return ERR_INVALID_PARAM;
    }

    discovery_clear_results(result);

    LOG_DEBUG("Starting server discovery (timeout: %u ms)", timeout_ms);

    /* Create UDP socket */
    socket_t sock = udp_socket_create();
    if (!socket_is_valid(sock)) {
        LOG_ERROR("Failed to create UDP socket for discovery");
        return ERR_SOCKET_CREATE;
    }

    /* Enable broadcast */
    error_code_t err = udp_enable_broadcast(sock);
    if (err != ERR_SUCCESS) {
        socket_close(sock);
        return err;
    }

    /* Bind to any port */
    err = udp_socket_bind(sock, "0.0.0.0", 0);
    if (err != ERR_SUCCESS) {
        socket_close(sock);
        return err;
    }

    /* Send broadcast discovery request */
    char broadcast_addr[64];
    get_broadcast_address(broadcast_addr, sizeof(broadcast_addr));

    LOG_DEBUG("Sending discovery broadcast to %s:%u", broadcast_addr, DISCOVERY_PORT);

    ssize_t sent = udp_sendto(sock, DISCOVERY_REQUEST, strlen(DISCOVERY_REQUEST),
                               broadcast_addr, DISCOVERY_PORT);
    if (sent < 0) {
        LOG_ERROR("Failed to send discovery broadcast");
        socket_close(sock);
        return ERR_SOCKET_SEND;
    }

    /* Also try subnet broadcast */
    sent = udp_sendto(sock, DISCOVERY_REQUEST, strlen(DISCOVERY_REQUEST),
                      "192.168.1.255", DISCOVERY_PORT);
    sent = udp_sendto(sock, DISCOVERY_REQUEST, strlen(DISCOVERY_REQUEST),
                      "192.168.0.255", DISCOVERY_PORT);
    sent = udp_sendto(sock, DISCOVERY_REQUEST, strlen(DISCOVERY_REQUEST),
                      "10.0.0.255", DISCOVERY_PORT);

    /* Collect responses */
    DWORD start_time = GetTickCount();
    char recv_buf[512];

    while (result->server_count < MAX_DISCOVERED_SERVERS) {
        DWORD elapsed = GetTickCount() - start_time;
        if (elapsed >= timeout_ms) {
            break;
        }

        char src_ip[64];
        uint16_t src_port;
        uint32_t remaining = timeout_ms - elapsed;

        ssize_t received = udp_recvfrom_timeout(sock, recv_buf, sizeof(recv_buf) - 1,
                                                  src_ip, sizeof(src_ip), &src_port,
                                                  (remaining > 500) ? 500 : remaining);

        if (received <= 0) {
            continue;
        }

        recv_buf[received] = '\0';

        /* Parse response: "USBIP_SERVER hostname device_count" */
        if (strncmp(recv_buf, DISCOVERY_RESPONSE, strlen(DISCOVERY_RESPONSE)) == 0) {
            char *ptr = recv_buf + strlen(DISCOVERY_RESPONSE);
            while (*ptr == ' ') ptr++;

            discovered_server_t *server = &result->servers[result->server_count];
            memset(server, 0, sizeof(discovered_server_t));

            strncpy(server->ip_address, src_ip, sizeof(server->ip_address) - 1);
            server->port = USBIP_PORT;
            server->response_time_ms = GetTickCount() - start_time;

            /* Parse hostname and device count */
            if (sscanf(ptr, "%127s %d", server->hostname, &server->device_count) >= 1) {
                result->server_count++;
                LOG_DEBUG("Discovered server: %s (%s) with %d devices",
                    server->ip_address, server->hostname, server->device_count);
            }
        }
    }

    socket_close(sock);

    LOG_INFO("Discovery complete: found %d servers", result->server_count);
    return ERR_SUCCESS;
}

/*
 * Query specific server
 */
error_code_t discovery_query(const char *ip_address, discovered_server_t *server, uint32_t timeout_ms) {
    if (ip_address == NULL || server == NULL) {
        return ERR_INVALID_PARAM;
    }

    memset(server, 0, sizeof(discovered_server_t));

    socket_t sock = udp_socket_create();
    if (!socket_is_valid(sock)) {
        return ERR_SOCKET_CREATE;
    }

    DWORD start_time = GetTickCount();

    /* Send discovery request */
    ssize_t sent = udp_sendto(sock, DISCOVERY_REQUEST, strlen(DISCOVERY_REQUEST),
                               ip_address, DISCOVERY_PORT);
    if (sent < 0) {
        socket_close(sock);
        return ERR_SOCKET_SEND;
    }

    /* Wait for response */
    char recv_buf[512];
    char src_ip[64];
    uint16_t src_port;

    ssize_t received = udp_recvfrom_timeout(sock, recv_buf, sizeof(recv_buf) - 1,
                                              src_ip, sizeof(src_ip), &src_port,
                                              timeout_ms);

    socket_close(sock);

    if (received <= 0) {
        return ERR_TIMEOUT;
    }

    recv_buf[received] = '\0';

    /* Parse response */
    if (strncmp(recv_buf, DISCOVERY_RESPONSE, strlen(DISCOVERY_RESPONSE)) != 0) {
        return ERR_PROTOCOL_INVALID;
    }

    char *ptr = recv_buf + strlen(DISCOVERY_RESPONSE);
    while (*ptr == ' ') ptr++;

    strncpy(server->ip_address, src_ip, sizeof(server->ip_address) - 1);
    server->port = USBIP_PORT;
    server->response_time_ms = GetTickCount() - start_time;

    sscanf(ptr, "%127s %d", server->hostname, &server->device_count);

    return ERR_SUCCESS;
}

/*
 * Check if server is reachable
 */
bool discovery_ping(const char *ip_address, uint16_t port, uint32_t timeout_ms) {
    if (ip_address == NULL) {
        return false;
    }

    /* Try to establish TCP connection */
    socket_t sock = tcp_client_connect_timeout(ip_address, port, timeout_ms);
    if (!socket_is_valid(sock)) {
        return false;
    }

    socket_close(sock);
    return true;
}

/*
 * Print discovered servers
 */
void discovery_print_results(const discovery_result_t *result) {
    if (result == NULL || result->server_count == 0) {
        printf("No servers found.\n");
        return;
    }

    printf("\nDiscovered servers:\n");
    printf("-------------------\n");

    for (int i = 0; i < result->server_count; i++) {
        const discovered_server_t *server = &result->servers[i];
        printf("[%d] %s", i + 1, server->ip_address);
        if (server->hostname[0]) {
            printf(" (%s)", server->hostname);
        }
        printf(" - %d device(s)", server->device_count);
        printf(" [%llu ms]\n", server->response_time_ms);
    }
    printf("\n");
}

/*
 * Clear discovery results
 */
void discovery_clear_results(discovery_result_t *result) {
    if (result != NULL) {
        memset(result, 0, sizeof(discovery_result_t));
    }
}
