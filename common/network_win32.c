/*
 * USB Over Network - Network Implementation (Windows)
 * Windows Winsock2 implementation
 */

#include "network.h"
#include "log.h"
#include <mstcpip.h>
#include <iphlpapi.h>

/* Global state */
static struct {
    bool initialized;
    WSADATA wsa_data;
} g_network = {0};

/*
 * Initialize Winsock
 */
error_code_t network_init(void) {
    if (g_network.initialized) {
        return ERR_SUCCESS;
    }

    int result = WSAStartup(MAKEWORD(2, 2), &g_network.wsa_data);
    if (result != 0) {
        LOG_ERROR("WSAStartup failed: %d", result);
        return ERR_NETWORK_INIT;
    }

    LOG_DEBUG("Winsock initialized: %s", g_network.wsa_data.szDescription);
    g_network.initialized = true;
    return ERR_SUCCESS;
}

/*
 * Cleanup Winsock
 */
void network_cleanup(void) {
    if (g_network.initialized) {
        WSACleanup();
        g_network.initialized = false;
        LOG_DEBUG("Winsock cleanup complete");
    }
}

/*
 * Check if network is initialized
 */
bool network_is_initialized(void) {
    return g_network.initialized;
}

/*
 * Create TCP server socket
 */
socket_t tcp_server_create(const char *bind_addr, uint16_t port) {
    socket_t sock = INVALID_SOCKET_VAL;
    struct sockaddr_in addr;
    int result;

    /* Create socket */
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET_VAL) {
        LOG_WINSOCK_ERROR("socket() failed");
        return INVALID_SOCKET_VAL;
    }

    /* Set socket options */
    socket_set_reuseaddr(sock, true);
    socket_set_nodelay(sock, true);

    /* Bind */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (bind_addr == NULL || strcmp(bind_addr, "0.0.0.0") == 0) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        addr.sin_addr.s_addr = inet_addr(bind_addr);
    }

    result = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (result == SOCKET_ERROR) {
        LOG_WINSOCK_ERROR("bind() failed");
        closesocket(sock);
        return INVALID_SOCKET_VAL;
    }

    /* Listen */
    result = listen(sock, SOMAXCONN);
    if (result == SOCKET_ERROR) {
        LOG_WINSOCK_ERROR("listen() failed");
        closesocket(sock);
        return INVALID_SOCKET_VAL;
    }

    LOG_INFO("Server listening on %s:%u", bind_addr ? bind_addr : "0.0.0.0", port);
    return sock;
}

/*
 * Accept incoming connection
 */
socket_t tcp_server_accept(socket_t server_fd, char *client_ip, size_t ip_len, uint16_t *client_port) {
    struct sockaddr_in client_addr;
    int addr_len = sizeof(client_addr);

    socket_t client_sock = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (client_sock == INVALID_SOCKET_VAL) {
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK) {
            LOG_WINSOCK_ERROR("accept() failed");
        }
        return INVALID_SOCKET_VAL;
    }

    /* Get client info */
    if (client_ip != NULL && ip_len > 0) {
        strncpy(client_ip, inet_ntoa(client_addr.sin_addr), ip_len - 1);
        client_ip[ip_len - 1] = '\0';
    }
    if (client_port != NULL) {
        *client_port = ntohs(client_addr.sin_port);
    }

    /* Set socket options */
    socket_set_nodelay(client_sock, true);

    LOG_DEBUG("Accepted connection from %s:%u",
        inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    return client_sock;
}

/*
 * Accept with timeout
 */
socket_t tcp_server_accept_timeout(socket_t server_fd, char *client_ip, size_t ip_len,
                                   uint16_t *client_port, uint32_t timeout_ms) {
    socket_set_t set;
    socket_set_init(&set);
    socket_set_add_read(&set, server_fd);

    int result = socket_select(&set, timeout_ms);
    if (result <= 0) {
        return INVALID_SOCKET_VAL;
    }

    if (socket_is_readable(&set, server_fd)) {
        return tcp_server_accept(server_fd, client_ip, ip_len, client_port);
    }

    return INVALID_SOCKET_VAL;
}

/*
 * Connect to server
 */
