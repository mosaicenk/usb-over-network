/*
 * USB Over Network - Client GUI Implementation
 * Windows GUI for USB/IP client
 */

#include "client_gui.h"
#include "../common/log.h"
#include "../common/network.h"
#include "../common/config.h"
#include "../common/protocol.h"
#include "../common/usb_defs.h"
#include "../common/auth.h"
#include "../common/string_utils.h"
#include <stdio.h>

/* Global context pointer for window procedure */
static client_gui_context_t *g_client_ctx = NULL;

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
static LRESULT CALLBACK ClientWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void create_device_columns(HWND hList);
static void gui_layout_client(client_gui_context_t *ctx);
static void populate_device_list(client_gui_context_t *ctx);
static DWORD WINAPI discovery_thread_func(LPVOID param);

/* ----- Window Creation ----- */

static bool register_client_class(HINSTANCE hInstance) {
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = ClientWndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    if (!wc.hIcon) wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = CLIENT_CLASS_NAME;
    wc.hIconSm = wc.hIcon;

    return RegisterClassExW(&wc) != 0;
}

static bool create_client_window(client_gui_context_t *ctx) {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - CLIENT_WINDOW_WIDTH) / 2;
    int y = (screenH - CLIENT_WINDOW_HEIGHT) / 2;

    ctx->base.hMainWnd = CreateWindowExW(
        0, CLIENT_CLASS_NAME, L"USB Over Network Client - CTK Technologies",
        WS_OVERLAPPEDWINDOW,
        x, y, CLIENT_WINDOW_WIDTH, CLIENT_WINDOW_HEIGHT,
        NULL, NULL, ctx->base.hInstance, NULL
    );

    return ctx->base.hMainWnd != NULL;
}

