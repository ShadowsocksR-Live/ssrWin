/*********************************************************************
*
* File        :  $Source: w32taskbar.c,v $
*
* Purpose     :  Functions for creating, setting and destroying the
*                workspace tray icon
*
*********************************************************************/

#include <windows.h>
#include <stdio.h>

#ifndef STRICT
#define STRICT
#endif

#include "w32taskbar.h"

#define WM_TRAYMSG WM_USER+1

static HMENU g_hmenuTray = NULL;
static HWND g_hwndTrayX = NULL;
static UINT g_traycreatedmsg = 0;
static HWND g_hwndReciever = NULL;
static HICON g_hiconApp = NULL;
static wchar_t g_szToolTip[MAX_PATH] = { 0 };
static void(*g_dbclk_cb)(void*p) = NULL;
static void* g_dbclk_cb_p = NULL;
static void(*g_clk_cb)(void*p) = NULL;
static void* g_clk_cb_p = NULL;

static LRESULT CALLBACK TrayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/*********************************************************************
*
* Function    :  CreateTrayWindow
*
* Description :  Creates and returns the invisible window responsible
*                for processing tray messages.
*
* Parameters  :
*          1  :  hInstance = instance handle of this application
*
* Returns     :  Handle of the systray window.
*
*********************************************************************/
HWND CreateTrayWindow(HINSTANCE hInstance, HMENU hMenuTray, HWND hwndReciever)
{
    static const wchar_t* szWndName = L"MagicTrayWindow";
    WNDCLASSW wc = { 0 };

    wc.lpfnWndProc = TrayProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = szWndName;

    RegisterClassW(&wc);

    /* TaskbarCreated is sent to a window when it should re-add its tray icons */
    g_traycreatedmsg = RegisterWindowMessageW(L"TaskbarCreated");

    g_hwndTrayX = CreateWindowW(szWndName, szWndName, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, hInstance, NULL);

    ShowWindow(g_hwndTrayX, SW_HIDE);
    UpdateWindow(g_hwndTrayX);

    // g_hmenuTray = LoadMenuW(hInstance, MAKEINTRESOURCE(IDR_TRAYMENU));
    g_hmenuTray = hMenuTray;
    g_hwndReciever = hwndReciever;

    return g_hwndTrayX;
}

void TraySetDblClkCallback(HWND hwnd, void(*cb)(void*p), void*p) {
    g_dbclk_cb = cb;
    g_dbclk_cb_p = p;
}

void TraySetClickCallback(HWND hwnd, void(*cb)(void*p), void*p) {
    g_clk_cb = cb;
    g_clk_cb_p = p;
}

/*********************************************************************
*
* Function    :  TraySetIcon
*
* Description :  Sets the tray icon to the specified shape.
*
* Parameters  :
*          1  :  hwnd = handle of the systray window
*          2  :  uID = user message number to notify systray window
*          3  :  hicon = set the current icon to this handle
*
* Returns     :  Same value as `Shell_NotifyIcon'.
*
*********************************************************************/
BOOL TraySetIcon(HWND hwnd, UINT uID, HICON hicon)
{
    NOTIFYICONDATAW nid = { 0 };
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = uID;
    nid.uFlags = NIF_ICON;
    nid.uCallbackMessage = 0;
    nid.hIcon = hicon;
    return Shell_NotifyIconW(NIM_MODIFY, &nid);
}

/*********************************************************************
*
* Function    :  TrayAddIcon
*
* Description :  Adds a tray icon.
*
* Parameters  :
*          1  :  hwnd = handle of the systray window
*          2  :  uID = user message number to notify systray window
*          3  :  hicon = handle of icon to add to systray window
*          4  :  pszToolTip = tool tip when mouse hovers over systray window
*
* Returns     :  Same as `Shell_NotifyIcon'.
*
*********************************************************************/
BOOL TrayAddIcon(HWND hwnd, UINT uID, HICON hicon, const wchar_t* pszToolTip)
{
    NOTIFYICONDATAW nid = { 0 };
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = uID;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYMSG;
    nid.hIcon = hicon;
    if (pszToolTip) {
        lstrcpyW(nid.szTip, pszToolTip);
    }
    if (g_hiconApp == NULL) {
        g_hiconApp = hicon;
    }
    if (g_szToolTip[0]==0 && pszToolTip) {
        lstrcpyW(g_szToolTip, pszToolTip);
    }
    return Shell_NotifyIconW(NIM_ADD, &nid);
}

/*********************************************************************
*
* Function    :  TrayDeleteIcon
*
* Description :  Deletes a tray icon.
*
* Parameters  :
*          1  :  hwnd = handle of the systray window
*          2  :  uID = user message number to notify systray window
*
* Returns     :  Same as `Shell_NotifyIcon'.
*
*********************************************************************/
BOOL TrayDeleteIcon(HWND hwnd, UINT uID)
{
    NOTIFYICONDATAW nid = { 0 };
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = uID;
    return Shell_NotifyIconW(NIM_DELETE, &nid);
}

/*********************************************************************
*
* Function    :  TrayProc
*
* Description :  Call back procedure processes tray messages.
*
* Parameters  :
*          1  :  hwnd = handle of the systray window
*          2  :  msg = message number
*          3  :  wParam = first param for this message
*          4  :  lParam = next param for this message
*
* Returns     :  Appropriate M$ window message handler codes.
*
*********************************************************************/
LRESULT CALLBACK TrayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        return 0;

    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;

    case WM_TRAYMSG:
    {
        /* UINT uID = (UINT) wParam; */
        UINT uMouseMsg = (UINT)lParam;
        if (uMouseMsg == WM_RBUTTONDOWN) {
            POINT pt = { 0 };
            HMENU hmenu = GetSubMenu(g_hmenuTray, 0);
            GetCursorPos(&pt);
            SetForegroundWindow(g_hwndReciever);
            TrackPopupMenu(hmenu, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, g_hwndReciever, NULL);
            PostMessage(g_hwndReciever, WM_NULL, 0, 0);
        }
        else if (uMouseMsg == WM_LBUTTONDBLCLK) {
            if (g_dbclk_cb) {
                g_dbclk_cb(g_dbclk_cb_p); // ShowLogWindow(TRUE);
            }
        }
        else if (uMouseMsg == WM_LBUTTONDOWN) {
            if (g_clk_cb) {
                g_clk_cb(g_clk_cb_p);
            }
        }
        return 0;
    }
    default:
        if (msg == g_traycreatedmsg) {
            if (g_hiconApp) {
                TrayAddIcon(g_hwndTrayX, 1, g_hiconApp, g_szToolTip);
            }
        }
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