socket_t tcp_client_connect(const char *host, uint16_t port) {
    socket_t sock = INVALID_SOCKET_VAL;
    struct sockaddr_in addr;
    struct hostent *he;

    /* Create socket */
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET_VAL) {
        LOG_WINSOCK_ERROR("socket() failed");
        return INVALID_SOCKET_VAL;
    }

    /* Resolve hostname */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    addr.sin_addr.s_addr = inet_addr(host);
    if (addr.sin_addr.s_addr == INADDR_NONE) {
        he = gethostbyname(host);
        if (he == NULL) {
            LOG_WINSOCK_ERROR("gethostbyname() failed");
            closesocket(sock);
            return INVALID_SOCKET_VAL;
        }
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    }

    /* Connect */
    int result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (result == SOCKET_ERROR) {
        LOG_WINSOCK_ERROR("connect() failed");
        closesocket(sock);
        return INVALID_SOCKET_VAL;
    }

    /* Set socket options */
    socket_set_nodelay(sock, true);

    LOG_DEBUG("Connected to %s:%u", host, port);
    return sock;
}

/*
 * Connect with timeout
 */
socket_t tcp_client_connect_timeout(const char *host, uint16_t port, uint32_t timeout_ms) {
    socket_t sock = INVALID_SOCKET_VAL;
    struct sockaddr_in addr;
    struct hostent *he;

    /* Create socket */
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET_VAL) {
        return INVALID_SOCKET_VAL;
    }

    /* Set non-blocking */
    socket_set_nonblocking(sock, true);

    /* Resolve and connect */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    addr.sin_addr.s_addr = inet_addr(host);
    if (addr.sin_addr.s_addr == INADDR_NONE) {
        he = gethostbyname(host);
        if (he == NULL) {
            closesocket(sock);
            return INVALID_SOCKET_VAL;
        }
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    }

    int result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (result == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK) {
            closesocket(sock);
            return INVALID_SOCKET_VAL;
        }

        /* Wait for connection */
        socket_set_t set;
        socket_set_init(&set);
        socket_set_add_write(&set, sock);

        result = socket_select(&set, timeout_ms);
        if (result <= 0 || !socket_is_writable(&set, sock)) {
            closesocket(sock);
            return INVALID_SOCKET_VAL;
        }

        /* Check connection result */
        int error = socket_get_error(sock);
        if (error != 0) {
            closesocket(sock);
            return INVALID_SOCKET_VAL;
        }
    }

    /* Set back to blocking and configure */
    socket_set_nonblocking(sock, false);
    socket_set_nodelay(sock, true);

    return sock;
}

/*
 * Send data (may be partial)
 */
ssize_t net_send(socket_t fd, const void *buf, size_t len) {
    int result = send(fd, (const char*)buf, (int)len, 0);
    if (result == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            return 0;
        }
        return -1;
    }
    return result;
}

/*
 * Receive data (may be partial)
 */
ssize_t net_recv(socket_t fd, void *buf, size_t len) {
    int result = recv(fd, (char*)buf, (int)len, 0);
    if (result == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            return 0;
        }
        return -1;
    }
    return result;
}

/*
 * Send all data (guaranteed complete)
 */
ssize_t net_send_all(socket_t fd, const void *buf, size_t len) {
    const char *ptr = (const char*)buf;
    size_t remaining = len;

    while (remaining > 0) {
        int sent = send(fd, ptr, (int)remaining, 0);
        if (sent == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                Sleep(1);
                continue;
            }
            return -1;
        }
        if (sent == 0) {
            return (ssize_t)(len - remaining);
        }
        ptr += sent;
        remaining -= sent;
    }

    return (ssize_t)len;
}

/*
 * Receive exact amount
 */
ssize_t net_recv_all(socket_t fd, void *buf, size_t len) {
    char *ptr = (char*)buf;
    size_t remaining = len;

    while (remaining > 0) {
        int received = recv(fd, ptr, (int)remaining, 0);
        if (received == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                Sleep(1);
                continue;
            }
            return -1;
        }
        if (received == 0) {
            /* Connection closed */
            return (ssize_t)(len - remaining);
        }
        ptr += received;
        remaining -= received;
    }

    return (ssize_t)len;
}

/*
 * Send all with timeout
 */
ssize_t net_send_all_timeout(socket_t fd, const void *buf, size_t len, uint32_t timeout_ms) {
    const char *ptr = (const char*)buf;
    size_t remaining = len;
    DWORD start = GetTickCount();

    while (remaining > 0) {
        DWORD elapsed = GetTickCount() - start;
        if (elapsed >= timeout_ms) {
            return -1;
        }

        socket_set_t set;
        socket_set_init(&set);
        socket_set_add_write(&set, fd);

        int result = socket_select(&set, timeout_ms - elapsed);
        if (result <= 0) {
            return -1;
        }

        int sent = send(fd, ptr, (int)remaining, 0);
        if (sent == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                continue;
            }
            return -1;
        }
        ptr += sent;
        remaining -= sent;
    }

    return (ssize_t)len;
}

