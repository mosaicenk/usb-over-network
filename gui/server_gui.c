/*
 * USB Over Network - Server GUI Implementation
 * Windows GUI for USB sharing server
 */

#include "server_gui.h"
#include "../common/log.h"
#include "../common/network.h"
#include "../common/config.h"
#include "../common/auth.h"
#include "../common/string_utils.h"
#include "../server/usb_host.h"
#include <stdio.h>

/* Global context pointer for window procedure */
static server_gui_context_t *g_server_ctx = NULL;

/* Guard flag to prevent recursive checkbox handling */
static bool g_updating_checkbox = false;

/* Helper to get ListView item text using SendMessageW for MinGW Unicode compatibility */
static void gui_lv_get_item_text(HWND hList, int row, int col, wchar_t *buf, int bufLen) {
    LVITEMW lvi = {0};
    lvi.iSubItem = col;
    lvi.pszText = buf;
    lvi.cchTextMax = bufLen;
    SendMessageW(hList, LVM_GETITEMTEXTW, (WPARAM)row, (LPARAM)&lvi);
}

/* Forward declarations */
static LRESULT CALLBACK ServerWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void create_device_columns(HWND hList);
static void create_client_columns(HWND hList);
static void gui_layout_server(server_gui_context_t *ctx);
static void populate_device_list(server_gui_context_t *ctx);
static void on_device_checkbox_changed(server_gui_context_t *ctx, int index);
static DWORD WINAPI server_accept_thread(LPVOID param);
static DWORD WINAPI client_handler_thread(LPVOID param);

/* Client handler data */
typedef struct client_handler_data {
    server_gui_context_t *gui_ctx;
    client_connection_t *client;
} client_handler_data_t;

/* ----- Window Creation ----- */

static bool register_server_class(HINSTANCE hInstance) {
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = ServerWndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    if (!wc.hIcon) wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = SERVER_CLASS_NAME;
    wc.hIconSm = wc.hIcon;

    return RegisterClassExW(&wc) != 0;
}

static bool create_server_window(server_gui_context_t *ctx) {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - SERVER_WINDOW_WIDTH) / 2;
    int y = (screenH - SERVER_WINDOW_HEIGHT) / 2;

    ctx->base.hMainWnd = CreateWindowExW(
        0, SERVER_CLASS_NAME, L"USB Over Network Server - CTK Technologies",
        WS_OVERLAPPEDWINDOW,
        x, y, SERVER_WINDOW_WIDTH, SERVER_WINDOW_HEIGHT,
        NULL, NULL, ctx->base.hInstance, NULL
    );

    return ctx->base.hMainWnd != NULL;
}

