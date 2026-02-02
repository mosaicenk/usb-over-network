/*
 * USB Over Network - Client Application
 * Windows-only implementation
 *
 * Main entry point for USB/IP client
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
#include "discovery.h"
#include "vhci.h"
#include "remote_device.h"

/* Command types */
typedef enum command_type {
    CMD_NONE = 0,
    CMD_DISCOVER,
    CMD_LIST,
    CMD_ATTACH,
    CMD_DETACH,
    CMD_STATUS,
    CMD_HELP
} command_type_t;

/* Global state */
static struct {
    bool running;
    command_type_t command;
    char server_ip[64];
    uint16_t server_port;
    char busid[USBIP_BUSID_MAX];
    int port;
    bool verbose;
    vhci_context_t vhci;
    remote_device_list_t device_list;
} g_client = {
    .running = false,
    .command = CMD_NONE,
    .server_port = USBIP_PORT,
    .port = -1,
    .verbose = false
};

/* ----- Signal Handling ----- */

static void signal_handler(int sig) {
    (void)sig;
    LOG_INFO("Shutdown signal received");
    g_client.running = false;
}

static void setup_signal_handlers(void) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
}

/* ----- Command Line Parsing ----- */

static void print_usage(const char *prog_name) {
    printf("%s v%s - USB/IP Client\n", APP_NAME, APP_VERSION);
    printf("\nUsage: %s [OPTIONS] <command> [args]\n", prog_name);
    printf("\nCommands:\n");
    printf("  discover                    Discover USB/IP servers on LAN\n");
    printf("  list <server_ip>            List devices on server\n");
    printf("  attach <server_ip> <busid>  Attach remote device\n");
    printf("  detach <port>               Detach device from local port\n");
    printf("  status                      Show attached devices\n");
    printf("\nOptions:\n");
    printf("  -p, --port PORT       Server port (default: %d)\n", USBIP_PORT);
    printf("  -v, --verbose         Verbose output\n");
    printf("  -h, --help            Show this help\n");
    printf("\nExamples:\n");
    printf("  %s discover\n", prog_name);
    printf("  %s list 192.168.1.100\n", prog_name);
    printf("  %s attach 192.168.1.100 1-2\n", prog_name);
    printf("  %s detach 0\n", prog_name);
}

static int parse_args(int argc, char *argv[]) {
    int i = 1;

    /* Parse options */
    while (i < argc && argv[i][0] == '-') {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return -1;
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            g_client.verbose = true;
            i++;
        }
        else if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) && i + 1 < argc) {
            g_client.server_port = (uint16_t)atoi(argv[++i]);
            i++;
        }
        else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return -1;
        }
    }

    /* Parse command */
    if (i >= argc) {
        fprintf(stderr, "No command specified\n");
        print_usage(argv[0]);
        return -1;
    }

    const char *cmd = argv[i++];

    if (strcmp(cmd, "discover") == 0) {
        g_client.command = CMD_DISCOVER;
    }
    else if (strcmp(cmd, "list") == 0) {
        g_client.command = CMD_LIST;
        if (i >= argc) {
            fprintf(stderr, "Missing server IP address\n");
            return -1;
        }
        strncpy(g_client.server_ip, argv[i++], sizeof(g_client.server_ip) - 1);
    }
    else if (strcmp(cmd, "attach") == 0) {
        g_client.command = CMD_ATTACH;
        if (i + 1 >= argc) {
            fprintf(stderr, "Missing server IP or bus ID\n");
            return -1;
        }
        strncpy(g_client.server_ip, argv[i++], sizeof(g_client.server_ip) - 1);
        strncpy(g_client.busid, argv[i++], sizeof(g_client.busid) - 1);
    }
    else if (strcmp(cmd, "detach") == 0) {
        g_client.command = CMD_DETACH;
        if (i >= argc) {
            fprintf(stderr, "Missing port number\n");
            return -1;
        }
        g_client.port = atoi(argv[i++]);
    }
    else if (strcmp(cmd, "status") == 0) {
        g_client.command = CMD_STATUS;
    }
    else if (strcmp(cmd, "help") == 0) {
        print_usage(argv[0]);
        return -1;
    }
    else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        print_usage(argv[0]);
        return -1;
    }

    return 0;
}

/* ----- Command Handlers ----- */

static int cmd_discover(void) {
    printf("Discovering USB/IP servers on LAN...\n\n");

    discovery_result_t result;
    error_code_t err = discovery_broadcast(&result, DISCOVERY_TIMEOUT_MS);

    if (err != ERR_SUCCESS) {
        fprintf(stderr, "Discovery failed: %s\n", error_string(err));
        return 1;
    }

    discovery_print_results(&result);

    if (result.server_count == 0) {
        printf("No servers responded. You can try direct connection with:\n");
        printf("  %s list <server_ip>\n", CLIENT_APP_NAME);
    }

    return 0;
}

static int cmd_list(void) {
    printf("Listing devices on %s:%u...\n\n", g_client.server_ip, g_client.server_port);

    usbip_usb_device_t devices[MAX_DEVICES];
    int device_count = 0;

    error_code_t err = remote_server_list_devices(
        g_client.server_ip, g_client.server_port,
        devices, &device_count, MAX_DEVICES);

    if (err != ERR_SUCCESS) {
        fprintf(stderr, "Failed to get device list: %s\n", error_string(err));
        return 1;
    }

    remote_server_print_devices(devices, device_count);

    if (device_count > 0) {
        printf("To attach a device, use:\n");
        printf("  %s attach %s <busid>\n", CLIENT_APP_NAME, g_client.server_ip);
    }

    return 0;
}