/*
 * Receive exact amount with timeout
 */
ssize_t net_recv_all_timeout(socket_t fd, void *buf, size_t len, uint32_t timeout_ms) {
    char *ptr = (char*)buf;
    size_t remaining = len;
    DWORD start = GetTickCount();

    while (remaining > 0) {
        DWORD elapsed = GetTickCount() - start;
        if (elapsed >= timeout_ms) {
            return -1;
        }

        socket_set_t set;
        socket_set_init(&set);
        socket_set_add_read(&set, fd);

        int result = socket_select(&set, timeout_ms - elapsed);
        if (result <= 0) {
            return -1;
        }

        int received = recv(fd, ptr, (int)remaining, 0);
        if (received == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                continue;
            }
            return -1;
        }
        if (received == 0) {
            return (ssize_t)(len - remaining);
        }
        ptr += received;
        remaining -= received;
    }

    return (ssize_t)len;
}

/*
 * Create UDP socket
 */
socket_t udp_socket_create(void) {
    socket_t sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET_VAL) {
        LOG_WINSOCK_ERROR("UDP socket() failed");
    }
    return sock;
}

/*
 * Bind UDP socket
 */
error_code_t udp_socket_bind(socket_t fd, const char *bind_addr, uint16_t port) {
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (bind_addr == NULL || strcmp(bind_addr, "0.0.0.0") == 0) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        addr.sin_addr.s_addr = inet_addr(bind_addr);
    }

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        LOG_WINSOCK_ERROR("UDP bind() failed");
        return ERR_SOCKET_BIND;
    }

    return ERR_SUCCESS;
}

/*
 * Enable broadcast on UDP socket
 */
error_code_t udp_enable_broadcast(socket_t fd) {
    BOOL enable = TRUE;
    if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (char*)&enable, sizeof(enable)) == SOCKET_ERROR) {
        LOG_WINSOCK_ERROR("setsockopt(SO_BROADCAST) failed");
        return ERR_SOCKET_OPTION;
    }
    return ERR_SUCCESS;
}

/*
 * Send UDP datagram
 */
ssize_t udp_sendto(socket_t fd, const void *buf, size_t len,
                   const char *dest_addr, uint16_t dest_port) {
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(dest_port);
    addr.sin_addr.s_addr = inet_addr(dest_addr);

    int result = sendto(fd, (const char*)buf, (int)len, 0,
                        (struct sockaddr*)&addr, sizeof(addr));
    if (result == SOCKET_ERROR) {
        return -1;
    }
    return result;
}

/*
 * Receive UDP datagram
 */
ssize_t udp_recvfrom(socket_t fd, void *buf, size_t len,
                     char *src_addr, size_t addr_len, uint16_t *src_port) {
    struct sockaddr_in addr;
    int sockaddr_len = sizeof(addr);

    int result = recvfrom(fd, (char*)buf, (int)len, 0,
                          (struct sockaddr*)&addr, &sockaddr_len);
    if (result == SOCKET_ERROR) {
        return -1;
    }

    if (src_addr != NULL && addr_len > 0) {
        strncpy(src_addr, inet_ntoa(addr.sin_addr), addr_len - 1);
        src_addr[addr_len - 1] = '\0';
    }
    if (src_port != NULL) {
        *src_port = ntohs(addr.sin_port);
    }

    return result;
}

/*
 * Receive UDP with timeout
 */
ssize_t udp_recvfrom_timeout(socket_t fd, void *buf, size_t len,
                             char *src_addr, size_t addr_len, uint16_t *src_port,
                             uint32_t timeout_ms) {
    socket_set_t set;
    socket_set_init(&set);
    socket_set_add_read(&set, fd);

    int result = socket_select(&set, timeout_ms);
    if (result <= 0) {
        return result;
    }

    return udp_recvfrom(fd, buf, len, src_addr, addr_len, src_port);
}

/*
 * Set non-blocking mode
 */