static void create_server_controls(server_gui_context_t *ctx) {
    HWND hWnd = ctx->base.hMainWnd;
    int y = MARGIN;

    /* Auth token row (optional; leave blank to accept all LAN clients) */
    ctx->hLabelToken = gui_create_label(hWnd, IDC_SERVER_LABEL_TOKEN,
        L"Auth Token:", MARGIN, y + 4, 80, LABEL_HEIGHT);
    ctx->hEditToken = gui_create_edit(hWnd, IDC_SERVER_EDIT_TOKEN,
        MARGIN + 85, y, SERVER_WINDOW_WIDTH - 2 * MARGIN - 101, EDIT_HEIGHT);
    SendMessage(ctx->hEditToken, EM_SETPASSWORDCHAR, L'*', 0);
    y += EDIT_HEIGHT + MARGIN;

    /* Device list label */
    ctx->hLabelDevices = gui_create_label(hWnd, IDC_SERVER_LABEL_DEV,
        L"USB Devices on This Computer:", MARGIN, y, 300, LABEL_HEIGHT);
    y += LABEL_HEIGHT + PADDING;

    /* Device ListView */
    ctx->base.hDeviceList = gui_create_listview(hWnd, IDC_SERVER_DEVICE_LIST,
        MARGIN, y, SERVER_WINDOW_WIDTH - 2 * MARGIN - 16, 120, LVS_SHOWSELALWAYS);
    create_device_columns(ctx->base.hDeviceList);
    y += 120 + MARGIN;

    /* Client list label */
    ctx->hLabelClients = gui_create_label(hWnd, IDC_SERVER_LABEL_CLI,
        L"Connected Clients:", MARGIN, y, 200, LABEL_HEIGHT);
    y += LABEL_HEIGHT + PADDING;

    /* Client ListView */
    ctx->base.hClientList = gui_create_listview(hWnd, IDC_SERVER_CLIENT_LIST,
        MARGIN, y, SERVER_WINDOW_WIDTH - 2 * MARGIN - 16, 80, 0);
    create_client_columns(ctx->base.hClientList);
    /* Disable checkboxes for client list */
    ListView_SetExtendedListViewStyle(ctx->base.hClientList,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    y += 80 + MARGIN;

    /* Buttons - only Refresh and Hide to Tray */
    int btnX = MARGIN;
    int btnY = SERVER_WINDOW_HEIGHT - BUTTON_HEIGHT - MARGIN - 40;

    ctx->hBtnRefresh = gui_create_button(hWnd, IDC_SERVER_BTN_REFRESH,
        L"Refresh", btnX, btnY, BUTTON_WIDTH, BUTTON_HEIGHT);
    btnX += BUTTON_WIDTH + PADDING;

    ctx->hBtnHide = gui_create_button(hWnd, IDC_SERVER_BTN_HIDE,
        L"Hide to Tray", btnX, btnY, BUTTON_WIDTH + 10, BUTTON_HEIGHT);

    /* Apply font to all controls */
    SendMessage(ctx->hLabelToken, WM_SETFONT, (WPARAM)ctx->base.hFont, TRUE);
    SendMessage(ctx->hEditToken, WM_SETFONT, (WPARAM)ctx->base.hFont, TRUE);
    SendMessage(ctx->hLabelDevices, WM_SETFONT, (WPARAM)ctx->base.hFont, TRUE);
    SendMessage(ctx->hLabelClients, WM_SETFONT, (WPARAM)ctx->base.hFont, TRUE);
    SendMessage(ctx->base.hDeviceList, WM_SETFONT, (WPARAM)ctx->base.hFont, TRUE);
    SendMessage(ctx->base.hClientList, WM_SETFONT, (WPARAM)ctx->base.hFont, TRUE);
    SendMessage(ctx->hBtnRefresh, WM_SETFONT, (WPARAM)ctx->base.hFont, TRUE);
    SendMessage(ctx->hBtnHide, WM_SETFONT, (WPARAM)ctx->base.hFont, TRUE);
}

/* Recompute control rectangles from the current client size.
 * Called on WM_CREATE and WM_SIZE so nothing overlaps or clips when the
 * window is resized or opened at a different DPI. */
static void gui_layout_server(server_gui_context_t *ctx) {
    HWND hWnd = ctx->base.hMainWnd;
    int cw, ch;
    gui_get_client_size(hWnd, &cw, &ch);

    /* Status bar auto-sizes; query its height. */
    RECT sbrc;
    GetWindowRect(ctx->base.hStatusBar, &sbrc);
    int status_h = sbrc.bottom - sbrc.top;
    int bottom = ch - status_h;
    int margin = gui_scale_dpi(MARGIN);
    int pad = gui_scale_dpi(PADDING);
    int lblH = gui_scale_dpi(LABEL_HEIGHT);
    int editH = gui_scale_dpi(EDIT_HEIGHT);
    int btnH = gui_scale_dpi(BUTTON_HEIGHT);
    int btnW = gui_scale_dpi(BUTTON_WIDTH);

    int y = margin;
    int list_w = cw - 2 * margin - gui_scale_dpi(16);

    /* Token row */
    gui_move(ctx->hLabelToken, margin, y + (editH - lblH) / 2,
             gui_scale_dpi(80), lblH, false);
    gui_move(ctx->hEditToken, margin + gui_scale_dpi(85), y,
             cw - 2 * margin - gui_scale_dpi(85), editH, false);
    y += editH + margin;

    /* Devices label */
    gui_move(ctx->hLabelDevices, margin, y, cw - 2 * margin, lblH, false);
    y += lblH + pad;

    /* Devices list: fixed height band */
    int dev_list_h = gui_scale_dpi(120);
    gui_move(ctx->base.hDeviceList, margin, y, list_w, dev_list_h, false);
    y += dev_list_h + margin;

    /* Clients label */
    gui_move(ctx->hLabelClients, margin, y, cw - 2 * margin, lblH, false);
    y += lblH + pad;

    /* Clients list fills until the button row. */
    int btn_row_h = btnH + pad;
    int cli_list_h = (bottom - pad) - btn_row_h - y;
    if (cli_list_h < gui_scale_dpi(60)) cli_list_h = gui_scale_dpi(60);
    gui_move(ctx->base.hClientList, margin, y, list_w, cli_list_h, false);

    /* Button row pinned above the status bar. */
    int btnY = bottom - pad - btnH;
    gui_move(ctx->hBtnRefresh, margin, btnY, btnW, btnH, false);
    gui_move(ctx->hBtnHide, margin + btnW + pad, btnY, btnW + gui_scale_dpi(10), btnH, false);

    /* Status bar spans the full width. */
    SendMessageW(ctx->base.hStatusBar, WM_SIZE, 0, 0);

    /* Let columns use the available width instead of fixed pixel sizes. */
    for (int i = 0; i < 4; i++) {
        ListView_SetColumnWidth(ctx->base.hDeviceList, i, LVSCW_AUTOSIZE_USEHEADER);
    }
    for (int i = 0; i < 3; i++) {
        ListView_SetColumnWidth(ctx->base.hClientList, i, LVSCW_AUTOSIZE_USEHEADER);
    }
}

static void create_device_columns(HWND hList) {
    LV_ADDCOLUMN(hList, 0, L"Bus ID", SRV_COL_BUSID);
    LV_ADDCOLUMN(hList, 1, L"VID:PID", SRV_COL_VIDPID);
    LV_ADDCOLUMN(hList, 2, L"Description", SRV_COL_DESC);
    LV_ADDCOLUMN(hList, 3, L"Status", SRV_COL_STATUS);
}

static void create_client_columns(HWND hList) {
    LV_ADDCOLUMN(hList, 0, L"IP Address", SRV_COL_IP);
    LV_ADDCOLUMN(hList, 1, L"Device", SRV_COL_DEVICE);
    LV_ADDCOLUMN(hList, 2, L"Connected", SRV_COL_TIME);
}

static void create_tray_menu(server_gui_context_t *ctx) {
    ctx->base.hTrayMenu = CreatePopupMenu();
    AppendMenuW(ctx->base.hTrayMenu, MF_STRING, IDM_TRAY_SHOW, L"Show Window");
    AppendMenuW(ctx->base.hTrayMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(ctx->base.hTrayMenu, MF_STRING, IDM_TRAY_EXIT, L"Exit");
}

/* ----- Device List Population ----- */

typedef struct enum_ctx {
    server_gui_context_t *gui_ctx;
    int index;
} enum_ctx_t;

static void enum_device_callback(const usb_device_t *device, void *user_data) {
    enum_ctx_t *ectx = (enum_ctx_t *)user_data;
    server_gui_context_t *ctx = ectx->gui_ctx;

    wchar_t wBusId[32], wVidPid[32], wDesc[128];
    gui_utf8_to_wide(device->busid, wBusId, 32);

    swprintf(wVidPid, 32, L"%04X:%04X", device->vendor_id, device->product_id);
    gui_utf8_to_wide(device->product[0] ? device->product : "USB Device", wDesc, 128);

    int row = gui_listview_add_item(ctx->base.hDeviceList, ectx->index, wBusId);
    gui_listview_set_item(ctx->base.hDeviceList, row, 1, wVidPid);
    gui_listview_set_item(ctx->base.hDeviceList, row, 2, wDesc);

    /* Check if device is shared */
    device_entry_t *dev = device_list_find(&ctx->device_list, device->busid);
    bool shared = (dev != NULL && dev->is_shared);
    gui_listview_set_item(ctx->base.hDeviceList, row, 3, shared ? L"Shared" : L"Local");
    gui_listview_set_checkbox(ctx->base.hDeviceList, row, shared);

    /* Add to device list if not already present */
    if (dev == NULL) {
        device_list_add(&ctx->device_list, device);
    }

    ectx->index++;
}

static void populate_device_list(server_gui_context_t *ctx) {
    /* Set guard to prevent checkbox events during list update */
    g_updating_checkbox = true;

    gui_listview_clear(ctx->base.hDeviceList);

    enum_ctx_t ectx = { .gui_ctx = ctx, .index = 0 };
    usb_enumerate_devices(enum_device_callback, &ectx);

    /* Update label */
    wchar_t label[64];
    swprintf(label, 64, L"USB Devices on This Computer (%d):", ectx.index);
    SetWindowTextW(ctx->hLabelDevices, label);

    g_updating_checkbox = false;
}

/* ----- Event Handlers ----- */

static void on_device_checkbox_changed(server_gui_context_t *ctx, int index) {
    /* Prevent recursive calls when we programmatically change checkbox */
    if (g_updating_checkbox) {
        return;
    }

    wchar_t busidW[32];
    char busid[32];

    gui_lv_get_item_text(ctx->base.hDeviceList, index, 0, busidW, 32);
    gui_wide_to_utf8(busidW, busid, 32);

    /* Skip if busid is empty (can happen during list refresh) */
    if (busid[0] == '\0') {
        return;
    }

    bool checked = gui_listview_get_checkbox(ctx->base.hDeviceList, index);

    device_list_set_shared(&ctx->device_list, busid, checked);
    gui_listview_set_item(ctx->base.hDeviceList, index, 3, checked ? L"Shared" : L"Local");

    LOG_INFO("Device %s: %s", busid, checked ? "Shared" : "Unshared");
}

static void on_refresh_clicked(server_gui_context_t *ctx) {
    populate_device_list(ctx);
}

static void on_hide_clicked(server_gui_context_t *ctx) {
    ShowWindow(ctx->base.hMainWnd, SW_HIDE);
    ctx->base.minimizedToTray = true;
}

/* ----- Client Handler Thread ----- */

static DWORD WINAPI client_handler_thread(LPVOID param) {
    client_handler_data_t *data = (client_handler_data_t *)param;
    client_connection_t *client = data->client;
    server_gui_context_t *ctx = data->gui_ctx;

    free(data);

    /* Handle client requests */
    while (client->running && ctx->server_running) {
        error_code_t err = client_handle_request(&ctx->client_manager, client);
        if (err != ERR_SUCCESS) {
            break;
        }
    }

    /* Remove client */
    client_manager_remove(&ctx->client_manager, client);
    PostMessage(ctx->base.hMainWnd, WM_CLIENT_UPDATE, 0, 0);

    return 0;
}

/* ----- Discovery Thread ----- */

static DWORD WINAPI discovery_listener_thread(LPVOID param) {
    server_gui_context_t *ctx = (server_gui_context_t *)param;
    char recv_buf[256];
    char src_ip[64];
    uint16_t src_port;
    char hostname[128];
    char response[256];

    /* Get hostname */
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "USB-Server", sizeof(hostname) - 1);
    }

    LOG_INFO("Discovery listener started on port %d", DISCOVERY_PORT);

    while (ctx->server_running) {
        ssize_t received = udp_recvfrom_timeout(ctx->discovery_socket,
            recv_buf, sizeof(recv_buf) - 1,
            src_ip, sizeof(src_ip), &src_port, 1000);

        if (received <= 0) {
            continue;
        }

        recv_buf[received] = '\0';

        /* Check for discovery request */
        if (strncmp(recv_buf, DISCOVERY_MAGIC, DISCOVERY_MAGIC_LEN) == 0) {
            LOG_DEBUG("Discovery request from %s:%u", src_ip, src_port);

            /* Count shared devices by iterating linked list */
            int shared_count = 0;
            device_list_lock(&ctx->device_list);
            device_entry_t *entry = ctx->device_list.head;
            while (entry) {
                if (entry->is_shared) {
                    shared_count++;
                }
                entry = entry->next;
            }
            device_list_unlock(&ctx->device_list);

            /* Build response: "USBIP_SERVER hostname device_count" */
            snprintf(response, sizeof(response), "%s %s %d",
                SERVER_RESPONSE_MAGIC, hostname, shared_count);

            /* Send response */
            udp_sendto(ctx->discovery_socket, response, strlen(response),
                src_ip, src_port);

            LOG_DEBUG("Sent discovery response to %s:%u", src_ip, src_port);
        }
    }

    LOG_INFO("Discovery listener stopped");
    return 0;
}