static void create_client_controls(client_gui_context_t *ctx) {
    HWND hWnd = ctx->base.hMainWnd;
    int y = MARGIN;
    int editWidth = 200;
    int btnSmall = 80;

    /* Server row */
    ctx->hLabelServer = gui_create_label(hWnd, IDC_CLIENT_LABEL_SERVER,
        L"Server:", MARGIN, y + 4, 50, LABEL_HEIGHT);

    ctx->hEditServer = gui_create_edit(hWnd, IDC_CLIENT_EDIT_SERVER,
        MARGIN + 55, y, editWidth, EDIT_HEIGHT);
    SetWindowTextW(ctx->hEditServer, L"192.168.1.100");

    int btnX = MARGIN + 55 + editWidth + PADDING;
    ctx->hBtnDiscover = gui_create_button(hWnd, IDC_CLIENT_BTN_DISCOVER,
        L"Discover", btnX, y, btnSmall, BUTTON_HEIGHT);
    btnX += btnSmall + PADDING;

    ctx->hBtnConnect = gui_create_button(hWnd, IDC_CLIENT_BTN_CONNECT,
        L"Connect", btnX, y, btnSmall, BUTTON_HEIGHT);

    y += BUTTON_HEIGHT + MARGIN;

    /* Token row (optional preshared auth; leave blank for no-auth servers) */
    ctx->hLabelToken = gui_create_label(hWnd, IDC_CLIENT_LABEL_TOKEN,
        L"Token:", MARGIN, y + 4, 50, LABEL_HEIGHT);
    ctx->hEditToken = gui_create_edit(hWnd, IDC_CLIENT_EDIT_TOKEN,
        MARGIN + 55, y, editWidth, EDIT_HEIGHT);
    SendMessage(ctx->hEditToken, EM_SETPASSWORDCHAR, L'*', 0);

    y += EDIT_HEIGHT + MARGIN;

    /* Device list label */
    ctx->hLabelDevices = gui_create_label(hWnd, 0,
        L"Remote Devices:", MARGIN, y, 200, LABEL_HEIGHT);
    y += LABEL_HEIGHT + PADDING;

    /* Device ListView with checkboxes */
    ctx->base.hDeviceList = gui_create_listview(hWnd, IDC_CLIENT_DEVICE_LIST,
        MARGIN, y, CLIENT_WINDOW_WIDTH - 2 * MARGIN - 16, 140, LVS_SHOWSELALWAYS);
    create_device_columns(ctx->base.hDeviceList);
    /* Enable checkboxes for attach/detach */
    ListView_SetExtendedListViewStyle(ctx->base.hDeviceList,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES);
    y += 140 + MARGIN;

    /* Bottom buttons - only Refresh */
    btnX = MARGIN;
    ctx->hBtnRefresh = gui_create_button(hWnd, IDC_CLIENT_BTN_REFRESH,
        L"Refresh", btnX, y, BUTTON_WIDTH, BUTTON_HEIGHT);

    /* Status bar */
    ctx->base.hStatusBar = gui_create_statusbar(hWnd, IDC_CLIENT_STATUSBAR);
    gui_statusbar_set_text(ctx->base.hStatusBar, L"Not connected");

    /* Apply font */
    SendMessage(ctx->hLabelServer, WM_SETFONT, (WPARAM)ctx->base.hFont, TRUE);
    SendMessage(ctx->hEditServer, WM_SETFONT, (WPARAM)ctx->base.hFont, TRUE);
    SendMessage(ctx->hLabelToken, WM_SETFONT, (WPARAM)ctx->base.hFont, TRUE);
    SendMessage(ctx->hEditToken, WM_SETFONT, (WPARAM)ctx->base.hFont, TRUE);
    SendMessage(ctx->hBtnDiscover, WM_SETFONT, (WPARAM)ctx->base.hFont, TRUE);
    SendMessage(ctx->hBtnConnect, WM_SETFONT, (WPARAM)ctx->base.hFont, TRUE);
    SendMessage(ctx->hLabelDevices, WM_SETFONT, (WPARAM)ctx->base.hFont, TRUE);
    SendMessage(ctx->base.hDeviceList, WM_SETFONT, (WPARAM)ctx->base.hFont, TRUE);
    SendMessage(ctx->hBtnRefresh, WM_SETFONT, (WPARAM)ctx->base.hFont, TRUE);

    /* Disable buttons initially */
    EnableWindow(ctx->hBtnRefresh, FALSE);
}

/* Recompute control rectangles from the current client size.
 * Called on init and WM_SIZE so controls never overlap or clip. */
static void gui_layout_client(client_gui_context_t *ctx) {
    HWND hWnd = ctx->base.hMainWnd;
    int cw, ch;
    gui_get_client_size(hWnd, &cw, &ch);

    RECT sbrc;
    GetWindowRect(ctx->base.hStatusBar, &sbrc);
    int status_h = sbrc.bottom - sbrc.top;
    int bottom = ch - status_h;
    int margin = gui_scale_dpi(MARGIN);
    int pad = gui_scale_dpi(PADDING);
    int lblH = gui_scale_dpi(LABEL_HEIGHT);
    int editH = gui_scale_dpi(EDIT_HEIGHT);
    int btnH = gui_scale_dpi(BUTTON_HEIGHT);
    int btnSmall = gui_scale_dpi(80);
    int editW = gui_scale_dpi(200);
    int labelW = gui_scale_dpi(50);

    int y = margin;
    int row1_x = margin + labelW + pad;
    int list_w = cw - 2 * margin - gui_scale_dpi(16);

    /* Server row: label + edit + Discover + Connect */
    gui_move(ctx->hLabelServer, margin, y + (editH - lblH) / 2, labelW, lblH, false);
    gui_move(ctx->hEditServer, row1_x, y, editW, editH, false);
    int bx = row1_x + editW + pad;
    gui_move(ctx->hBtnDiscover, bx, y, btnSmall, btnH, false);
    bx += btnSmall + pad;
    gui_move(ctx->hBtnConnect, bx, y, btnSmall, btnH, false);
    y += editH + margin;

    /* Token row */
    gui_move(ctx->hLabelToken, margin, y + (editH - lblH) / 2, labelW, lblH, false);
    gui_move(ctx->hEditToken, row1_x, y, editW, editH, false);
    y += editH + margin;

    /* Devices label */
    gui_move(ctx->hLabelDevices, margin, y, cw - 2 * margin, lblH, false);
    y += lblH + pad;

    /* Device list fills down to the Refresh button. */
    int btnY = bottom - pad - btnH;
    int list_h = btnY - pad - y;
    if (list_h < gui_scale_dpi(80)) list_h = gui_scale_dpi(80);
    gui_move(ctx->base.hDeviceList, margin, y, list_w, list_h, false);

    /* Refresh button docked above the status bar. */
    gui_move(ctx->hBtnRefresh, margin, btnY, gui_scale_dpi(BUTTON_WIDTH), btnH, false);

    /* Status bar spans the full width. */
    SendMessageW(ctx->base.hStatusBar, WM_SIZE, 0, 0);

    /* Columns use the header width so nothing is truncated. */
    for (int i = 0; i < 4; i++) {
        ListView_SetColumnWidth(ctx->base.hDeviceList, i, LVSCW_AUTOSIZE_USEHEADER);
    }
}