static int cmd_attach(void) {
    printf("Attaching device %s from %s:%u...\n",
        g_client.busid, g_client.server_ip, g_client.server_port);

    /* Initialize VHCI */
    error_code_t err = vhci_init(&g_client.vhci);
    if (err != ERR_SUCCESS) {
        fprintf(stderr, "Failed to initialize VHCI: %s\n", error_string(err));
        return 1;
    }

    /* Initialize device list */
    err = remote_device_list_init(&g_client.device_list);
    if (err != ERR_SUCCESS) {
        vhci_cleanup(&g_client.vhci);
        fprintf(stderr, "Failed to initialize device list: %s\n", error_string(err));
        return 1;
    }

    /* Connect to device */
    remote_device_t *device = NULL;
    err = remote_device_connect(&g_client.device_list, &g_client.vhci,
        g_client.server_ip, g_client.server_port,
        g_client.busid, &device);

    if (err != ERR_SUCCESS) {
        fprintf(stderr, "Failed to attach device: %s\n", error_string(err));
        remote_device_list_cleanup(&g_client.device_list);
        vhci_cleanup(&g_client.vhci);
        return 1;
    }

    printf("\nDevice attached successfully!\n");
    printf("  Port: %d\n", device->vhci_port);
    printf("  Device: %s (%04X:%04X)\n",
        device->busid,
        device->device_info.idVendor,
        device->device_info.idProduct);

    printf("\nPress Ctrl+C to detach and exit...\n");

    /* Keep running until interrupted */
    g_client.running = true;
    while (g_client.running) {
        Sleep(1000);
    }

    /* Cleanup */
    printf("\nDetaching device...\n");
    remote_device_disconnect(&g_client.device_list, device);
    remote_device_list_cleanup(&g_client.device_list);
    vhci_cleanup(&g_client.vhci);

    printf("Device detached.\n");
    return 0;
}

static int cmd_detach(void) {
    printf("Detaching device from port %d...\n", g_client.port);

    /* Initialize VHCI to access port info */
    error_code_t err = vhci_init(&g_client.vhci);
    if (err != ERR_SUCCESS) {
        fprintf(stderr, "Failed to initialize VHCI: %s\n", error_string(err));
        return 1;
    }

    /* Check if port is valid */
    if (g_client.port < 0 || g_client.port >= g_client.vhci.max_ports) {
        fprintf(stderr, "Invalid port number: %d (valid: 0-%d)\n",
            g_client.port, g_client.vhci.max_ports - 1);
        vhci_cleanup(&g_client.vhci);
        return 1;
    }

    /* Detach */
    err = vhci_detach(&g_client.vhci, g_client.port);
    if (err != ERR_SUCCESS) {
        fprintf(stderr, "Failed to detach: %s\n", error_string(err));
        vhci_cleanup(&g_client.vhci);
        return 1;
    }

    vhci_cleanup(&g_client.vhci);
    printf("Device detached from port %d.\n", g_client.port);

    return 0;
}

static int cmd_status(void) {
    printf("USB/IP Client Status\n");
    printf("====================\n\n");

    /* Initialize VHCI */
    error_code_t err = vhci_init(&g_client.vhci);
    if (err != ERR_SUCCESS) {
        fprintf(stderr, "Failed to initialize VHCI: %s\n", error_string(err));
        return 1;
    }

    /* Check VHCI driver */
    printf("VHCI Driver: %s\n", vhci_is_available() ? "Available" : "Not installed");

    /* Print VHCI status */
    vhci_print_status(&g_client.vhci);

    vhci_cleanup(&g_client.vhci);

    return 0;
}

/* ----- Initialization & Main ----- */

static error_code_t client_init(void) {
    /* Initialize logging */
    log_config_t log_config = {
        .level = g_client.verbose ? LOG_LEVEL_DEBUG : LOG_LEVEL_INFO,
        .targets = LOG_TARGET_CONSOLE,
        .show_timestamp = g_client.verbose,
        .show_level = g_client.verbose,
        .show_location = false,
        .use_colors = true
    };
    error_code_t err = log_init(&log_config);
    if (err != ERR_SUCCESS) {
        fprintf(stderr, "Failed to initialize logging\n");
        return err;
    }

    /* Initialize network */
    err = network_init();
    if (err != ERR_SUCCESS) {
        LOG_ERROR("Failed to initialize network: %s", error_string(err));
        return err;
    }

    return ERR_SUCCESS;
}

static void client_cleanup(void) {
    network_cleanup();
    log_cleanup();
}

int main(int argc, char *argv[]) {
    int exit_code = 0;

    /* Parse command line */
    if (parse_args(argc, argv) != 0) {
        return 1;
    }

    /* Setup signal handlers */
    setup_signal_handlers();

    /* Initialize */
    error_code_t err = client_init();
    if (err != ERR_SUCCESS) {
        fprintf(stderr, "Initialization failed: %s\n", error_string(err));
        return 1;
    }

    /* Execute command */
    switch (g_client.command) {
        case CMD_DISCOVER:
            exit_code = cmd_discover();
            break;
        case CMD_LIST:
            exit_code = cmd_list();
            break;
        case CMD_ATTACH:
            exit_code = cmd_attach();
            break;
        case CMD_DETACH:
            exit_code = cmd_detach();
            break;
        case CMD_STATUS:
            exit_code = cmd_status();
            break;
        default:
            fprintf(stderr, "No command specified\n");
            exit_code = 1;
            break;
    }

    /* Cleanup */
    client_cleanup();

    return exit_code;
}