/* ----- Server Thread ----- */

static DWORD WINAPI server_accept_thread(LPVOID param) {
    server_gui_context_t *ctx = (server_gui_context_t *)param;

    while (ctx->server_running) {
        char client_ip[64] = {0};
        uint16_t client_port = 0;
        socket_t client_sock = tcp_server_accept_timeout(ctx->server_socket,
            client_ip, sizeof(client_ip), &client_port, 1000);

        if (socket_is_valid(client_sock)) {
            LOG_INFO("Client connected: %s:%u", client_ip, client_port);

            /* Authenticate before any USB/IP traffic (no-op if token empty). */
            if (auth_is_enabled(ctx->auth_token)) {
                error_code_t auth_err = auth_server_handshake(client_sock, ctx->auth_token);
                if (auth_err != ERR_SUCCESS) {
                    LOG_WARN("Rejecting %s:%u - auth failed (%s)",
                        client_ip, client_port, error_string(auth_err));
                    socket_close(client_sock);
                    continue;
                }
            }

            /* Add to client manager */
            client_connection_t *conn = NULL;
            error_code_t err = client_manager_add(&ctx->client_manager,
                client_sock, client_ip, client_port, &conn);

            if (err == ERR_SUCCESS && conn) {
                /* Start handler thread for this client */
                client_handler_data_t *data = (client_handler_data_t *)malloc(sizeof(client_handler_data_t));
                if (data) {
                    data->gui_ctx = ctx;
                    data->client = conn;
                    conn->running = true;
                    conn->recv_thread = CreateThread(NULL, 0, client_handler_thread, data, 0, NULL);
                }
                PostMessage(ctx->base.hMainWnd, WM_CLIENT_UPDATE, 0, 0);
            } else {
                socket_close(client_sock);
            }
        }
    }

    return 0;
}

