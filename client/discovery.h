/*
 * USB Over Network - Server Discovery
 * Windows-only implementation
 *
 * LAN server discovery using UDP broadcast
 */

#ifndef DISCOVERY_H
#define DISCOVERY_H

#include "../common/types.h"
#include "../common/error.h"
#include "../common/config.h"

/* Maximum number of discovered servers */
#define MAX_DISCOVERED_SERVERS 32

/* Discovered server information */
typedef struct discovered_server {
    char ip_address[64];            /* Server IP address */
    uint16_t port;                  /* Server port */
    char hostname[128];             /* Server hostname */
    int device_count;               /* Number of devices on server */
    uint64_t response_time_ms;      /* Response time in milliseconds */
} discovered_server_t;

/* Discovery result structure */
typedef struct discovery_result {
    discovered_server_t servers[MAX_DISCOVERED_SERVERS];
    int server_count;
} discovery_result_t;

/* ----- Discovery Functions ----- */

/* Discover servers on LAN using UDP broadcast */
error_code_t discovery_broadcast(discovery_result_t *result, uint32_t timeout_ms);

/* Send discovery request to specific IP */
error_code_t discovery_query(const char *ip_address, discovered_server_t *server, uint32_t timeout_ms);

/* Check if server is reachable */
bool discovery_ping(const char *ip_address, uint16_t port, uint32_t timeout_ms);

/* ----- Utility Functions ----- */

/* Print discovered servers */
void discovery_print_results(const discovery_result_t *result);

/* Clear discovery results */
void discovery_clear_results(discovery_result_t *result);

#endif /* DISCOVERY_H */