static void create_device_columns(HWND hList) {
    LV_ADDCOLUMN(hList, 0, L"Bus ID", CLI_COL_BUSID);
    LV_ADDCOLUMN(hList, 1, L"VID:PID", CLI_COL_VIDPID);
    LV_ADDCOLUMN(hList, 2, L"Description", CLI_COL_DESC);
    LV_ADDCOLUMN(hList, 3, L"Status", CLI_COL_STATUS);
}

/* ----- Device List ----- */

static void populate_device_list(client_gui_context_t *ctx) {
    /* Set guard to prevent checkbox events during entire list update */
    g_updating_checkbox = true;

    gui_listview_clear(ctx->base.hDeviceList);

    if (!ctx->connected) {
        g_updating_checkbox = false;
        return;
    }

    /* Get device list from server */
    usbip_usb_device_t devices[MAX_DEVICES];
    int device_count = 0;

    error_code_t err = remote_server_list_devices(
        ctx->current_server, ctx->current_port,
        devices, &device_count, MAX_DEVICES);

    if (err != ERR_SUCCESS) {
        g_updating_checkbox = false;
        gui_show_error(ctx->base.hMainWnd, L"Failed to get device list from server");
        return;
    }

    for (int i = 0; i < device_count; i++) {
        wchar_t busidW[32], vidpidW[32], descW[128], statusW[32];

        gui_utf8_to_wide(devices[i].busid, busidW, 32);
        swprintf(vidpidW, 32, L"%04X:%04X", devices[i].idVendor, devices[i].idProduct);

        /* Description from path or class */
        if (devices[i].path[0]) {
            gui_utf8_to_wide(devices[i].path, descW, 128);
        } else {
            const char *classStr = usb_class_string(devices[i].bDeviceClass);
            gui_utf8_to_wide(classStr, descW, 128);
        }

        /* Check if attached */
        remote_device_t *attached = remote_device_find_by_busid(&ctx->device_list, devices[i].busid);
        wcscpy(statusW, attached ? L"Attached" : L"Available");

        int row = gui_listview_add_item(ctx->base.hDeviceList, i, busidW);
        gui_listview_set_item(ctx->base.hDeviceList, row, 1, vidpidW);
        gui_listview_set_item(ctx->base.hDeviceList, row, 2, descW);
        gui_listview_set_item(ctx->base.hDeviceList, row, 3, statusW);

        /* Set checkbox for attached devices */
        if (attached) {
            gui_listview_set_checkbox(ctx->base.hDeviceList, row, true);
        }
    }

    /* Update label */
    wchar_t label[64];
    swprintf(label, 64, L"Remote Devices (%d):", device_count);
    SetWindowTextW(ctx->hLabelDevices, label);

    g_updating_checkbox = false;
}