/* ----- Public Functions ----- */

bool server_gui_init(server_gui_context_t *ctx, HINSTANCE hInstance) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->base.hInstance = hInstance;
    ctx->base.running = true;
    g_server_ctx = ctx;

    /* Initialize common controls */
    gui_init_common_controls();

    /* Create font */
    ctx->base.hFont = gui_create_font(GUI_FONT_SIZE, GUI_FONT_NAME);

    /* Register window class */
    if (!register_server_class(hInstance)) {
        LOG_ERROR("Failed to register server window class");
        return false;
    }

    /* Create window */
    if (!create_server_window(ctx)) {
        LOG_ERROR("Failed to create server window");
        return false;
    }

    /* Create controls */
    create_server_controls(ctx);

    /* Lay out controls for the initial client size (also redone on WM_SIZE). */
    gui_layout_server(ctx);

    /* Create tray menu */
    create_tray_menu(ctx);

    /* Add tray icon */
    gui_tray_add(&ctx->base, 1, L"USB Over Network - Server\nCTK Technologies");

    /* Initialize device list */
    device_list_init(&ctx->device_list);

    /* Initialize client manager */
    client_manager_init(&ctx->client_manager, &ctx->device_list);

    /* Start server */
    server_gui_start_server(ctx);

    /* Populate device list */
    populate_device_list(ctx);

    /* Set refresh timer */
    SetTimer(ctx->base.hMainWnd, IDT_REFRESH_TIMER, REFRESH_INTERVAL_MS, NULL);

    return true;
}

