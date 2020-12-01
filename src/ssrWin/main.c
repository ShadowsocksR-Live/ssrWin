#include <Windows.h>
#include <WindowsX.h>
#include <assert.h>
#include "resource.h"
#include <privoxyexports.h>
#include "w32taskbar.h"

HINSTANCE hinst;
HWND hMainDlg = NULL;
HWND hTrayWnd = NULL;

#define TRAY_ICON_ID 1

INT_PTR CALLBACK MainDlgProc(HWND, UINT, WPARAM, LPARAM);
static void TrayDblClkCb(void*p);
static void TrayClickCb(void*p);

int PASCAL wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpszCmdLine, int nCmdShow)
{
    MSG msg = { 0 };
    BOOL bRet = FALSE;
    WNDCLASSW wc = { 0 };
    HACCEL hAccel;
    HMENU hMenuTray;
    HICON hIconApp;
    wchar_t AppName[MAX_PATH] = { 0 };
    UNREFERENCED_PARAMETER(lpszCmdLine);

    hinst = hInstance;

    hMainDlg = CreateDialogW(hInstance, MAKEINTRESOURCE(IDD_CONFIG_LIST), NULL, MainDlgProc);
    ShowWindow(hMainDlg, nCmdShow);

    {
        hMenuTray = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_TRAYMENU));
        hTrayWnd = CreateTrayWindow(hInstance, hMenuTray, hMainDlg);

        hIconApp = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SSRWIN));
        LoadStringW(hInstance, IDS_APP_NAME, AppName, ARRAYSIZE(AppName));
        TrayAddIcon(hTrayWnd, TRAY_ICON_ID, hIconApp, AppName);

        TraySetDblClkCallback(hTrayWnd, TrayDblClkCb, hMainDlg);
        TraySetClickCallback(hTrayWnd, TrayClickCb, hMainDlg);
    }

    hAccel = LoadAcceleratorsW(hInstance, MAKEINTRESOURCEW(IDR_ACCE_CONFIG));

    while ((bRet = GetMessageW(&msg, NULL, 0, 0)) != FALSE) {
        if (bRet == -1) {
            // handle the error and possibly exit
            break;
        }
        else if (!IsWindow(hMainDlg) || 
            !IsDialogMessage(hMainDlg, &msg) ||
            !TranslateAccelerator(hMainDlg, hAccel, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}

INT_PTR CALLBACK MainDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    INT_PTR result = (INT_PTR)FALSE;
    switch (message)
    {
    case WM_INITDIALOG: 
        break;
    case WM_CLOSE:
        if (wParam == ID_CMD_EXIT) {
            DestroyWindow(hWnd);
        } else {
            ShowWindow(hWnd, SW_HIDE);
        }
        result = (INT_PTR)TRUE;
        break;
    case WM_DESTROY:
        TrayDeleteIcon(hTrayWnd, TRAY_ICON_ID);
        PostQuitMessage(0);
        result = (INT_PTR)TRUE;
        break;
    case WM_COMMAND: 
        switch (LOWORD(wParam)) 
        {
        case ID_CMD_EXIT:
            SendMessageW(hWnd, WM_CLOSE, ID_CMD_EXIT, 0);
            break;
        case ID_CMD_CONFIG:
            ShowWindow(hWnd, SW_SHOW);
            break;
        case IDCANCEL:
            SendMessageW(hWnd, WM_CLOSE, 0, 0);
            break;
        }
        result = (INT_PTR)TRUE;
        break;
    default:
        break;
    }
    return result;
}

static void TrayDblClkCb(void*p) {
    HWND hWnd = (HWND)p;
    if (IsWindow(hWnd)) {
        ShowWindow(hWnd, SW_SHOW);
    }
}

static void TrayClickCb(void*p) {
    TrayDblClkCb(p);
}
