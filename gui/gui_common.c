/*
 * USB Over Network - Common GUI Implementation
 * Shared GUI utility functions
 */

#include "gui_common.h"
#include <stdio.h>

/* ----- Initialization ----- */

void gui_init_common_controls(void) {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);
}

HFONT gui_create_font(int size, const wchar_t *name) {
    return CreateFontW(
        -MulDiv(size, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72),
        0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        name
    );
}

/* ----- Control Creation ----- */

HWND gui_create_listview(HWND hParent, int id, int x, int y, int w, int h, DWORD style) {
    DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | style;

    HWND hList = CreateWindowExW(
        0, WC_LISTVIEWW, L"",
        dwStyle,
        x, y, w, h,
        hParent, (HMENU)(INT_PTR)id,
        GetModuleHandle(NULL), NULL
    );

    if (hList) {
        ListView_SetExtendedListViewStyle(hList,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES);
    }

    return hList;
}

HWND gui_create_button(HWND hParent, int id, const wchar_t *text, int x, int y, int w, int h) {
    return CreateWindowExW(
        0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, w, h,
        hParent, (HMENU)(INT_PTR)id,
        GetModuleHandle(NULL), NULL
    );
}

HWND gui_create_edit(HWND hParent, int id, int x, int y, int w, int h) {
    return CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        x, y, w, h,
        hParent, (HMENU)(INT_PTR)id,
        GetModuleHandle(NULL), NULL
    );
}

HWND gui_create_label(HWND hParent, int id, const wchar_t *text, int x, int y, int w, int h) {
    return CreateWindowExW(
        0, L"STATIC", text,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, w, h,
        hParent, (HMENU)(INT_PTR)id,
        GetModuleHandle(NULL), NULL
    );
}

HWND gui_create_statusbar(HWND hParent, int id) {
    HWND hStatus = CreateWindowExW(
        0, STATUSCLASSNAMEW, NULL,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        hParent, (HMENU)(INT_PTR)id,
        GetModuleHandle(NULL), NULL
    );
    return hStatus;
}

/* ----- ListView Operations ----- */

void gui_listview_clear(HWND hList) {
    ListView_DeleteAllItems(hList);
}

int gui_listview_add_item(HWND hList, int index, const wchar_t *text) {
    LVITEMW lvi = {0};
    lvi.mask = LVIF_TEXT;
    lvi.iItem = index;
    lvi.pszText = (LPWSTR)text;
    return ListView_InsertItem(hList, &lvi);
}

void gui_listview_set_item(HWND hList, int row, int col, const wchar_t *text) {
    ListView_SetItemText(hList, row, col, (LPWSTR)text);
}

int gui_listview_get_selected(HWND hList) {
    return ListView_GetNextItem(hList, -1, LVNI_SELECTED);
}

void gui_listview_set_checkbox(HWND hList, int row, bool checked) {
    ListView_SetCheckState(hList, row, checked);
}

bool gui_listview_get_checkbox(HWND hList, int row) {
    return ListView_GetCheckState(hList, row) != 0;
}

void gui_listview_autosize_columns(HWND hList) {
    HWND hHeader = ListView_GetHeader(hList);
    int colCount = Header_GetItemCount(hHeader);

    for (int i = 0; i < colCount; i++) {
        ListView_SetColumnWidth(hList, i, LVSCW_AUTOSIZE_USEHEADER);
    }
}

/* ----- Tray Icon ----- */

bool gui_tray_add(gui_context_t *ctx, UINT uID, const wchar_t *tip) {
    memset(&ctx->nid, 0, sizeof(ctx->nid));
    ctx->nid.cbSize = sizeof(ctx->nid);
    ctx->nid.hWnd = ctx->hMainWnd;
    ctx->nid.uID = uID;
    ctx->nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    ctx->nid.uCallbackMessage = WM_TRAYICON;
    ctx->nid.hIcon = LoadIcon(ctx->hInstance, MAKEINTRESOURCE(IDI_APP_ICON));

    if (ctx->nid.hIcon == NULL) {
        ctx->nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }

    if (tip) {
        wcsncpy(ctx->nid.szTip, tip, sizeof(ctx->nid.szTip) / sizeof(wchar_t) - 1);
    }

    return Shell_NotifyIconW(NIM_ADD, &ctx->nid) != 0;
}

void gui_tray_remove(gui_context_t *ctx) {
    Shell_NotifyIconW(NIM_DELETE, &ctx->nid);
}

void gui_tray_update_tip(gui_context_t *ctx, const wchar_t *tip) {
    if (tip) {
        wcsncpy(ctx->nid.szTip, tip, sizeof(ctx->nid.szTip) / sizeof(wchar_t) - 1);
        ctx->nid.uFlags = NIF_TIP;
        Shell_NotifyIconW(NIM_MODIFY, &ctx->nid);
    }
}

void gui_tray_show_menu(gui_context_t *ctx, POINT pt) {
    SetForegroundWindow(ctx->hMainWnd);
    TrackPopupMenu(ctx->hTrayMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, ctx->hMainWnd, NULL);
    PostMessage(ctx->hMainWnd, WM_NULL, 0, 0);
}

/* ----- Message Helpers ----- */

void gui_show_error(HWND hParent, const wchar_t *message) {
    MessageBoxW(hParent, message, L"Error", MB_OK | MB_ICONERROR);
}

void gui_show_info(HWND hParent, const wchar_t *message) {
    MessageBoxW(hParent, message, L"Information", MB_OK | MB_ICONINFORMATION);
}

int gui_show_confirm(HWND hParent, const wchar_t *message) {
    return MessageBoxW(hParent, message, L"Confirm", MB_YESNO | MB_ICONQUESTION);
}

/* ----- String Conversion ----- */

void gui_utf8_to_wide(const char *utf8, wchar_t *wide, int wideLen) {
    if (utf8 == NULL || wide == NULL || wideLen <= 0) {
        if (wide && wideLen > 0) wide[0] = L'\0';
        return;
    }
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, wideLen);
}

void gui_wide_to_utf8(const wchar_t *wide, char *utf8, int utf8Len) {
    if (wide == NULL || utf8 == NULL || utf8Len <= 0) {
        if (utf8 && utf8Len > 0) utf8[0] = '\0';
        return;
    }
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, utf8Len, NULL, NULL);
}

/* ----- Status Bar ----- */

void gui_statusbar_set_text(HWND hStatus, const wchar_t *text) {
    if (hStatus) {
        SendMessageW(hStatus, SB_SETTEXTW, 0, (LPARAM)text);
    }
}
