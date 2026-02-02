/*
 * USB Over Network - Network Abstraction Layer
 * Windows-only implementation
 *
 * Platform-independent network interface for TCP/UDP operations
 */

#ifndef NETWORK_H
#define NETWORK_H

#include "types.h"
#include "error.h"
#include "config.h"

/* Network initialization/cleanup */
error_code_t network_init(void);
void network_cleanup(void);
bool network_is_initialized(void);

/* ----- TCP Server Functions ----- */

/* Create TCP server socket and bind to address:port */
socket_t tcp_server_create(const char *bind_addr, uint16_t port);

/* Accept incoming connection (blocking) */
socket_t tcp_server_accept(socket_t server_fd, char *client_ip, size_t ip_len, uint16_t *client_port);

/* Accept with timeout (returns INVALID_SOCKET on timeout) */
socket_t tcp_server_accept_timeout(socket_t server_fd, char *client_ip, size_t ip_len,
                                   uint16_t *client_port, uint32_t timeout_ms);

/* ----- TCP Client Functions ----- */

/* Connect to server */
socket_t tcp_client_connect(const char *host, uint16_t port);

/* Connect with timeout */
socket_t tcp_client_connect_timeout(const char *host, uint16_t port, uint32_t timeout_ms);

/* ----- Data Transfer Functions ----- */

/* Send data (may send partial) */
ssize_t net_send(socket_t fd, const void *buf, size_t len);

/* Receive data (may receive partial) */
ssize_t net_recv(socket_t fd, void *buf, size_t len);

/* Send all data (guaranteed complete or error) */
ssize_t net_send_all(socket_t fd, const void *buf, size_t len);

/* Receive exact amount of data (guaranteed complete or error) */
ssize_t net_recv_all(socket_t fd, void *buf, size_t len);

/* Send all data with timeout */
ssize_t net_send_all_timeout(socket_t fd, const void *buf, size_t len, uint32_t timeout_ms);

/* Receive exact amount with timeout */
ssize_t net_recv_all_timeout(socket_t fd, void *buf, size_t len, uint32_t timeout_ms);

/* ----- UDP Functions ----- */

/* Create UDP socket */
socket_t udp_socket_create(void);

/* Bind UDP socket */
error_code_t udp_socket_bind(socket_t fd, const char *bind_addr, uint16_t port);

/* Enable broadcast on UDP socket */
error_code_t udp_enable_broadcast(socket_t fd);

/* Send UDP datagram */
ssize_t udp_sendto(socket_t fd, const void *buf, size_t len,
                   const char *dest_addr, uint16_t dest_port);

/* Receive UDP datagram */
ssize_t udp_recvfrom(socket_t fd, void *buf, size_t len,
                     char *src_addr, size_t addr_len, uint16_t *src_port);

/* Receive UDP with timeout */
ssize_t udp_recvfrom_timeout(socket_t fd, void *buf, size_t len,
                             char *src_addr, size_t addr_len, uint16_t *src_port,
                             uint32_t timeout_ms);

/* ----- Socket Options ----- */

/* Set socket to non-blocking mode */
error_code_t socket_set_nonblocking(socket_t fd, bool nonblocking);

/* Set TCP_NODELAY option */
error_code_t socket_set_nodelay(socket_t fd, bool nodelay);

/* Set SO_REUSEADDR option */
error_code_t socket_set_reuseaddr(socket_t fd, bool reuse);

/* Set socket timeouts */
error_code_t socket_set_timeout(socket_t fd, uint32_t recv_timeout_ms, uint32_t send_timeout_ms);

/* Set keep-alive */
error_code_t socket_set_keepalive(socket_t fd, bool enable, uint32_t idle_sec,
                                   uint32_t interval_sec, uint32_t count);

/* Set buffer sizes */
error_code_t socket_set_buffer_size(socket_t fd, int recv_size, int send_size);

/* ----- Socket Utilities ----- */

/* Close socket */
void socket_close(socket_t fd);

/* Check if socket is valid */
bool socket_is_valid(socket_t fd);

/* Check if socket is connected */
bool socket_is_connected(socket_t fd);

/* Get socket error */
int socket_get_error(socket_t fd);

/* Get local address of socket */
error_code_t socket_get_local_addr(socket_t fd, char *addr, size_t addr_len, uint16_t *port);

/* Get peer address of socket */
error_code_t socket_get_peer_addr(socket_t fd, char *addr, size_t addr_len, uint16_t *port);

/* ----- Select/Poll Operations ----- */

/* Socket set for select operations */
typedef struct {
    fd_set read_fds;
    fd_set write_fds;
    fd_set error_fds;
    int max_fd;
} socket_set_t;

/* Initialize socket set */
void socket_set_init(socket_set_t *set);

/* Add socket to read set */
void socket_set_add_read(socket_set_t *set, socket_t fd);

/* Add socket to write set */
void socket_set_add_write(socket_set_t *set, socket_t fd);

/* Add socket to error set */
void socket_set_add_error(socket_set_t *set, socket_t fd);

/* Wait for socket events */
int socket_select(socket_set_t *set, uint32_t timeout_ms);

/* Check if socket is readable */
bool socket_is_readable(socket_set_t *set, socket_t fd);

/* Check if socket is writable */
bool socket_is_writable(socket_set_t *set, socket_t fd);

/* Check if socket has error */
bool socket_has_error(socket_set_t *set, socket_t fd);

/* ----- Address Utilities ----- */

/* Resolve hostname to IP address */
error_code_t resolve_hostname(const char *hostname, char *ip_addr, size_t ip_len);

/* Get broadcast address for interface */
error_code_t get_broadcast_address(char *broadcast_addr, size_t addr_len);

/* Get local IP address */
error_code_t get_local_ip(char *ip_addr, size_t ip_len);

/* Validate IP address string */
bool is_valid_ip(const char *ip_str);

/* Convert IP address to string */
const char* ip_to_string(uint32_t ip);

/* Convert string to IP address */
uint32_t string_to_ip(const char *ip_str);

#endif /* NETWORK_H */
