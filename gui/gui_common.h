/*
 * USB Over Network - Common GUI Definitions
 * Simple Win32 GUI helpers
 */

#ifndef GUI_COMMON_H
#define GUI_COMMON_H

#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE

#include <winsock2.h>
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "resource.h"

/* Window Class Names */
#define SERVER_CLASS_NAME   L"USBNetworkServer"
#define CLIENT_CLASS_NAME   L"USBNetworkClient"

/* Margins and Sizes (not in resource.h) */
#define MARGIN              10
#define PADDING             5
#define BUTTON_HEIGHT       28
#define BUTTON_WIDTH        90
#define LABEL_HEIGHT        18
#define EDIT_HEIGHT         24

/* Font */
#define GUI_FONT_NAME       L"Segoe UI"
#define GUI_FONT_SIZE       9

/* ListView column helper macro */
#define LV_ADDCOLUMN(hList, idx, text, width) do { \
    LVCOLUMNW lvc = {0}; \
    lvc.mask = LVCF_TEXT | LVCF_WIDTH; \
    lvc.cx = (width); \
    lvc.pszText = (LPWSTR)(text); \
    SendMessageW((hList), LVM_INSERTCOLUMNW, (idx), (LPARAM)&lvc); \
} while(0)

/* Base GUI context */
typedef struct gui_context {
    HINSTANCE hInstance;
    HWND hMainWnd;
    HWND hDeviceList;
    HWND hClientList;
    HWND hStatusBar;
    HFONT hFont;
    HMENU hTrayMenu;
    NOTIFYICONDATAW nid;
    bool running;
    bool minimizedToTray;
} gui_context_t;

/* Initialize common controls */
void gui_init_common_controls(void);

/* Create font */
HFONT gui_create_font(int size, const wchar_t *name);

/* Create controls */
HWND gui_create_listview(HWND hParent, int id, int x, int y, int w, int h, DWORD exStyle);
HWND gui_create_button(HWND hParent, int id, const wchar_t *text, int x, int y, int w, int h);
HWND gui_create_edit(HWND hParent, int id, int x, int y, int w, int h);
HWND gui_create_label(HWND hParent, int id, const wchar_t *text, int x, int y, int w, int h);
HWND gui_create_statusbar(HWND hParent, int id);

/* ListView operations */
void gui_listview_clear(HWND hList);
int gui_listview_add_item(HWND hList, int index, const wchar_t *text);
void gui_listview_set_item(HWND hList, int row, int col, const wchar_t *text);
int gui_listview_get_selected(HWND hList);
void gui_listview_set_checkbox(HWND hList, int row, bool checked);
bool gui_listview_get_checkbox(HWND hList, int row);
void gui_listview_autosize_columns(HWND hList);

/* Tray icon */
bool gui_tray_add(gui_context_t *ctx, UINT uID, const wchar_t *tip);
void gui_tray_remove(gui_context_t *ctx);
void gui_tray_update_tip(gui_context_t *ctx, const wchar_t *tip);
void gui_tray_show_menu(gui_context_t *ctx, POINT pt);

/* Message dialogs */
void gui_show_error(HWND hParent, const wchar_t *message);
void gui_show_info(HWND hParent, const wchar_t *message);
int gui_show_confirm(HWND hParent, const wchar_t *message);

/* String conversion */
void gui_utf8_to_wide(const char *utf8, wchar_t *wide, int wideLen);
void gui_wide_to_utf8(const wchar_t *wide, char *utf8, int utf8Len);

/* Status bar */
void gui_statusbar_set_text(HWND hStatus, const wchar_t *text);

#endif /* GUI_COMMON_H */