/* ----- Discovery ----- */

static DWORD WINAPI discovery_thread_func(LPVOID param) {
    client_gui_context_t *ctx = (client_gui_context_t *)param;

    error_code_t err = discovery_broadcast(&ctx->discovery_result, DISCOVERY_TIMEOUT_MS);
    (void)err;

    PostMessage(ctx->base.hMainWnd, WM_DISCOVERY_DONE, 0, 0);
    return 0;
}

static void show_discovery_results(client_gui_context_t *ctx) {
    if (ctx->discovery_result.server_count == 0) {
        gui_show_info(ctx->base.hMainWnd, L"No servers found on LAN.\nTry entering IP address manually.");
        return;
    }

    /* If only one server, use it directly */
    if (ctx->discovery_result.server_count == 1) {
        SetWindowTextA(ctx->hEditServer, ctx->discovery_result.servers[0].ip_address);
        gui_show_info(ctx->base.hMainWnd, L"Found 1 server. Address filled in.");
        return;
    }

    /* Multiple servers - show selection dialog */
    wchar_t msg[512] = L"Found servers:\n\n";
    for (int i = 0; i < ctx->discovery_result.server_count; i++) {
        wchar_t line[64];
        wchar_t ipW[32];
        gui_utf8_to_wide(ctx->discovery_result.servers[i].ip_address, ipW, 32);
        swprintf(line, 64, L"%d. %s\n", i + 1, ipW);
        wcscat(msg, line);
    }
    wcscat(msg, L"\nUsing first server.");

    SetWindowTextA(ctx->hEditServer, ctx->discovery_result.servers[0].ip_address);
    gui_show_info(ctx->base.hMainWnd, msg);
}

/* ----- Event Handlers ----- */

static void on_discover_clicked(client_gui_context_t *ctx) {
    gui_statusbar_set_text(ctx->base.hStatusBar, L"Discovering servers...");
    EnableWindow(ctx->hBtnDiscover, FALSE);

    memset(&ctx->discovery_result, 0, sizeof(ctx->discovery_result));
    ctx->discovery_thread = CreateThread(NULL, 0, discovery_thread_func, ctx, 0, NULL);
}

static void on_connect_clicked(client_gui_context_t *ctx) {
    if (ctx->connected) {
        client_gui_disconnect(ctx);
    } else {
        char server[64];
        GetWindowTextA(ctx->hEditServer, server, sizeof(server));
        if (server[0] == '\0') {
            gui_show_error(ctx->base.hMainWnd, L"Please enter server IP address");
            return;
        }
        /* Read optional auth token and forward to the remote_device module. */
        char token[AUTH_TOKEN_MAX_LEN];
        GetWindowTextA(ctx->hEditToken, token, sizeof(token));
        remote_device_set_token(token[0] ? token : NULL);
        client_gui_set_server(ctx, server);
        client_gui_connect(ctx);
    }
}

static void on_refresh_clicked(client_gui_context_t *ctx) {
    client_gui_refresh_devices(ctx);
}

