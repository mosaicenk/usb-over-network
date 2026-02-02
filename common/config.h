/*
 * USB Over Network - Configuration Constants
 * Windows-only implementation
 *
 * Compile-time configuration for USB/IP implementation
 */

#ifndef CONFIG_H
#define CONFIG_H

/* USB/IP Protocol Version */
#define USBIP_VERSION               0x0111

/* Network Configuration */
#define USBIP_PORT                  3240
#define DISCOVERY_PORT              3241
#define BIND_ADDRESS                "0.0.0.0"

/* Connection Limits */
#define MAX_CLIENTS                 16
#define MAX_DEVICES                 32
#define MAX_PENDING_URBS            256

/* Buffer Sizes */
#define RECV_BUFFER_SIZE            65536       /* 64KB receive buffer */
#define SEND_BUFFER_SIZE            65536       /* 64KB send buffer */
#define URB_BUFFER_SIZE             262144      /* 256KB max URB size */
#define COMMAND_BUFFER_SIZE         4096        /* 4KB command buffer */

/* Timeouts (in milliseconds) */
#define CONNECTION_TIMEOUT_MS       30000       /* 30 seconds */
#define URB_TIMEOUT_MS              5000        /* 5 seconds */
#define DISCOVERY_TIMEOUT_MS        3000        /* 3 seconds */
#define POLL_TIMEOUT_MS             1000        /* 1 second */
#define KEEPALIVE_INTERVAL_MS       10000       /* 10 seconds */

/* Discovery Protocol */
#define DISCOVERY_MAGIC             "USBIP_DISCOVER"
#define SERVER_RESPONSE_MAGIC       "USBIP_SERVER"
#define DISCOVERY_MAGIC_LEN         14
#define SERVER_RESPONSE_MAGIC_LEN   12

/* USB/IP Path Lengths */
#define USBIP_PATH_MAX              256
#define USBIP_BUSID_MAX             32
#define USBIP_DEV_PATH_MAX          256

/* Retry Configuration */
#define MAX_CONNECT_RETRIES         3
#define RETRY_DELAY_MS              1000

/* Log Configuration */
#define LOG_BUFFER_SIZE             1024
#define LOG_FILE_MAX_SIZE           (10 * 1024 * 1024)  /* 10MB */
#define DEFAULT_LOG_LEVEL           LOG_LEVEL_INFO

/* Application Info */
#define APP_NAME                    "USB Over Network"
#define APP_VERSION                 "1.0.0"
#define APP_COMPANY                 "CTK Technologies"
#define SERVER_APP_NAME             "USB Over Network - Server"
#define CLIENT_APP_NAME             "USB Over Network - Client"

/* Windows-specific USB Configuration */
#define USB_ENUM_GUID_CLASS         "{A5DCBF10-6530-11D2-901F-00C04FB951ED}"  /* USB device interface GUID */
#define USB_MAX_DESCRIPTOR_SIZE     4096

/* VHCI Configuration for Windows */
#define VHCI_DRIVER_NAME            "usbip_vhci"
#define VHCI_DEVICE_NAME            "\\\\.\\usbip_vhci"
#define VHCI_MAX_PORTS              8

/* Thread Pool Size */
#define WORKER_THREAD_COUNT         4

/* Queue Sizes */
#define EVENT_QUEUE_SIZE            1024
#define URB_QUEUE_SIZE              512

/* Network Socket Options */
#define SOCKET_RECV_TIMEOUT_MS      5000
#define SOCKET_SEND_TIMEOUT_MS      5000
#define TCP_NODELAY_ENABLED         1
#define SO_REUSEADDR_ENABLED        1

/* Debug Configuration */
#ifdef _DEBUG
#define DEBUG_PROTOCOL              1
#define DEBUG_USB                   1
#define DEBUG_NETWORK               1
#else
#define DEBUG_PROTOCOL              0
#define DEBUG_USB                   0
#define DEBUG_NETWORK               0
#endif

#endif /* CONFIG_H */
