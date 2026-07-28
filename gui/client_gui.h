/*
 * USB Over Network - Client GUI Interface
 * Windows GUI for USB/IP client
 */

#ifndef CLIENT_GUI_H
#define CLIENT_GUI_H

#include "gui_common.h"
#include "../client/discovery.h"
#include "../client/remote_device.h"
#include "../client/vhci.h"
#include "../common/auth.h"

/* Client GUI Context */
typedef struct client_gui_context {
    gui_context_t base;

    /* Client state */
    vhci_context_t vhci;
    remote_device_list_t device_list;
    char current_server[64];
    uint16_t current_port;
    bool connected;

    /* Controls */
    HWND hLabelServer;
    HWND hEditServer;
    HWND hLabelToken;
    HWND hEditToken;
    HWND hBtnDiscover;
    HWND hBtnConnect;
    HWND hLabelDevices;
    HWND hBtnRefresh;

    /* Discovery thread */
    HANDLE discovery_thread;
    discovery_result_t discovery_result;

} client_gui_context_t;

/* Initialize client GUI */
bool client_gui_init(client_gui_context_t *ctx, HINSTANCE hInstance);

/* Run message loop */
int client_gui_run(client_gui_context_t *ctx);

/* Cleanup */
void client_gui_cleanup(client_gui_context_t *ctx);

/* Set server address */
void client_gui_set_server(client_gui_context_t *ctx, const char *ip);

/* Connect to server */
bool client_gui_connect(client_gui_context_t *ctx);

/* Disconnect from server */
void client_gui_disconnect(client_gui_context_t *ctx);

/* Refresh device list */
void client_gui_refresh_devices(client_gui_context_t *ctx);

/* Attach selected device */
bool client_gui_attach_selected(client_gui_context_t *ctx);

/* Detach selected device */
void client_gui_detach_selected(client_gui_context_t *ctx);

/* Attach device by busid (for checkbox) */
error_code_t client_gui_attach_device(client_gui_context_t *ctx, const char *busid);

/* Detach device by busid (for checkbox) */
error_code_t client_gui_detach_device(client_gui_context_t *ctx, const char *busid);

/* Start discovery */
void client_gui_start_discovery(client_gui_context_t *ctx);

/* Entry point for GUI client */
int client_gui_main(HINSTANCE hInstance, int nCmdShow);

#endif /* CLIENT_GUI_H */
