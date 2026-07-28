/*
 * USB Over Network - Server Application
 * Windows-only implementation
 *
 * Main entry point for USB/IP server
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "../common/types.h"
#include "../common/config.h"
#include "../common/error.h"
#include "../common/log.h"
#include "../common/network.h"
#include "../common/protocol.h"
#include "../common/auth.h"
#include "../common/string_utils.h"
#include "usb_host.h"
#include "device_list.h"
#include "client_manager.h"
#include "urb_handler.h"

/* Global state */
static struct {
    bool running;
    socket_t server_socket;
    device_list_t device_list;
    client_manager_t client_manager;
    uint16_t port;
    char bind_address[64];
    char device_filter[USBIP_BUSID_MAX];
    const char *auth_token;
    bool verbose;
    bool list_only;
} g_server = {
    .running = false,
    .server_socket = INVALID_SOCKET_VAL,
    .port = USBIP_PORT,
    .bind_address = "0.0.0.0",
    .device_filter = "",
    .auth_token = NULL,
    .verbose = false,
    .list_only = false
};

/* ----- Signal Handling ----- */

static void signal_handler(int sig) {
    (void)sig;
    LOG_INFO("Shutdown signal received");
    g_server.running = false;
}

static void setup_signal_handlers(void) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
}

/* ----- Command Line Parsing ----- */

static void print_usage(const char *prog_name) {
    printf("%s v%s - USB/IP Server\n", APP_NAME, APP_VERSION);
    printf("\nUsage: %s [OPTIONS]\n", prog_name);
    printf("\nOptions:\n");
    printf("  -p, --port PORT       Listen port (default: %d)\n", USBIP_PORT);
    printf("  -b, --bind ADDR       Bind address (default: 0.0.0.0)\n");
    printf("  -d, --device BUSID    Share only specific device\n");
    printf("  -k, --auth-token T    Preshared token clients must present\n");
    printf("                        (default: $USBIP_AUTH_TOKEN env, empty=off)\n");
    printf("  -l, --list            List available USB devices and exit\n");
    printf("  -v, --verbose         Verbose output\n");
    printf("  -h, --help            Show this help\n");
    printf("\nExamples:\n");
    printf("  %s                    Share all USB devices on port %d\n", prog_name, USBIP_PORT);
    printf("  %s -d 1-2             Share only device 1-2\n", prog_name);
    printf("  %s -p 5000 -v         Use port 5000 with verbose logging\n", prog_name);
}

static int parse_args(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return -1;
        }
        else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0) {
            g_server.list_only = true;
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            g_server.verbose = true;
        }
        else if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) && i + 1 < argc) {
            g_server.port = (uint16_t)atoi(argv[++i]);
        }
        else if ((strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--bind") == 0) && i + 1 < argc) {
            str_copy(g_server.bind_address, argv[++i], sizeof(g_server.bind_address));
        }
        else if ((strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--device") == 0) && i + 1 < argc) {
            str_copy(g_server.device_filter, argv[++i], sizeof(g_server.device_filter));
        }
        else if ((strcmp(argv[i], "-k") == 0 || strcmp(argv[i], "--auth-token") == 0) && i + 1 < argc) {
            g_server.auth_token = argv[++i];
        }
        else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return -1;
        }
    }
    return 0;
}

/* ----- Device Listing ----- */

static void print_device_callback(const usb_device_t *device, void *user_data) {
    (void)user_data;

    printf("  %s: %04X:%04X\n", device->busid, device->vendor_id, device->product_id);
    printf("    Path: %s\n", device->path);
    printf("    Class: %s (%02X/%02X/%02X)\n",
        usb_class_string(device->device_class),
        device->device_class, device->device_subclass, device->device_protocol);
    printf("    Speed: %s\n", usb_speed_string(device->speed));
    if (device->manufacturer[0]) {
        printf("    Manufacturer: %s\n", device->manufacturer);
    }
    if (device->product[0]) {
        printf("    Product: %s\n", device->product);
    }
    if (device->serial[0]) {
        printf("    Serial: %s\n", device->serial);
    }
    printf("\n");
}

static int list_devices(void) {
    printf("Available USB devices:\n\n");

    error_code_t err = usb_enumerate_devices(print_device_callback, NULL);
    if (err != ERR_SUCCESS) {
        fprintf(stderr, "Error enumerating devices: %s\n", error_string(err));
        return 1;
    }

    return 0;
}

/* ----- Client Handling ----- */