error_code_t socket_set_nonblocking(socket_t fd, bool nonblocking) {
    u_long mode = nonblocking ? 1 : 0;
    if (ioctlsocket(fd, FIONBIO, &mode) == SOCKET_ERROR) {
        return ERR_SOCKET_OPTION;
    }
    return ERR_SUCCESS;
}

/*
 * Set TCP_NODELAY
 */
error_code_t socket_set_nodelay(socket_t fd, bool nodelay) {
    BOOL opt = nodelay ? TRUE : FALSE;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
        return ERR_SOCKET_OPTION;
    }
    return ERR_SUCCESS;
}

/*
 * Set SO_REUSEADDR
 */
error_code_t socket_set_reuseaddr(socket_t fd, bool reuse) {
    BOOL opt = reuse ? TRUE : FALSE;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
        return ERR_SOCKET_OPTION;
    }
    return ERR_SUCCESS;
}

/*
 * Set socket timeouts
 */
error_code_t socket_set_timeout(socket_t fd, uint32_t recv_timeout_ms, uint32_t send_timeout_ms) {
    DWORD recv_to = recv_timeout_ms;
    DWORD send_to = send_timeout_ms;

    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&recv_to, sizeof(recv_to)) == SOCKET_ERROR) {
        return ERR_SOCKET_OPTION;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char*)&send_to, sizeof(send_to)) == SOCKET_ERROR) {
        return ERR_SOCKET_OPTION;
    }
    return ERR_SUCCESS;
}

/*
 * Set keep-alive
 */
error_code_t socket_set_keepalive(socket_t fd, bool enable, uint32_t idle_sec,
                                   uint32_t interval_sec, uint32_t count) {
    BOOL opt = enable ? TRUE : FALSE;
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
        return ERR_SOCKET_OPTION;
    }

    if (enable) {
        struct tcp_keepalive keepalive;
        keepalive.onoff = 1;
        keepalive.keepalivetime = idle_sec * 1000;
        keepalive.keepaliveinterval = interval_sec * 1000;

        DWORD bytes_returned;
        if (WSAIoctl(fd, SIO_KEEPALIVE_VALS, &keepalive, sizeof(keepalive),
                     NULL, 0, &bytes_returned, NULL, NULL) == SOCKET_ERROR) {
            return ERR_SOCKET_OPTION;
        }
    }

    (void)count; /* Windows doesn't support keep-alive count */
    return ERR_SUCCESS;
}

/*
 * Set buffer sizes
 */
error_code_t socket_set_buffer_size(socket_t fd, int recv_size, int send_size) {
    if (recv_size > 0) {
        if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*)&recv_size, sizeof(recv_size)) == SOCKET_ERROR) {
            return ERR_SOCKET_OPTION;
        }
    }
    if (send_size > 0) {
        if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char*)&send_size, sizeof(send_size)) == SOCKET_ERROR) {
            return ERR_SOCKET_OPTION;
        }
    }
    return ERR_SUCCESS;
}

/*
 * Close socket
 */
void socket_close(socket_t fd) {
    if (fd != INVALID_SOCKET_VAL) {
        shutdown(fd, SD_BOTH);
        closesocket(fd);
    }
}

/*
 * Check if socket is valid
 */
bool socket_is_valid(socket_t fd) {
    return fd != INVALID_SOCKET_VAL;
}

/*
 * Check if socket is connected
 */
bool socket_is_connected(socket_t fd) {
    char buf;
    int result = recv(fd, &buf, 1, MSG_PEEK);
    if (result == SOCKET_ERROR) {
        int err = WSAGetLastError();
        return (err == WSAEWOULDBLOCK);
    }
    return (result >= 0);
}

/*
 * Get socket error
 */
int socket_get_error(socket_t fd) {
    int error = 0;
    int len = sizeof(error);
    getsockopt(fd, SOL_SOCKET, SO_ERROR, (char*)&error, &len);
    return error;
}

/*
 * Get local address
 */
error_code_t socket_get_local_addr(socket_t fd, char *addr, size_t addr_len, uint16_t *port) {
    struct sockaddr_in sa;
    int len = sizeof(sa);

    if (getsockname(fd, (struct sockaddr*)&sa, &len) == SOCKET_ERROR) {
        return ERR_SOCKET_OPTION;
    }

    if (addr != NULL && addr_len > 0) {
        strncpy(addr, inet_ntoa(sa.sin_addr), addr_len - 1);
        addr[addr_len - 1] = '\0';
    }
    if (port != NULL) {
        *port = ntohs(sa.sin_port);
    }
    return ERR_SUCCESS;
}