/* Handle checkbox toggle - attach when checked, detach when unchecked */
static void on_device_checkbox_changed(client_gui_context_t *ctx, int index) {
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

    if (checked) {
        /* Attach device */
        gui_statusbar_set_text(ctx->base.hStatusBar, L"Attaching device...");
        error_code_t err = client_gui_attach_device(ctx, busid);
        if (err == ERR_SUCCESS) {
            gui_listview_set_item(ctx->base.hDeviceList, index, 3, L"Attached");
            gui_statusbar_set_text(ctx->base.hStatusBar, L"Device attached");
            LOG_INFO("Attached device: %s", busid);
        } else {
            /* Uncheck on failure - use guard to prevent recursion */
            g_updating_checkbox = true;
            gui_listview_set_checkbox(ctx->base.hDeviceList, index, false);
            g_updating_checkbox = false;
            gui_statusbar_set_text(ctx->base.hStatusBar, L"Failed to attach device");
            gui_show_error(ctx->base.hMainWnd, L"Failed to attach device");
        }
    } else {
        /* Detach device */
        gui_statusbar_set_text(ctx->base.hStatusBar, L"Detaching device...");
        error_code_t err = client_gui_detach_device(ctx, busid);
        if (err == ERR_SUCCESS) {
            gui_listview_set_item(ctx->base.hDeviceList, index, 3, L"Available");
            gui_statusbar_set_text(ctx->base.hStatusBar, L"Device detached");
            LOG_INFO("Detached device: %s", busid);
        } else {
            /* Re-check on failure - use guard to prevent recursion */
            g_updating_checkbox = true;
            gui_listview_set_checkbox(ctx->base.hDeviceList, index, true);
            g_updating_checkbox = false;
            gui_statusbar_set_text(ctx->base.hStatusBar, L"Failed to detach device");
        }
    }
}

/* ----- Public Functions ----- */

bool client_gui_init(client_gui_context_t *ctx, HINSTANCE hInstance) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->base.hInstance = hInstance;
    ctx->base.running = true;
    ctx->current_port = USBIP_PORT;
    g_client_ctx = ctx;

    /* Initialize common controls */
    gui_init_common_controls();

    /* Create font */
    ctx->base.hFont = gui_create_font(GUI_FONT_SIZE, GUI_FONT_NAME);

    /* Register window class */
    if (!register_client_class(hInstance)) {
        LOG_ERROR("Failed to register client window class");
        return false;
    }

    /* Create window */
    if (!create_client_window(ctx)) {
        LOG_ERROR("Failed to create client window");
        return false;
    }

    /* Create controls */
    create_client_controls(ctx);

    /* Lay out controls for the initial client size (also redone on WM_SIZE). */
    gui_layout_client(ctx);

    /* Initialize VHCI */
    error_code_t err = vhci_init(&ctx->vhci);
    if (err != ERR_SUCCESS) {
        LOG_WARN("VHCI not available: %s", error_string(err));
    }

    /* Initialize device list */
    remote_device_list_init(&ctx->device_list);

    return true;
}