static error_code_t handle_client_session(client_connection_t *client) {
    error_code_t err;

    while (g_server.running && client->state != CLIENT_STATE_DISCONNECTED) {
        if (client->state == CLIENT_STATE_CONNECTED) {
            /* Handle browsing client (device list, import requests) */
            err = client_handle_request(&g_server.client_manager, client);
            if (err != ERR_SUCCESS) {
                if (err == ERR_SOCKET_RECV) {
                    LOG_DEBUG("Client %u disconnected", client->id);
                } else {
                    LOG_WARN("Client %u request error: %s", client->id, error_string(err));
                }
                break;
            }
        }
        else if (client->state == CLIENT_STATE_ATTACHED) {
            /* Handle URB forwarding */
            usbip_header_t urb_header;
            err = usbip_recv_urb_header(client->socket, &urb_header);
            if (err != ERR_SUCCESS) {
                LOG_DEBUG("Client %u URB receive error", client->id);
                break;
            }

            if (urb_header.common.command == USBIP_CMD_SUBMIT) {
                /* Receive OUT data if present */
                uint8_t *data = NULL;
                uint32_t data_len = 0;

                if (urb_header.common.direction == USBIP_DIR_OUT &&
                    urb_header.u.cmd_submit.transfer_buffer_length > 0) {
                    data_len = urb_header.u.cmd_submit.transfer_buffer_length;
                    data = (uint8_t *)malloc(data_len);
                    if (data == NULL) {
                        LOG_ERROR("Failed to allocate URB data buffer");
                        break;
                    }
                    err = usbip_recv_urb_data(client->socket, data, data_len);
                    if (err != ERR_SUCCESS) {
                        free(data);
                        break;
                    }
                    client->bytes_received += data_len;
                }

                /* Forward URB to device */
                err = client_forward_urb(client, &urb_header, data, data_len);
                free(data);

                if (err != ERR_SUCCESS) {
                    LOG_WARN("URB forward error: %s", error_string(err));
                }

                /* Check for completed URBs and send responses */
                urb_entry_t *completed;
                while ((completed = urb_handler_get_completed(client->urb_handler)) != NULL) {
                    err = client_send_urb_completion(client, completed);
                    urb_handler_free_entry(completed);
                    if (err != ERR_SUCCESS) {
                        LOG_WARN("URB completion send error");
                        break;
                    }
                }
            }
            else if (urb_header.common.command == USBIP_CMD_UNLINK) {
                /* Handle URB unlink */
                urb_handler_unlink(client->urb_handler, urb_header.u.cmd_unlink.seqnum_to_unlink);

                /* Send unlink response */
                usbip_header_t unlink_ret;
                usbip_create_urb_unlink_ret(&unlink_ret, urb_header.common.seqnum,
                    urb_header.common.devid, 0);
                usbip_send_urb_header(client->socket, &unlink_ret);
            }
        }
    }

    return ERR_SUCCESS;
}

/* ----- Main Server Loop ----- */

static error_code_t server_main_loop(void) {
    LOG_INFO("Server starting main loop");

    while (g_server.running) {
        /* Accept new connection */
        char client_ip[64];
        uint16_t client_port;

        socket_t client_socket = tcp_server_accept_timeout(
            g_server.server_socket, client_ip, sizeof(client_ip),
            &client_port, 1000);

        if (!socket_is_valid(client_socket)) {
            /* Timeout or error, check if we should continue */
            continue;
        }

        /* Authenticate before any USB/IP traffic. With auth disabled this is
         * a no-op, preserving interop with plain USB/IP clients. */
        if (auth_is_enabled(g_server.auth_token)) {
            error_code_t auth_err = auth_server_handshake(client_socket, g_server.auth_token);
            if (auth_err != ERR_SUCCESS) {
                LOG_WARN("Rejecting %s:%u - auth failed (%s)",
                    client_ip, client_port, error_string(auth_err));
                socket_close(client_socket);
                continue;
            }
        }

        /* Add new client */
        client_connection_t *client = NULL;
        error_code_t err = client_manager_add(&g_server.client_manager,
            client_socket, client_ip, client_port, &client);

        if (err != ERR_SUCCESS) {
            LOG_WARN("Failed to add client: %s", error_string(err));
            socket_close(client_socket);
            continue;
        }

        /* Handle client session (synchronous for now) */
        /* For production, this should be handled in a separate thread */
        handle_client_session(client);

        /* Cleanup client */
        client_detach_device(&g_server.client_manager, client);
        client_manager_remove(&g_server.client_manager, client);
    }

    return ERR_SUCCESS;
}

/* ----- Initialization & Cleanup ----- */

