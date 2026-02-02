/*
 * USB Over Network - GUI Resource Definitions
 * Control IDs, Menu IDs, and Resource Constants
 */

#ifndef GUI_RESOURCE_H
#define GUI_RESOURCE_H

/* Application Icons */
#define IDI_APP_ICON            100
#define IDI_TRAY_ICON           101

/* Server Window Controls */
#define IDC_SERVER_DEVICE_LIST  1001
#define IDC_SERVER_CLIENT_LIST  1002
#define IDC_SERVER_BTN_REFRESH  1003
#define IDC_SERVER_BTN_SHARE    1004
#define IDC_SERVER_BTN_STOP     1005
#define IDC_SERVER_BTN_HIDE     1006
#define IDC_SERVER_LABEL_DEV    1007
#define IDC_SERVER_LABEL_CLI    1008

/* Client Window Controls */
#define IDC_CLIENT_EDIT_SERVER  1101
#define IDC_CLIENT_BTN_DISCOVER 1102
#define IDC_CLIENT_BTN_CONNECT  1103
#define IDC_CLIENT_DEVICE_LIST  1104
#define IDC_CLIENT_BTN_ATTACH   1105
#define IDC_CLIENT_BTN_DETACH   1106
#define IDC_CLIENT_BTN_REFRESH  1107
#define IDC_CLIENT_STATUSBAR    1108
#define IDC_CLIENT_LABEL_SERVER 1109

/* Tray Menu IDs */
#define IDM_TRAY_SHOW           2001
#define IDM_TRAY_STOP_ALL       2002
#define IDM_TRAY_SEPARATOR      2003
#define IDM_TRAY_EXIT           2004

/* Timer IDs */
#define IDT_REFRESH_TIMER       3001
#define IDT_STATUS_TIMER        3002

/* Custom Window Messages */
#define WM_TRAYICON             (WM_USER + 100)
#define WM_DEVICE_UPDATE        (WM_USER + 101)
#define WM_CLIENT_UPDATE        (WM_USER + 102)
#define WM_STATUS_UPDATE        (WM_USER + 103)
#define WM_DISCOVERY_DONE       (WM_USER + 104)

/* Window Dimensions */
#define SERVER_WINDOW_WIDTH     520
#define SERVER_WINDOW_HEIGHT    380
#define CLIENT_WINDOW_WIDTH     480
#define CLIENT_WINDOW_HEIGHT    340

/* ListView Column Widths for Server Device List (520-20-16=484px, -24 checkbox=460px) */
#define SRV_COL_BUSID           50
#define SRV_COL_VIDPID          75
#define SRV_COL_DESC            280
#define SRV_COL_STATUS          55
/* Server Connected Clients List (no checkbox, 484px) */
#define SRV_COL_IP              180
#define SRV_COL_DEVICE          150
#define SRV_COL_TIME            154
/* Client Device List (480-20-16=444px, -24 checkbox=420px) */
#define CLI_COL_BUSID           45
#define CLI_COL_VIDPID          70
#define CLI_COL_DESC            250
#define CLI_COL_STATUS          55

/* Refresh Intervals (ms) */
#define REFRESH_INTERVAL_MS     5000
#define STATUS_UPDATE_MS        1000

/* String Table IDs */
#define IDS_APP_TITLE_SERVER    4001
#define IDS_APP_TITLE_CLIENT    4002
#define IDS_STATUS_READY        4003
#define IDS_STATUS_CONNECTED    4004
#define IDS_STATUS_DISCONNECTED 4005

/* Dialog IDs */
#define IDD_DISCOVERY           5001

#endif /* GUI_RESOURCE_H */