int client_gui_run(client_gui_context_t *ctx) {
    ShowWindow(ctx->base.hMainWnd, SW_SHOW);
    UpdateWindow(ctx->base.hMainWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

void client_gui_cleanup(client_gui_context_t *ctx) {
    /* Disconnect all */
    remote_device_disconnect_all(&ctx->device_list);
    remote_device_list_cleanup(&ctx->device_list);

    /* Cleanup VHCI */
    vhci_cleanup(&ctx->vhci);

    /* Wait for discovery thread */
    if (ctx->discovery_thread) {
        WaitForSingleObject(ctx->discovery_thread, 1000);
        CloseHandle(ctx->discovery_thread);
    }

    /* Delete font */
    if (ctx->base.hFont) {
        DeleteObject(ctx->base.hFont);
    }
}

void client_gui_set_server(client_gui_context_t *ctx, const char *ip) {
    str_copy(ctx->current_server, ip, sizeof(ctx->current_server));
}

bool client_gui_connect(client_gui_context_t *ctx) {
    wchar_t status[128];
    wchar_t ipW[64];
    gui_utf8_to_wide(ctx->current_server, ipW, 64);

    swprintf(status, 128, L"Connecting to %s...", ipW);
    gui_statusbar_set_text(ctx->base.hStatusBar, status);

    /* Test connection by getting device list */
    usbip_usb_device_t devices[1];
    int count = 0;
    error_code_t err = remote_server_list_devices(
        ctx->current_server, ctx->current_port,
        devices, &count, 1);

    if (err != ERR_SUCCESS) {
        gui_statusbar_set_text(ctx->base.hStatusBar, L"Connection failed");
        gui_show_error(ctx->base.hMainWnd, L"Failed to connect to server");
        return false;
    }

    ctx->connected = true;

    /* Update UI */
    SetWindowTextW(ctx->hBtnConnect, L"Disconnect");
    EnableWindow(ctx->hBtnRefresh, TRUE);

    swprintf(status, 128, L"Connected to %s", ipW);
    gui_statusbar_set_text(ctx->base.hStatusBar, status);

    /* Refresh device list */
    client_gui_refresh_devices(ctx);

    LOG_INFO("Connected to server %s", ctx->current_server);
    return true;
}

void client_gui_disconnect(client_gui_context_t *ctx) {
    /* Detach all devices */
    remote_device_disconnect_all(&ctx->device_list);

    ctx->connected = false;

    /* Update UI */
    SetWindowTextW(ctx->hBtnConnect, L"Connect");
    EnableWindow(ctx->hBtnRefresh, FALSE);

    gui_listview_clear(ctx->base.hDeviceList);
    gui_statusbar_set_text(ctx->base.hStatusBar, L"Disconnected");

    LOG_INFO("Disconnected from server");
}

void client_gui_refresh_devices(client_gui_context_t *ctx) {
    populate_device_list(ctx);
}

bool client_gui_attach_selected(client_gui_context_t *ctx) {
    int sel = gui_listview_get_selected(ctx->base.hDeviceList);
    if (sel < 0) {
        gui_show_error(ctx->base.hMainWnd, L"Please select a device to attach");
        return false;
    }

    wchar_t busidW[32];
    char busid[32];
    gui_lv_get_item_text(ctx->base.hDeviceList, sel, 0, busidW, 32);
    gui_wide_to_utf8(busidW, busid, 32);

    /* Check if already attached */
    if (remote_device_find_by_busid(&ctx->device_list, busid)) {
        gui_show_error(ctx->base.hMainWnd, L"Device is already attached");
        return false;
    }

    gui_statusbar_set_text(ctx->base.hStatusBar, L"Attaching device...");

    remote_device_t *device = NULL;
    error_code_t err = remote_device_connect(
        &ctx->device_list, &ctx->vhci,
        ctx->current_server, ctx->current_port,
        busid, &device);

    if (err != ERR_SUCCESS) {
        wchar_t msg[128];
        wchar_t errW[64];
        gui_utf8_to_wide(error_string(err), errW, 64);
        swprintf(msg, 128, L"Failed to attach device: %s", errW);
        gui_show_error(ctx->base.hMainWnd, msg);
        gui_statusbar_set_text(ctx->base.hStatusBar, L"Attach failed");
        return false;
    }

    /* Refresh list */
    client_gui_refresh_devices(ctx);

    wchar_t status[64];
    swprintf(status, 64, L"Device %s attached to port %d", busidW, device->vhci_port);
    gui_statusbar_set_text(ctx->base.hStatusBar, status);

    LOG_INFO("Attached device %s", busid);
    return true;
}

void client_gui_detach_selected(client_gui_context_t *ctx) {
    int sel = gui_listview_get_selected(ctx->base.hDeviceList);
    if (sel < 0) {
        gui_show_error(ctx->base.hMainWnd, L"Please select a device to detach");
        return;
    }

    wchar_t busidW[32];
    char busid[32];
    gui_lv_get_item_text(ctx->base.hDeviceList, sel, 0, busidW, 32);
    gui_wide_to_utf8(busidW, busid, 32);

    remote_device_t *device = remote_device_find_by_busid(&ctx->device_list, busid);
    if (!device) {
        gui_show_error(ctx->base.hMainWnd, L"Device is not attached");
        return;
    }

    remote_device_disconnect(&ctx->device_list, device);

    /* Refresh list */
    client_gui_refresh_devices(ctx);
    gui_statusbar_set_text(ctx->base.hStatusBar, L"Device detached");

    LOG_INFO("Detached device %s", busid);
}

/* Attach device by busid (for checkbox control) */
error_code_t client_gui_attach_device(client_gui_context_t *ctx, const char *busid) {
    /* Check if already attached */
    if (remote_device_find_by_busid(&ctx->device_list, busid)) {
        return ERR_ALREADY_EXISTS;
    }

    remote_device_t *device = NULL;
    error_code_t err = remote_device_connect(
        &ctx->device_list, &ctx->vhci,
        ctx->current_server, ctx->current_port,
        busid, &device);

    return err;
}

/* Detach device by busid (for checkbox control) */
error_code_t client_gui_detach_device(client_gui_context_t *ctx, const char *busid) {
    remote_device_t *device = remote_device_find_by_busid(&ctx->device_list, busid);
    if (!device) {
        return ERR_NOT_FOUND;
    }

    remote_device_disconnect(&ctx->device_list, device);
    return ERR_SUCCESS;
}

void client_gui_start_discovery(client_gui_context_t *ctx) {
    on_discover_clicked(ctx);
}

/* ----- Window Procedure ----- */

static LRESULT CALLBACK ClientWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    client_gui_context_t *ctx = g_client_ctx;

    switch (msg) {
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_CLIENT_BTN_DISCOVER:
                    on_discover_clicked(ctx);
                    break;
                case IDC_CLIENT_BTN_CONNECT:
                    on_connect_clicked(ctx);
                    break;
                case IDC_CLIENT_BTN_REFRESH:
                    on_refresh_clicked(ctx);
                    break;
            }
            break;

        case WM_NOTIFY: {
            NMHDR *nmhdr = (NMHDR *)lParam;
            if (nmhdr->idFrom == IDC_CLIENT_DEVICE_LIST && nmhdr->code == LVN_ITEMCHANGED) {
                NMLISTVIEW *nmlv = (NMLISTVIEW *)lParam;
                /* Check if checkbox state changed */
                if ((nmlv->uChanged & LVIF_STATE) &&
                    ((nmlv->uOldState & LVIS_STATEIMAGEMASK) != (nmlv->uNewState & LVIS_STATEIMAGEMASK))) {
                    /* Checkbox was toggled */
                    on_device_checkbox_changed(ctx, nmlv->iItem);
                }
            }
            break;
        }

        case WM_DISCOVERY_DONE:
            EnableWindow(ctx->hBtnDiscover, TRUE);
            gui_statusbar_set_text(ctx->base.hStatusBar,
                ctx->discovery_result.server_count > 0 ? L"Discovery complete" : L"No servers found");
            show_discovery_results(ctx);

            if (ctx->discovery_thread) {
                CloseHandle(ctx->discovery_thread);
                ctx->discovery_thread = NULL;
            }
            break;

        case WM_SIZE:
            /* Reposition all controls to match the new window size; the device
             * list grows, buttons dock above the status bar. Skip when
             * minimized to avoid pointless work. */
            if (wParam != SIZE_MINIMIZED) {
                gui_layout_client(ctx);
            }
            break;

        case WM_CLOSE:
            if (ctx->device_list.count > 0) {
                if (gui_show_confirm(hWnd, L"Detach all devices and exit?") != IDYES) {
                    return 0;
                }
            }
            DestroyWindow(hWnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    return 0;
}

/* ----- Entry Point ----- */

int client_gui_main(HINSTANCE hInstance, int nCmdShow) {
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

    /* Initialize and run GUI */
    client_gui_context_t ctx;
    if (!client_gui_init(&ctx, hInstance)) {
        gui_show_error(NULL, L"Failed to initialize client GUI");
        return 1;
    }

    int result = client_gui_run(&ctx);

    client_gui_cleanup(&ctx);
    network_cleanup();
    log_cleanup();

    return result;
}

/* WinMain for GUI application */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    return client_gui_main(hInstance, nCmdShow);
}