/*
 * Get peer address
 */
error_code_t socket_get_peer_addr(socket_t fd, char *addr, size_t addr_len, uint16_t *port) {
    struct sockaddr_in sa;
    int len = sizeof(sa);

    if (getpeername(fd, (struct sockaddr*)&sa, &len) == SOCKET_ERROR) {
        return ERR_SOCKET_OPTION;
    }

    if (addr != NULL && addr_len > 0) {
        strncpy(addr, inet_ntoa(sa.sin_addr), addr_len - 1);
        addr[addr_len - 1] = '\0';
    }
    if (port != NULL) {
        *port = ntohs(sa.sin_port);
    }
    return ERR_SUCCESS;
}

/*
 * Initialize socket set
 */
void socket_set_init(socket_set_t *set) {
    FD_ZERO(&set->read_fds);
    FD_ZERO(&set->write_fds);
    FD_ZERO(&set->error_fds);
    set->max_fd = 0;
}

/*
 * Add socket to read set
 */
void socket_set_add_read(socket_set_t *set, socket_t fd) {
    FD_SET(fd, &set->read_fds);
    if ((int)fd > set->max_fd) {
        set->max_fd = (int)fd;
    }
}

/*
 * Add socket to write set
 */
void socket_set_add_write(socket_set_t *set, socket_t fd) {
    FD_SET(fd, &set->write_fds);
    if ((int)fd > set->max_fd) {
        set->max_fd = (int)fd;
    }
}

/*
 * Add socket to error set
 */
void socket_set_add_error(socket_set_t *set, socket_t fd) {
    FD_SET(fd, &set->error_fds);
    if ((int)fd > set->max_fd) {
        set->max_fd = (int)fd;
    }
}

/*
 * Wait for socket events
 */
int socket_select(socket_set_t *set, uint32_t timeout_ms) {
    struct timeval tv;
    struct timeval *ptv = NULL;

    if (timeout_ms != (uint32_t)-1) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        ptv = &tv;
    }

    return select(set->max_fd + 1, &set->read_fds, &set->write_fds, &set->error_fds, ptv);
}

/*
 * Check if socket is readable
 */
bool socket_is_readable(socket_set_t *set, socket_t fd) {
    return FD_ISSET(fd, &set->read_fds) != 0;
}

/*
 * Check if socket is writable
 */
bool socket_is_writable(socket_set_t *set, socket_t fd) {
    return FD_ISSET(fd, &set->write_fds) != 0;
}

/*
 * Check if socket has error
 */
bool socket_has_error(socket_set_t *set, socket_t fd) {
    return FD_ISSET(fd, &set->error_fds) != 0;
}

/*
 * Resolve hostname to IP address (first AF_INET result).
 *
 * Uses getaddrinfo (Vista+) instead of the deprecated gethostbyname, which
 * cannot resolve IPv6/literal addresses and is not reentrant.
 */
error_code_t resolve_hostname(const char *hostname, char *ip_addr, size_t ip_len) {
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int rc = getaddrinfo(hostname, NULL, &hints, &res);
    if (rc != 0 || res == NULL) {
        return ERR_HOST_NOT_FOUND;
    }

    struct sockaddr_in *sa = (struct sockaddr_in *)res->ai_addr;
    if (inet_ntop(AF_INET, &sa->sin_addr, ip_addr, (socklen_t)ip_len) == NULL) {
        freeaddrinfo(res);
        return ERR_GENERAL;
    }

    freeaddrinfo(res);
    return ERR_SUCCESS;
}

/*
 * Pick a subnet-directed broadcast address for the first up, non-loopback
 * IPv4 interface. Falls back to limited broadcast (255.255.255.255) which
 * most home routers will relay.
 */
