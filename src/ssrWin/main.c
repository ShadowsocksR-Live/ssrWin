#include <Windows.h>
#include <WindowsX.h>
#include <assert.h>
#include "resource.h"
#include "w32taskbar.h"

HINSTANCE hinst;
HWND hMainDlg = NULL;
HWND hTrayWnd = NULL;

#define TRAY_ICON_ID 1

ATOM RegisterWndClass(HINSTANCE hInstance, const wchar_t* szWindowClass);
HWND InitInstance(HINSTANCE hInstance, const wchar_t* wndClass, const wchar_t* title, int nCmdShow);
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
static void TrayClickCb(void* p);
static void ShowWindowSimple(HWND hWnd, BOOL bShow);
static void RestoreWindowPos(HWND hWnd);

int PASCAL wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpszCmdLine, int nCmdShow)
{
    MSG msg = { 0 };
    BOOL bRet = FALSE;
    WNDCLASSW wc = { 0 };
    HACCEL hAccel;
    HMENU hMenuTray;
    HICON hIconApp;
    wchar_t WndClass[MAX_PATH] = { 0 };
    wchar_t AppName[MAX_PATH] = { 0 };
    UNREFERENCED_PARAMETER(lpszCmdLine);

    hinst = hInstance;

    LoadStringW(hInstance, IDS_MAIN_WND_CLASS, WndClass, ARRAYSIZE(WndClass));
    RegisterWndClass(hInstance, WndClass);

    LoadStringW(hInstance, IDS_APP_NAME, AppName, ARRAYSIZE(AppName));
    hMainDlg = InitInstance(hInstance, WndClass, AppName, nCmdShow);

    {
        hMenuTray = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_TRAYMENU));
        hTrayWnd = CreateTrayWindow(hInstance, hMenuTray, hMainDlg);

        hIconApp = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SSRWIN));
        TrayAddIcon(hTrayWnd, TRAY_ICON_ID, hIconApp, AppName);

        TraySetClickCallback(hTrayWnd, TrayClickCb, hMainDlg);
    }

    SendMessage(hMainDlg, WM_SETICON, ICON_BIG, (LPARAM)hIconApp);

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

    return (int)msg.wParam;
}

ATOM RegisterWndClass(HINSTANCE hInstance, const wchar_t* szWindowClass)
{
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = MainWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SSRWIN));
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDR_TRAYMENU);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SSRWIN));

    return RegisterClassExW(&wcex);
}

HWND InitInstance(HINSTANCE hInstance, const wchar_t* wndClass, const wchar_t* title, int nCmdShow)
{
    HWND hWnd = CreateWindowW(wndClass, title, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);
    if (IsWindow(hWnd)) {
        ShowWindow(hWnd, nCmdShow);
        UpdateWindow(hWnd);
    }
    return hWnd;
}

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    BOOL passToNext = TRUE;
    switch (message)
    {
    case WM_CREATE:
        RestoreWindowPos(hWnd);
        break;
    case WM_CLOSE:
        if (wParam == ID_CMD_EXIT) {
            DestroyWindow(hWnd);
        }
        else {
            ShowWindowSimple(hWnd, FALSE);
        }
        passToNext = FALSE;
        break;
    case WM_DESTROY:
        TrayDeleteIcon(hTrayWnd, TRAY_ICON_ID);
        PostQuitMessage(0);
        passToNext = FALSE;
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_CMD_EXIT:
            SendMessageW(hWnd, WM_CLOSE, ID_CMD_EXIT, 0);
            break;
        case ID_CMD_CONFIG:
            ShowWindowSimple(hWnd, TRUE);
            break;
        case IDCANCEL:
            SendMessageW(hWnd, WM_CLOSE, 0, 0);
            break;
        }
        passToNext = FALSE;
        break;
    case WM_SYSCOMMAND:
        switch (wParam)
        {
        case SC_CLOSE:
        case SC_MINIMIZE:
            ShowWindowSimple(hWnd, FALSE);
            passToNext = FALSE;
            break;
        }
        break;

    default:
        break;
    }
    if (passToNext) {
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

static void TrayClickCb(void* p) {
    HWND hWnd = (HWND)p;
    if (IsWindow(hWnd)) {
        ShowWindowSimple(hWnd, IsWindowVisible(hWnd) ? FALSE : TRUE);
    }
}

BOOL g_bShowOnTaskBar = FALSE;
static void ShowWindowSimple(HWND hWnd, BOOL bShow) {
    if (IsWindow(hWnd) == FALSE) {
        return;
    }
    if (bShow) {
        SetForegroundWindow(hWnd);
        SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOMOVE);
        ShowWindow(hWnd, SW_RESTORE);
    }
    else {
        ShowWindow(hWnd, g_bShowOnTaskBar ? SW_MINIMIZE : SW_HIDE);
    }
}

static void RestoreWindowPos(HWND hWnd) {
    HWND hwndOwner;
    RECT rc, rcDlg, rcOwner;
    if ((hwndOwner = GetParent(hWnd)) == NULL) {
        hwndOwner = GetDesktopWindow();
    }

    GetWindowRect(hwndOwner, &rcOwner);
    GetWindowRect(hWnd, &rcDlg);
    CopyRect(&rc, &rcOwner);

    // Offset the owner and dialog box rectangles so that right and bottom 
    // values represent the width and height, and then offset the owner again 
    // to discard space taken up by the dialog box. 

    OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top);
    OffsetRect(&rc, -rc.left, -rc.top);
    OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom);

    // The new position is the sum of half the remaining space and the owner's 
    // original position. 

    SetWindowPos(hWnd,
        HWND_TOP,
        rcOwner.left + (rc.right / 2),
        rcOwner.top + (rc.bottom / 2),
        0, 0,          // Ignores size arguments. 
        SWP_NOSIZE);
}