int server_gui_run(server_gui_context_t *ctx) {
    ShowWindow(ctx->base.hMainWnd, SW_SHOW);
    UpdateWindow(ctx->base.hMainWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

void server_gui_cleanup(server_gui_context_t *ctx) {
    /* Stop server */
    server_gui_stop_server(ctx);

    /* Remove tray icon */
    gui_tray_remove(&ctx->base);

    /* Cleanup managers */
    client_manager_cleanup(&ctx->client_manager);
    device_list_cleanup(&ctx->device_list);

    /* Cleanup menu */
    if (ctx->base.hTrayMenu) {
        DestroyMenu(ctx->base.hTrayMenu);
    }

    /* Delete font */
    if (ctx->base.hFont) {
        DeleteObject(ctx->base.hFont);
    }
}

void server_gui_refresh_devices(server_gui_context_t *ctx) {
    populate_device_list(ctx);
}

void server_gui_update_clients(server_gui_context_t *ctx) {
    gui_listview_clear(ctx->base.hClientList);

    int index = 0;
    client_connection_t *conn = ctx->client_manager.clients;

    while (conn) {
        wchar_t ipW[64], busidW[32], timeW[32];
        gui_utf8_to_wide(conn->ip_address, ipW, 64);
        gui_utf8_to_wide(conn->attached_busid[0] ? conn->attached_busid : "-", busidW, 32);

        /* Calculate connected time (connect_time is from GetTickCount64) */
        uint64_t now = GetTickCount64();
        uint64_t elapsed_ms = (now > conn->connect_time) ? (now - conn->connect_time) : 0;
        DWORD elapsed_sec = (DWORD)(elapsed_ms / 1000);
        if (elapsed_sec < 60) {
            swprintf(timeW, 32, L"%lu sec", (unsigned long)elapsed_sec);
        } else {
            swprintf(timeW, 32, L"%lu min", (unsigned long)(elapsed_sec / 60));
        }

        int row = gui_listview_add_item(ctx->base.hClientList, index, ipW);
        gui_listview_set_item(ctx->base.hClientList, row, 1, busidW);
        gui_listview_set_item(ctx->base.hClientList, row, 2, timeW);

        index++;
        conn = conn->next;
    }

    /* Update label */
    wchar_t label[64];
    swprintf(label, 64, L"Connected Clients (%d):", index);
    SetWindowTextW(ctx->hLabelClients, label);
}

void server_gui_toggle_share(server_gui_context_t *ctx, int index, bool share) {
    gui_listview_set_checkbox(ctx->base.hDeviceList, index, share);
    on_device_checkbox_changed(ctx, index);
}

void server_gui_stop_all(server_gui_context_t *ctx) {
    int count = ListView_GetItemCount(ctx->base.hDeviceList);
    for (int i = 0; i < count; i++) {
        wchar_t busidW[32];
        char busid[32];
        gui_lv_get_item_text(ctx->base.hDeviceList, i, 0, busidW, 32);
        gui_wide_to_utf8(busidW, busid, 32);

        device_list_set_shared(&ctx->device_list, busid, false);
        gui_listview_set_checkbox(ctx->base.hDeviceList, i, false);
        gui_listview_set_item(ctx->base.hDeviceList, i, 3, L"Local");
    }
    LOG_INFO("Stopped sharing all devices");
}

bool server_gui_start_server(server_gui_context_t *ctx) {
    /* Read the auth token from the GUI field (empty = auth disabled). */
    char token[AUTH_TOKEN_MAX_LEN] = {0};
    GetWindowTextA(ctx->hEditToken, token, sizeof(token));
    str_copy(ctx->auth_token, token, sizeof(ctx->auth_token));

    /* Create TCP server socket for USB/IP */
    ctx->server_socket = tcp_server_create(NULL, USBIP_PORT);
    if (!socket_is_valid(ctx->server_socket)) {
        LOG_ERROR("Failed to create server socket on port %d", USBIP_PORT);
        return false;
    }

    /* Create UDP socket for discovery */
    ctx->discovery_socket = udp_socket_create();
    if (socket_is_valid(ctx->discovery_socket)) {
        if (udp_socket_bind(ctx->discovery_socket, "0.0.0.0", DISCOVERY_PORT) != ERR_SUCCESS) {
            LOG_WARN("Failed to bind discovery socket to port %d", DISCOVERY_PORT);
            socket_close(ctx->discovery_socket);
            ctx->discovery_socket = INVALID_SOCKET_VAL;
        }
    }

    ctx->server_running = true;

    /* Start TCP accept thread */
    ctx->server_thread = CreateThread(NULL, 0, server_accept_thread, ctx, 0, NULL);
    if (!ctx->server_thread) {
        socket_close(ctx->server_socket);
        if (socket_is_valid(ctx->discovery_socket)) {
            socket_close(ctx->discovery_socket);
        }
        ctx->server_running = false;
        return false;
    }

    /* Start UDP discovery thread */
    if (socket_is_valid(ctx->discovery_socket)) {
        ctx->discovery_thread = CreateThread(NULL, 0, discovery_listener_thread, ctx, 0, NULL);
    }

    LOG_INFO("Server started on port %d (discovery on %d)", USBIP_PORT, DISCOVERY_PORT);
    return true;
}

void server_gui_stop_server(server_gui_context_t *ctx) {
    ctx->server_running = false;

    /* Close sockets */
    if (socket_is_valid(ctx->server_socket)) {
        socket_close(ctx->server_socket);
        ctx->server_socket = INVALID_SOCKET_VAL;
    }

    if (socket_is_valid(ctx->discovery_socket)) {
        socket_close(ctx->discovery_socket);
        ctx->discovery_socket = INVALID_SOCKET_VAL;
    }

    /* Wait for threads */
    if (ctx->server_thread) {
        WaitForSingleObject(ctx->server_thread, 3000);
        CloseHandle(ctx->server_thread);
        ctx->server_thread = NULL;
    }

    if (ctx->discovery_thread) {
        WaitForSingleObject(ctx->discovery_thread, 3000);
        CloseHandle(ctx->discovery_thread);
        ctx->discovery_thread = NULL;
    }

    LOG_INFO("Server stopped");
}

/* ----- Window Procedure ----- */

static LRESULT CALLBACK ServerWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    server_gui_context_t *ctx = g_server_ctx;

    switch (msg) {
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_SERVER_BTN_REFRESH:
                    on_refresh_clicked(ctx);
                    break;
                case IDC_SERVER_BTN_HIDE:
                    on_hide_clicked(ctx);
                    break;
                case IDM_TRAY_SHOW:
                    ShowWindow(hWnd, SW_SHOW);
                    SetForegroundWindow(hWnd);
                    ctx->base.minimizedToTray = false;
                    break;
                case IDM_TRAY_EXIT:
                    PostQuitMessage(0);
                    break;
            }
            break;

        case WM_NOTIFY: {
            NMHDR *nmhdr = (NMHDR *)lParam;
            if (nmhdr->idFrom == IDC_SERVER_DEVICE_LIST && nmhdr->code == LVN_ITEMCHANGED) {
                NMLISTVIEW *nmlv = (NMLISTVIEW *)lParam;
                if ((nmlv->uChanged & LVIF_STATE) &&
                    ((nmlv->uNewState ^ nmlv->uOldState) & LVIS_STATEIMAGEMASK)) {
                    on_device_checkbox_changed(ctx, nmlv->iItem);
                }
            }
            break;
        }

        case WM_TIMER:
            if (wParam == IDT_REFRESH_TIMER) {
                server_gui_update_clients(ctx);
            }
            break;

        case WM_SIZE:
            /* Keep controls in sync with the window: listviews fill, buttons
             * dock above the status bar. Minimizing sends SIZE_MINIMIZED;
             * skip the work then. */
            if (wParam != SIZE_MINIMIZED) {
                gui_layout_server(ctx);
            }
            break;

        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP) {
                POINT pt;
                GetCursorPos(&pt);
                gui_tray_show_menu(&ctx->base, pt);
            } else if (lParam == WM_LBUTTONDBLCLK) {
                ShowWindow(hWnd, SW_SHOW);
                SetForegroundWindow(hWnd);
                ctx->base.minimizedToTray = false;
            }
            break;

        case WM_CLIENT_UPDATE:
            server_gui_update_clients(ctx);
            break;

        case WM_CLOSE:
            if (gui_show_confirm(hWnd, L"Stop server and exit?") == IDYES) {
                DestroyWindow(hWnd);
            }
            return 0;

        case WM_DESTROY:
            KillTimer(hWnd, IDT_REFRESH_TIMER);
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    return 0;
}

/* ----- Entry Point ----- */

int server_gui_main(HINSTANCE hInstance, int nCmdShow) {
    (void)nCmdShow;

    /* Initialize logging */
    log_config_t log_cfg = {
        .level = LOG_LEVEL_INFO,
        .targets = LOG_TARGET_CONSOLE,
        .show_timestamp = true,
        .show_level = true,
        .use_colors = true
    };
    log_init(&log_cfg);

    /* Initialize network */
    network_init();

    /* Initialize USB host subsystem */
    if (usb_host_init() != ERR_SUCCESS) {
        gui_show_error(NULL, L"Failed to initialize USB host subsystem");
        return 1;
    }

    /* Initialize and run GUI */
    server_gui_context_t ctx;
    if (!server_gui_init(&ctx, hInstance)) {
        gui_show_error(NULL, L"Failed to initialize server GUI");
        usb_host_cleanup();
        return 1;
    }

    int result = server_gui_run(&ctx);

    server_gui_cleanup(&ctx);
    usb_host_cleanup();
    network_cleanup();
    log_cleanup();

    return result;
}

/* WinMain for GUI application */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    return server_gui_main(hInstance, nCmdShow);
}