error_code_t get_broadcast_address(char *broadcast_addr, size_t addr_len) {
    ULONG buf_len = 16 * 1024;
    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                  GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_FRIENDLY_NAME;
   PIP_ADAPTER_ADDRESSES adapters = (PIP_ADAPTER_ADDRESSES)malloc(buf_len);
    if (adapters == NULL) {
        strncpy(broadcast_addr, "255.255.255.255", addr_len - 1);
        broadcast_addr[addr_len - 1] = '\0';
        return ERR_SUCCESS;
    }

    DWORD rc = GetAdaptersAddresses(AF_INET, flags, NULL, adapters, &buf_len);
    if (rc == ERROR_BUFFER_OVERFLOW) {
        free(adapters);
        adapters = (PIP_ADAPTER_ADDRESSES)malloc(buf_len);
        if (adapters == NULL) {
            strncpy(broadcast_addr, "255.255.255.255", addr_len - 1);
            broadcast_addr[addr_len - 1] = '\0';
            return ERR_SUCCESS;
        }
        rc = GetAdaptersAddresses(AF_INET, flags, NULL, adapters, &buf_len);
    }

    if (rc != NO_ERROR) {
        free(adapters);
        strncpy(broadcast_addr, "255.255.255.255", addr_len - 1);
        broadcast_addr[addr_len - 1] = '\0';
        return ERR_SUCCESS;
    }

    for (PIP_ADAPTER_ADDRESSES a = adapters; a != NULL; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp ||
            a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
            continue;
        }
        for (PIP_ADAPTER_UNICAST_ADDRESS u = a->FirstUnicastAddress; u != NULL; u = u->Next) {
            struct sockaddr_in *sa = (struct sockaddr_in *)u->Address.lpSockaddr;
            ULONG mask = 0;
            if (u->OnLinkPrefixLength < 32) {
                mask = htonl(~((1UL << (32 - u->OnLinkPrefixLength)) - 1));
            } else {
                mask = 0xFFFFFFFFu;
            }
            uint32_t bcast = (sa->sin_addr.s_addr & mask) | ~mask;
            struct in_addr baddr;
            baddr.s_addr = bcast;
            if (inet_ntop(AF_INET, &baddr, broadcast_addr, (socklen_t)addr_len) != NULL) {
                free(adapters);
                return ERR_SUCCESS;
            }
        }
    }

    free(adapters);
    strncpy(broadcast_addr, "255.255.255.255", addr_len - 1);
    broadcast_addr[addr_len - 1] = '\0';
    return ERR_SUCCESS;
}

/*
 * Get local IP address (first up, non-loopback IPv4 interface).
 */
error_code_t get_local_ip(char *ip_addr, size_t ip_len) {
    ULONG buf_len = 16 * 1024;
    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                  GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_FRIENDLY_NAME;
    PIP_ADAPTER_ADDRESSES adapters = (PIP_ADAPTER_ADDRESSES)malloc(buf_len);
    if (adapters == NULL) {
        return ERR_OUT_OF_MEMORY;
    }

    DWORD rc = GetAdaptersAddresses(AF_INET, flags, NULL, adapters, &buf_len);
    if (rc == ERROR_BUFFER_OVERFLOW) {
        free(adapters);
        adapters = (PIP_ADAPTER_ADDRESSES)malloc(buf_len);
        if (adapters == NULL) return ERR_OUT_OF_MEMORY;
        rc = GetAdaptersAddresses(AF_INET, flags, NULL, adapters, &buf_len);
    }
    if (rc != NO_ERROR) {
        free(adapters);
        return ERR_HOST_NOT_FOUND;
    }

    for (PIP_ADAPTER_ADDRESSES a = adapters; a != NULL; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp ||
            a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
            continue;
        }
        for (PIP_ADAPTER_UNICAST_ADDRESS u = a->FirstUnicastAddress; u != NULL; u = u->Next) {
            struct sockaddr_in *sa = (struct sockaddr_in *)u->Address.lpSockaddr;
            if (inet_ntop(AF_INET, &sa->sin_addr, ip_addr, (socklen_t)ip_len) != NULL) {
                free(adapters);
                return ERR_SUCCESS;
            }
        }
    }

    free(adapters);
    return ERR_HOST_NOT_FOUND;
}

/*
 * Validate dotted-quad IPv4 string. inet_addr treats 255.255.255.255 as the
 * INADDR_NONE sentinel, so it cannot be used to validate. inet_pton has no
 * such ambiguity and also rejects malformed input (leading zeros, etc.).
 */
bool is_valid_ip(const char *ip_str) {
    struct in_addr addr;
    return ip_str != NULL && inet_pton(AF_INET, ip_str, &addr) == 1;
}

/*
 * IP to string (static buffer)
 */
const char* ip_to_string(uint32_t ip) {
    struct in_addr addr;
    addr.s_addr = ip;
    return inet_ntoa(addr);
}

/*
 * String to IP
 */
uint32_t string_to_ip(const char *ip_str) {
    return inet_addr(ip_str);
}
