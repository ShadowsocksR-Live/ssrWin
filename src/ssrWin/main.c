#include <Windows.h>
#include <WindowsX.h>
#include <CommCtrl.h>
#include <assert.h>
#include "resource.h"
#include "w32taskbar.h"
#include <privoxyexports.h>
#include <exe_file_path.h>
#include <ssr_executive.h>
#include "settings_json.h"

HWND hMainDlg = NULL;
HWND hTrayWnd = NULL;
HWND hListView = NULL;

char settings_file[MAX_PATH] = { 0 };

#define TRAY_ICON_ID 1

ATOM RegisterWndClass(HINSTANCE hInstance, const wchar_t* szWindowClass);
HWND InitInstance(HINSTANCE hInstance, const wchar_t* wndClass, const wchar_t* title, int nCmdShow);
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
static void TrayClickCb(void* p);
static void ShowWindowSimple(HWND hWnd, BOOL bShow);
static void RestoreWindowPos(HWND hWnd);
static HWND create_list_view(HWND hwndParent, HINSTANCE hinstance);
BOOL InitListViewColumns(HWND hWndListView);

static void json_config_iter(struct server_config* config, void* p);

int PASCAL wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpszCmdLine, int nCmdShow)
{
    MSG msg = { 0 };
    BOOL bRet = FALSE;
    WNDCLASSW wc = { 0 };
    HACCEL hAccel;
    HMENU hMenuTray, hmenu;
    HICON hIconApp;
    wchar_t WndClass[MAX_PATH] = { 0 };
    wchar_t AppName[MAX_PATH] = { 0 };
    UNREFERENCED_PARAMETER(lpszCmdLine);

    LoadStringW(hInstance, IDS_MAIN_WND_CLASS, WndClass, ARRAYSIZE(WndClass));
    RegisterWndClass(hInstance, WndClass);

    LoadStringW(hInstance, IDS_APP_NAME, AppName, ARRAYSIZE(AppName));
    hMainDlg = InitInstance(hInstance, WndClass, AppName, nCmdShow);

    {
        hMenuTray = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_TRAYMENU));
        hmenu = GetSubMenu(hMenuTray, 0);
        hTrayWnd = CreateTrayWindow(hInstance, hmenu, hMainDlg);

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
            //!IsDialogMessage(hMainDlg, &msg) ||
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
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = MainWndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SSRWIN));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDR_TRAYMENU);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SSRWIN));

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
    LPCREATESTRUCTW pcs = NULL;
    switch (message)
    {
    case WM_CREATE:
        pcs = (LPCREATESTRUCTW)lParam;
        RestoreWindowPos(hWnd);
        hListView = create_list_view(hWnd, pcs->hInstance);
        InitListViewColumns(hListView);
        {
            char* p, * tmp = exe_file_path(&malloc);
            if (tmp && (p = strrchr(tmp, '\\'))) {
                *p = '\0';
                sprintf(settings_file, "%s/settings.json", tmp);
                free(tmp);
            }
            parse_settings_file(settings_file, json_config_iter, hWnd);
        }
        break;
    case WM_SIZE:
        if (hListView) {
            RECT rc = { 0 };
            GetClientRect(hWnd, &rc);
            SetWindowPos(hListView, NULL, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER);
        }
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
        DestroyWindow(hListView);
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

static HWND create_list_view(HWND hwndParent, HINSTANCE hinstance)
{
#pragma comment(lib, "Comctl32.lib")
    RECT rcClient = { 0 };
    HWND hWndListView;

    INITCOMMONCONTROLSEX icex = { 0 };
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    GetClientRect(hwndParent, &rcClient);

    // Create the list-view window in report view with label editing enabled.
    hWndListView = CreateWindowW(WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_EDITLABELS,
        0, 0, rcClient.right - rcClient.left, rcClient.bottom - rcClient.top,
        hwndParent,
        NULL, // (HMENU)IDM_CODE_SAMPLES,
        hinstance,
        NULL);

    return hWndListView;
}

BOOL InitListViewColumns(HWND hWndListView)
{
    struct columm_info {
        wchar_t* name;
        int fmt;
        int width;
    } columns[] = {
        { L"Remarks", LVCFMT_LEFT, 100, },
        { L"Server ip", LVCFMT_LEFT, 200, },
        { L"Server port", LVCFMT_RIGHT, 80, },
        { L"Method", LVCFMT_RIGHT, 150, },
        { L"Password", LVCFMT_RIGHT, 200, },
    };

    LVCOLUMNW lvc = { 0 };
    int iCol;

    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

    for (iCol = 0; iCol < ARRAYSIZE(columns); iCol++) {
        lvc.iSubItem = iCol;
        lvc.pszText = columns[iCol].name;
        lvc.cx = columns[iCol].width;
        lvc.fmt = columns[iCol].fmt;
        // Insert the columns into the list view.
        if (ListView_InsertColumn(hWndListView, iCol, &lvc) == -1) {
            return FALSE;
        }
    }

    return TRUE;
}


static void json_config_iter(struct server_config* config, void* p) {
    config_release(config);
}
