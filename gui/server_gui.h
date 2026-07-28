/*
 * USB Over Network - Server GUI Interface
 * Windows GUI for USB sharing server
 */

#ifndef SERVER_GUI_H
#define SERVER_GUI_H

#include "gui_common.h"
#include "../server/device_list.h"
#include "../server/client_manager.h"
#include "../common/auth.h"

/* Server GUI Context */
typedef struct server_gui_context {
    gui_context_t base;

    /* Server state */
    device_list_t device_list;
    client_manager_t client_manager;
    bool server_running;
    socket_t server_socket;
    HANDLE server_thread;

    /* Auth token (empty = auth disabled). Read from the token field at start. */
    char auth_token[AUTH_TOKEN_MAX_LEN];

    /* Discovery state */
    socket_t discovery_socket;
    HANDLE discovery_thread;

    /* Controls */
    HWND hLabelDevices;
    HWND hLabelClients;
    HWND hLabelToken;
    HWND hEditToken;
    HWND hBtnRefresh;
    HWND hBtnHide;

} server_gui_context_t;

/* Initialize server GUI */
bool server_gui_init(server_gui_context_t *ctx, HINSTANCE hInstance);

/* Run message loop */
int server_gui_run(server_gui_context_t *ctx);

/* Cleanup */
void server_gui_cleanup(server_gui_context_t *ctx);

/* Refresh device list in UI */
void server_gui_refresh_devices(server_gui_context_t *ctx);

/* Update client list in UI */
void server_gui_update_clients(server_gui_context_t *ctx);

/* Toggle device sharing */
void server_gui_toggle_share(server_gui_context_t *ctx, int index, bool share);

/* Stop sharing all devices */
void server_gui_stop_all(server_gui_context_t *ctx);

/* Start/Stop server */
bool server_gui_start_server(server_gui_context_t *ctx);
void server_gui_stop_server(server_gui_context_t *ctx);

/* Entry point for GUI server */
int server_gui_main(HINSTANCE hInstance, int nCmdShow);

#endif /* SERVER_GUI_H */