static error_code_t server_init(void) {
    error_code_t err;

    /* Initialize logging */
    log_config_t log_config = {
        .level = g_server.verbose ? LOG_LEVEL_DEBUG : LOG_LEVEL_INFO,
        .targets = LOG_TARGET_CONSOLE,
        .show_timestamp = true,
        .show_level = true,
        .show_location = g_server.verbose,
        .use_colors = true
    };
    err = log_init(&log_config);
    if (err != ERR_SUCCESS) {
        fprintf(stderr, "Failed to initialize logging\n");
        return err;
    }

    LOG_INFO("%s v%s starting...", APP_NAME, APP_VERSION);

    /* Initialize network */
    err = network_init();
    if (err != ERR_SUCCESS) {
        LOG_ERROR("Failed to initialize network: %s", error_string(err));
        return err;
    }

    /* Initialize USB host */
    err = usb_host_init();
    if (err != ERR_SUCCESS) {
        LOG_ERROR("Failed to initialize USB host: %s", error_string(err));
        network_cleanup();
        return err;
    }

    /* Initialize device list */
    err = device_list_init(&g_server.device_list);
    if (err != ERR_SUCCESS) {
        LOG_ERROR("Failed to initialize device list: %s", error_string(err));
        usb_host_cleanup();
        network_cleanup();
        return err;
    }

    /* Enumerate devices */
    err = device_list_refresh(&g_server.device_list);
    if (err != ERR_SUCCESS) {
        LOG_WARN("Failed to enumerate USB devices: %s", error_string(err));
    }

    /* Apply device filter if specified */
    if (g_server.device_filter[0] != '\0') {
        LOG_INFO("Sharing only device: %s", g_server.device_filter);
        device_list_unshare_all(&g_server.device_list);
        device_list_set_shared(&g_server.device_list, g_server.device_filter, true);
    }

    /* Initialize client manager */
    err = client_manager_init(&g_server.client_manager, &g_server.device_list);
    if (err != ERR_SUCCESS) {
        LOG_ERROR("Failed to initialize client manager: %s", error_string(err));
        device_list_cleanup(&g_server.device_list);
        usb_host_cleanup();
        network_cleanup();
        return err;
    }

    /* Create server socket */
    g_server.server_socket = tcp_server_create(g_server.bind_address, g_server.port);
    if (!socket_is_valid(g_server.server_socket)) {
        LOG_ERROR("Failed to create server socket");
        client_manager_cleanup(&g_server.client_manager);
        device_list_cleanup(&g_server.device_list);
        usb_host_cleanup();
        network_cleanup();
        return ERR_SOCKET_CREATE;
    }

    g_server.running = true;
    LOG_INFO("Server initialized successfully");

    /* Resolve auth token: explicit -k > USBIP_AUTH_TOKEN env > default (off). */
    g_server.auth_token = auth_get_token(g_server.auth_token);
    if (auth_is_enabled(g_server.auth_token)) {
        LOG_INFO("Authentication enabled (preshared token)");
    } else {
        LOG_WARN("Authentication disabled - any LAN host can attach devices");
    }

    return ERR_SUCCESS;
}

static void server_cleanup(void) {
    LOG_INFO("Server shutting down...");

    g_server.running = false;

    /* Disconnect all clients */
    client_manager_disconnect_all(&g_server.client_manager);

    /* Close server socket */
    if (socket_is_valid(g_server.server_socket)) {
        socket_close(g_server.server_socket);
        g_server.server_socket = INVALID_SOCKET_VAL;
    }

    /* Cleanup managers */
    client_manager_cleanup(&g_server.client_manager);
    device_list_cleanup(&g_server.device_list);

    /* Cleanup subsystems */
    usb_host_cleanup();
    network_cleanup();
    log_cleanup();

    LOG_INFO("Server shutdown complete");
}

/* ----- Main Entry Point ----- */

int main(int argc, char *argv[]) {
    int exit_code = 0;

    /* Parse command line arguments */
    if (parse_args(argc, argv) != 0) {
        return 1;
    }

    /* List devices and exit if requested */
    if (g_server.list_only) {
        log_config_t log_config = {
            .level = LOG_LEVEL_WARN,
            .targets = LOG_TARGET_CONSOLE,
            .show_timestamp = false,
            .show_level = false,
            .use_colors = false
        };
        log_init(&log_config);
        network_init();
        usb_host_init();

        exit_code = list_devices();

        usb_host_cleanup();
        network_cleanup();
        log_cleanup();
        return exit_code;
    }

    /* Setup signal handlers */
    setup_signal_handlers();

    /* Initialize server */
    error_code_t err = server_init();
    if (err != ERR_SUCCESS) {
        fprintf(stderr, "Server initialization failed: %s\n", error_string(err));
        return 1;
    }

    /* Print shared devices */
    device_list_print(&g_server.device_list);

    /* Run main loop */
    err = server_main_loop();
    if (err != ERR_SUCCESS) {
        LOG_ERROR("Server error: %s", error_string(err));
        exit_code = 1;
    }

    /* Cleanup */
    server_cleanup();

    return exit_code;
}
