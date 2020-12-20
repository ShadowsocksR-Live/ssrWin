#include <Windows.h>
#include <WindowsX.h>
#include <tchar.h>
#include <CommCtrl.h>
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#include <assert.h>
#include <ssr_client_api.h>
#include "resource.h"
#include "w32taskbar.h"
#include "settings_json.h"
#include "utf8_to_wchar.h"
#include "checkablegroupbox.h"
#include "run_ssr_client.h"
#include "qrcode_gen.h"
#include "save_bitmap.h"
#include "capture_screen.h"
#include "qrcode_dec.h"

HWND hTrayWnd = NULL;

struct main_wnd_data {
    HWND hMainDlg;
    HWND hListView;
    char settings_file[MAX_PATH];
    int cur_selected;
    struct ssr_client_ctx* client_ctx;

    // system settings
    BOOL auto_run;
    BOOL auto_connect;
    char ssr_listen_host[MAX_PATH];
    int ssr_listen_port;
    int privoxy_listen_port;
    int delay_quit_ms;
};

#define TRAY_ICON_ID 1
#define LIST_VIEW_ID 55
#define MENU_ID_NODE_BEGINNING 60000

#define APP_NAME_KEY "ssrWin"
#define APP_REG_KEY L"Software\\ssrwin"
#define AUTO_RUN_REG_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"

ATOM RegisterWndClass(HINSTANCE hInstance, const wchar_t* szWindowClass);
HWND InitInstance(HINSTANCE hInstance, const wchar_t* wndClass, const wchar_t* title, int nCmdShow);
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
static void on_wm_create(HWND hWnd, LPCREATESTRUCTW pcs);
static void on_wm_destroy(HWND hWnd);
static void on_cmd_clipboard_import_url(HWND hWnd);
static void on_cmd_scan_screen_qrcode(HWND hWnd);
static int adjust_current_selected_item(int cur_sel, int total_count);
static void before_tray_menu_popup(HMENU hMenu, void* p);
static void TrayClickCb(void* p);
static void ShowWindowSimple(HWND hWnd, BOOL bShow);
static void RestoreWindowPos(HWND hWnd);
static HWND create_list_view(HWND hwndParent, HINSTANCE hinstance);
BOOL InitListViewColumns(HWND hWndListView);
BOOL InsertListViewItem(HWND hWndListView, int index, struct server_config* config);
BOOL on_context_menu(HWND hWnd, HWND targetWnd, LPARAM lParam);
BOOL on_delete_item(HWND hWnd);
BOOL on_cmd_server_export_to_json(HWND hWnd);
BOOL on_cmd_qr_code(HWND hWnd);
BOOL handle_WM_NOTIFY_from_list_view(HWND hWnd, int ctlID, LPNMHDR pnmHdr);
static void view_server_details(HWND hWnd, HWND hWndList);
static void on_list_view_notification_get_disp_info(NMLVDISPINFOW* plvdi, const struct server_config* config);
static INT_PTR CALLBACK ConfigDetailsDlgProc(HWND hDlg, UINT uMessage, WPARAM wParam, LPARAM lParam);
static INT_PTR CALLBACK OptionsDlgProc(HWND hDlg, UINT uMessage, WPARAM wParam, LPARAM lParam);
static void config_dlg_init(HWND hDlg);
static void load_config_to_dlg(HWND hDlg, const struct server_config* config);
static void save_dlg_to_config(HWND hDlg, struct server_config* config);
static void combo_box_set_cur_sel(HWND hCombo, const wchar_t* cur_sel);
static void save_config_to_file(HWND hListView, const char* settings_file);
static struct server_config* retrieve_config_from_list_view(HWND hListView, int index);

static void json_config_iter(struct server_config* config, void* p);
static char* retrieve_string_from_clipboard(HWND hWnd, void* (*allocator)(size_t));
static void SetFocusToPreviousInstance(const wchar_t* windowClass, const wchar_t* windowCaption);

int PASCAL wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpszCmdLine, int nCmdShow)
{
    HANDLE hMutexHandle;
    HWND hMainDlg;
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
    LoadStringW(hInstance, IDS_APP_NAME, AppName, ARRAYSIZE(AppName));

    hMutexHandle = CreateMutexW(NULL, TRUE, L"ssr_win_single_instance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        SetFocusToPreviousInstance(WndClass, AppName);
        return 0;
    }

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    RegisterWndClass(hInstance, WndClass);

    hMainDlg = InitInstance(hInstance, WndClass, AppName, nCmdShow);

    {
        hMenuTray = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_TRAYMENU));
        hmenu = GetSubMenu(hMenuTray, 0);
        hTrayWnd = CreateTrayWindow(hInstance, hmenu, hMainDlg);

        hIconApp = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SSRWIN));
        TrayAddIcon(hTrayWnd, TRAY_ICON_ID, hIconApp, AppName);

        TraySetBeforePopupMenuCallback(hTrayWnd, before_tray_menu_popup, hMainDlg);
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

    CoUninitialize();

    ReleaseMutex(hMutexHandle);
    CloseHandle(hMutexHandle);

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
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDR_MENU_MAIN);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SSRWIN));

    return RegisterClassExW(&wcex);
}

HWND InitInstance(HINSTANCE hInstance, const wchar_t* wndClass, const wchar_t* title, int nCmdShow)
{
    HWND hWnd = CreateWindowW(wndClass, title, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);
    if (IsWindow(hWnd)) {
        struct main_wnd_data* wnd_data = NULL;
        wnd_data = (struct main_wnd_data*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

        ShowWindow(hWnd, wnd_data->client_ctx ? SW_HIDE : nCmdShow);
        UpdateWindow(hWnd);
    }
    return hWnd;
}

struct json_iter_data {
    struct main_wnd_data* wnd_data;
    int index;
};

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    int count;
    struct server_config* config;
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);
    struct main_wnd_data* wnd_data = NULL;
    BOOL passToNext = TRUE;
    DWORD cmd_id;
    wnd_data = (struct main_wnd_data*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    switch (message)
    {
    case WM_CREATE:
        assert(wnd_data == NULL);
        on_wm_create(hWnd, (LPCREATESTRUCTW)lParam);
        break;
    case WM_SIZE:
        if (wnd_data->hListView) {
            RECT rc = { 0 };
            GetClientRect(hWnd, &rc);
            SetWindowPos(wnd_data->hListView, NULL, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER);
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
        on_wm_destroy(hWnd);
        passToNext = FALSE;
        break;
    case WM_COMMAND:
        cmd_id = LOWORD(wParam);
        switch (cmd_id)
        {
        case ID_CMD_OPTIONS:
            DialogBoxParamW(hInstance,
                MAKEINTRESOURCEW(IDD_OPTIONS),
                hWnd, OptionsDlgProc, (LPARAM)wnd_data);
            SetFocus(wnd_data->hListView);
            break;
        case ID_CMD_IMPORT_URL:
            on_cmd_clipboard_import_url(hWnd);
            break;
        case ID_CMD_SCAN_QRCODE:
            on_cmd_scan_screen_qrcode(hWnd);
            break;
        case ID_CMD_NEW_RECORD:
            config = config_create();
            if (IDOK == DialogBoxParamW(hInstance,
                MAKEINTRESOURCEW(IDD_CONFIG_DETAILS),
                hWnd, ConfigDetailsDlgProc, (LPARAM)config))
            {
                int nIndex = ListView_GetItemCount(wnd_data->hListView);
                InsertListViewItem(wnd_data->hListView, nIndex, config);
            }
            else {
                config_release(config);
            }
            SetFocus(wnd_data->hListView);
            break;
        case ID_CMD_EXIT:
            SendMessageW(hWnd, WM_CLOSE, ID_CMD_EXIT, 0);
            break;
        case ID_CMD_MANAGE_CENTER:
            ShowWindowSimple(hWnd, TRUE);
            break;
        case IDCANCEL:
            SendMessageW(hWnd, WM_CLOSE, 0, 0);
            break;
        case ID_CMD_SERVER_TO_JSON:
            on_cmd_server_export_to_json(hWnd);
            break;
        case ID_CMD_QR_CODE:
            on_cmd_qr_code(hWnd);
            break;
        case ID_CMD_SERVER_DETAILS:
            view_server_details(hWnd, wnd_data->hListView);
            break;
        case ID_CMD_DELETE:
            on_delete_item(hWnd);
            break;
        case ID_CMD_RUN:
            wnd_data->cur_selected = adjust_current_selected_item(wnd_data->cur_selected, ListView_GetItemCount(wnd_data->hListView));
            if (wnd_data->cur_selected < 0) {
                wchar_t Info[MAX_PATH] = { 0 };
                wchar_t AppName[MAX_PATH] = { 0 };
                LoadStringW(hInstance, IDS_APP_NAME, AppName, ARRAYSIZE(AppName));
                LoadStringW(hInstance, IDS_NO_CONFIG, Info, ARRAYSIZE(Info));
                MessageBoxW(hWnd, Info, AppName, MB_OK);
            }
            else {
                assert(wnd_data->client_ctx == NULL);
                config = retrieve_config_from_list_view(wnd_data->hListView, wnd_data->cur_selected);
                wnd_data->client_ctx = ssr_client_begin_run(config, wnd_data->ssr_listen_host, wnd_data->ssr_listen_port, wnd_data->privoxy_listen_port, wnd_data->delay_quit_ms);
            }
            break;
        case ID_CMD_STOP:
            assert(wnd_data->client_ctx != NULL);
            ssr_client_terminate(wnd_data->client_ctx);
            wnd_data->client_ctx = NULL;
            break;
        default:
            count = ListView_GetItemCount(wnd_data->hListView);
            if ((MENU_ID_NODE_BEGINNING <= cmd_id) &&
                (cmd_id < (DWORD)(MENU_ID_NODE_BEGINNING + count)))
            {
                wnd_data->cur_selected = cmd_id - MENU_ID_NODE_BEGINNING;
                break;
            }
            assert(0);
            break;
        }
        passToNext = FALSE;
        break;
    case WM_SETFOCUS:
        passToNext = FALSE;
        SetFocus(wnd_data->hListView);
        break;
    case WM_CONTEXTMENU:
        on_context_menu(hWnd, (HWND)wParam, lParam);
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
    case WM_NOTIFY:
        passToNext = (HANDLE_WM_NOTIFY(hWnd, wParam, lParam, handle_WM_NOTIFY_from_list_view) == FALSE);
        break;
    default:
        break;
    }
    if (passToNext) {
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

static void dump_info_callback(int dump_level, const char* info, void* p) {
    struct main_wnd_data* wnd_data = (struct main_wnd_data*)p;
    // TODO: output the client info.
}

static void on_wm_create(HWND hWnd, LPCREATESTRUCTW pcs)
{
    struct main_wnd_data* wnd_data = NULL;
    wnd_data = (struct main_wnd_data*)calloc(1, sizeof(*wnd_data));
    SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)wnd_data);
    wnd_data->cur_selected = -1;

    wnd_data->auto_connect = FALSE;
    wnd_data->auto_run = FALSE;
    lstrcpyA(wnd_data->ssr_listen_host, "127.0.0.1");
    wnd_data->ssr_listen_port = 0;
    wnd_data->privoxy_listen_port = PRIVOXY_LISTEN_PORT;
    wnd_data->delay_quit_ms = SSR_DELAY_QUIT_MIN;

    wnd_data->hMainDlg = hWnd;
    RestoreWindowPos(hWnd);
    wnd_data->hListView = create_list_view(hWnd, pcs->hInstance);
    InitListViewColumns(wnd_data->hListView);
    SetFocus(wnd_data->hListView);
    {
        struct json_iter_data iter_data = { wnd_data, 0 };
        char* p, * tmp = exe_file_path(&malloc);
        if (tmp && (p = strrchr(tmp, '\\'))) {
            *p = '\0';
            sprintf(wnd_data->settings_file, "%s/settings.json", tmp);
            free(tmp);
        }
        parse_settings_file(wnd_data->settings_file, json_config_iter, &iter_data);
    }

    {
        HKEY hKey = NULL;
        DWORD dwtype, sizeBuff;
        LSTATUS lRet = RegOpenKeyExW(HKEY_CURRENT_USER, APP_REG_KEY, 0, KEY_READ, &hKey);
        if (lRet == ERROR_SUCCESS) {
            dwtype = REG_BINARY;
            sizeBuff = sizeof(wnd_data->cur_selected);
            lRet = RegQueryValueExW(hKey, L"cur_selected", 0, &dwtype, (BYTE*)&wnd_data->cur_selected, &sizeBuff);

            sizeBuff = sizeof(wnd_data->auto_connect);
            lRet = RegQueryValueExW(hKey, L"auto_connect", 0, &dwtype, (BYTE*)&wnd_data->auto_connect, &sizeBuff);

            sizeBuff = sizeof(wnd_data->ssr_listen_host);
            lRet = RegQueryValueExW(hKey, L"ssr_listen_host", 0, &dwtype, (BYTE*)&wnd_data->ssr_listen_host[0], &sizeBuff);

            sizeBuff = sizeof(wnd_data->ssr_listen_port);
            lRet = RegQueryValueExW(hKey, L"ssr_listen_port", 0, &dwtype, (BYTE*)&wnd_data->ssr_listen_port, &sizeBuff);

            sizeBuff = sizeof(wnd_data->privoxy_listen_port);
            lRet = RegQueryValueExW(hKey, L"privoxy_listen_port", 0, &dwtype, (BYTE*)&wnd_data->privoxy_listen_port, &sizeBuff);

            sizeBuff = sizeof(wnd_data->delay_quit_ms);
            lRet = RegQueryValueExW(hKey, L"delay_quit_ms", 0, &dwtype, (BYTE*)&wnd_data->delay_quit_ms, &sizeBuff);

            RegCloseKey(hKey);
        }
    }
    {
        HKEY hKey = NULL;
        LSTATUS lRet = RegOpenKeyExW(HKEY_CURRENT_USER, AUTO_RUN_REG_KEY, 0, KEY_READ, &hKey);
        if (lRet == ERROR_SUCCESS) {
            char *tmp;
            char path[MAX_PATH] = { 0 };
            DWORD dwtype = REG_SZ, sizeBuff = sizeof(path);
            RegQueryValueExA(hKey, APP_NAME_KEY, 0, &dwtype, (BYTE*)path, &sizeBuff);
            RegCloseKey(hKey);
            tmp = exe_file_path(&malloc);
            wnd_data->auto_run = (lstrcmpA(tmp, path) == 0) ? 1 : 0;
            free(tmp);
        }
    }
    set_dump_info_callback(dump_info_callback, wnd_data);

    do {
        static struct server_config* config;
        int index = wnd_data->cur_selected;
        if (wnd_data->auto_connect == FALSE) {
            break;
        }
        if (index < 0 || index >= ListView_GetItemCount(wnd_data->hListView)) {
            break;
        }
        assert(wnd_data->client_ctx == NULL);
        config = retrieve_config_from_list_view(wnd_data->hListView, index);
        assert(config);
        wnd_data->client_ctx = ssr_client_begin_run(config, wnd_data->ssr_listen_host, wnd_data->ssr_listen_port, wnd_data->privoxy_listen_port, wnd_data->delay_quit_ms);
    } while (0);
}

static void on_wm_destroy(HWND hWnd) {
    struct main_wnd_data* wnd_data;
    wnd_data = (struct main_wnd_data*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    ssr_client_terminate(wnd_data->client_ctx);
    save_config_to_file(wnd_data->hListView, wnd_data->settings_file);
    DestroyWindow(wnd_data->hListView);
    TrayDeleteIcon(hTrayWnd, TRAY_ICON_ID);
    PostQuitMessage(0);

    do {
        HKEY hKey = NULL;
        DWORD state = REG_CREATED_NEW_KEY;
        LSTATUS lRet = RegOpenKeyExW(HKEY_CURRENT_USER, APP_REG_KEY, 0, KEY_WRITE, &hKey);
        if (lRet != ERROR_SUCCESS) {
            lRet = RegCreateKeyExW(HKEY_CURRENT_USER, APP_REG_KEY, 0, NULL, 0, 0, NULL, &hKey, &state);
            if (lRet != ERROR_SUCCESS) {
                break;
            }
        }
        if (state != REG_CREATED_NEW_KEY) {
            RegCloseKey(hKey);
            break;
        }
        RegSetValueExW(hKey, L"cur_selected", 0, REG_BINARY, (BYTE*)&wnd_data->cur_selected, sizeof(wnd_data->cur_selected));

        RegSetValueExW(hKey, L"auto_connect", 0, REG_BINARY, (BYTE*)&wnd_data->auto_connect, sizeof(wnd_data->auto_connect));
        RegSetValueExW(hKey, L"ssr_listen_host", 0, REG_BINARY, (BYTE*)&wnd_data->ssr_listen_host[0], sizeof(wnd_data->ssr_listen_host));
        RegSetValueExW(hKey, L"ssr_listen_port", 0, REG_BINARY, (BYTE*)&wnd_data->ssr_listen_port, sizeof(wnd_data->ssr_listen_port));
        RegSetValueExW(hKey, L"privoxy_listen_port", 0, REG_BINARY, (BYTE*)&wnd_data->privoxy_listen_port, sizeof(wnd_data->privoxy_listen_port));
        RegSetValueExW(hKey, L"delay_quit_ms", 0, REG_BINARY, (BYTE*)&wnd_data->delay_quit_ms, sizeof(wnd_data->delay_quit_ms));

        RegCloseKey(hKey);
    } while (0);
    {
        HKEY hKey = NULL;
        LSTATUS lRet = RegOpenKeyExW(HKEY_CURRENT_USER, AUTO_RUN_REG_KEY, 0, KEY_WRITE, &hKey);
        if (lRet == ERROR_SUCCESS) {
            if (wnd_data->auto_run) {
                char *tmp = exe_file_path(&malloc);
                lRet = RegSetValueExA(hKey, APP_NAME_KEY, 0, REG_SZ, (BYTE*)tmp, lstrlenA(tmp) + 1);
                if (lRet != ERROR_SUCCESS) {
                    DebugBreak();
                }
                free(tmp);
            } else {
                RegDeleteValueA(hKey, APP_NAME_KEY);
            }
        }
    }

    free(wnd_data);
}

static void add_ssr_url_to_sub_list_view(HWND hWnd, const char* ssr_url) {
    wchar_t AppName[MAX_PATH] = { 0 };
    wchar_t InfoFmt[MAX_PATH] = { 0 };
    wchar_t Info[MAX_PATH] = { 0 };
    BOOL succ = FALSE;
    UINT uType;
    struct main_wnd_data* wnd_data = (struct main_wnd_data*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);
    if (ssr_url) {
        struct server_config* config = ssr_qr_code_decode(ssr_url);
        if (config) {
            int count = ListView_GetItemCount(wnd_data->hListView);
            InsertListViewItem(wnd_data->hListView, count, config);
            succ = TRUE;
        }
    }

    LoadStringW(hInstance, IDS_APP_NAME, AppName, ARRAYSIZE(AppName));
    LoadStringW(hInstance, IDS_IMPORT_URL, InfoFmt, ARRAYSIZE(InfoFmt));
    wsprintfW(Info, InfoFmt, succ ? L"successfully" : L"failed");
    uType = (succ ? MB_ICONINFORMATION : MB_ICONERROR) | MB_OK;
    MessageBoxW(hWnd, Info, AppName, uType);
}

static void on_cmd_clipboard_import_url(HWND hWnd) {
    char* ssr_url = retrieve_string_from_clipboard(hWnd, &malloc);
    add_ssr_url_to_sub_list_view(hWnd, ssr_url);
    free(ssr_url);
}

static void get_img_data_agent(void* ctx, void* (*allocator)(size_t), int* pWidth, int* pHeight, unsigned char** pData) {
    extract_bitmap_in_grayscale_8bpp((HBITMAP)ctx, allocator, pWidth, pHeight, pData);
}

static void on_cmd_scan_screen_qrcode(HWND hWnd) {
    HBITMAP bmp = capture_screen();
    char* ssr_url = qr_code_decoder(&get_img_data_agent, (void*)bmp, &malloc);
    add_ssr_url_to_sub_list_view(hWnd, ssr_url);
    free(ssr_url);
    DeleteObject(bmp);
}

static void modify_popup_menu_items(struct main_wnd_data* wnd_data, HMENU hMenu) {
    int node_count = ListView_GetItemCount(wnd_data->hListView);
    int menu_count = GetMenuItemCount(hMenu);
    int index;
    for (index = menu_count - 1; index >= 0; --index) {
        int item_id = GetMenuItemID(hMenu, index);
        if (item_id == 0) {
            break;
        }
        DeleteMenu(hMenu, item_id, MF_BYCOMMAND);
    }
    for (index = 0; index < node_count; ++index) {
        wchar_t tmp[MAX_PATH] = { 0 };
        struct server_config* config;
        char* name;
        config = retrieve_config_from_list_view(wnd_data->hListView, index);
        if (config == NULL) {
            assert(0);
            break;
        }
        name = lstrlenA(config->remarks) ? config->remarks : config->remote_host;
        utf8_to_wchar_string(name, tmp, ARRAYSIZE(tmp));
        AppendMenuW(hMenu, MF_STRING, (UINT)(MENU_ID_NODE_BEGINNING + index), tmp);
    }

    wnd_data->cur_selected = adjust_current_selected_item(wnd_data->cur_selected, node_count);

    if (wnd_data->cur_selected >= 0) {
        CheckMenuRadioItem(hMenu,
            MENU_ID_NODE_BEGINNING,
            (UINT)(MENU_ID_NODE_BEGINNING + node_count - 1),
            (UINT)(MENU_ID_NODE_BEGINNING + wnd_data->cur_selected),
            MF_BYCOMMAND | MF_CHECKED);
    }

    EnableMenuItem(hMenu, ID_CMD_RUN, MF_BYCOMMAND | ((wnd_data->client_ctx == NULL) && (wnd_data->cur_selected >= 0) ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, ID_CMD_STOP, MF_BYCOMMAND | (wnd_data->client_ctx != NULL ? MF_ENABLED : MF_GRAYED));
}

static int adjust_current_selected_item(int cur_sel, int total_count) {
    if (total_count > 0) {
        cur_sel = ((0 <= cur_sel) && (cur_sel < total_count)) ? cur_sel : 0;
    }
    else {
        cur_sel = -1;
    }
    return cur_sel;
}

static void before_tray_menu_popup(HMENU hMenu, void* p) {
    HWND hWnd = (HWND)p;
    struct main_wnd_data* wnd_data = NULL;
    if (!IsWindow(hWnd)) {
        assert(0);
        return;
    }
    wnd_data = (struct main_wnd_data*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    modify_popup_menu_items(wnd_data, hMenu);
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
        WS_CHILD | WS_VISIBLE | LVS_REPORT,
        0, 0, rcClient.right - rcClient.left, rcClient.bottom - rcClient.top,
        hwndParent,
        (HMENU)LIST_VIEW_ID,
        hinstance,
        NULL);

    ListView_SetExtendedListViewStyle(hWndListView, LVS_EX_FULLROWSELECT);

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
        { L"Server Address", LVCFMT_LEFT, 200, },
        { L"Server Port", LVCFMT_RIGHT, 80, },
        { L"Method", LVCFMT_LEFT, 100, },
        { L"Password", LVCFMT_LEFT, 230, },
        { L"Protocol", LVCFMT_LEFT, 150, },
        { L"Protocol Param", LVCFMT_LEFT, 150, },
        { L"Obfs", LVCFMT_LEFT, 150, },
        { L"Obfs Param", LVCFMT_LEFT, 150, },
        { L"SSRoT Enable", LVCFMT_LEFT, 100, },
        { L"SSRoT Domain", LVCFMT_LEFT, 200, },
        { L"SSRoT Path", LVCFMT_LEFT, 250, },
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

BOOL InsertListViewItem(HWND hWndListView, int index, struct server_config* config)
{
    LVITEMW lvI = { 0 };

    // Initialize LVITEM members that are common to all items.
    lvI.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE | LVIF_PARAM;
    lvI.pszText = LPSTR_TEXTCALLBACKW; // Sends an LVN_GETDISPINFO message.
    lvI.iItem = index;
    lvI.iImage = index;
    lvI.lParam = (LPARAM)config;

    // Insert items into the list.
    if (ListView_InsertItem(hWndListView, &lvI) == -1) {
        return FALSE;
    }

    return TRUE;
}

BOOL on_context_menu(HWND hWnd, HWND targetWnd, LPARAM lParam)
{
    int nIndex;
    HWND  hwndListView = targetWnd;
    HMENU hMenuLoad, hMenu;
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);
    UINT uEnable;

    if (hwndListView != GetDlgItem(hWnd, LIST_VIEW_ID)) {
        return FALSE;
    }

    nIndex = ListView_GetNextItem(hwndListView, -1, LVNI_SELECTED);

    hMenuLoad = LoadMenuW(hInstance, MAKEINTRESOURCEW(IDR_MENU_CONTEXT));
    hMenu = GetSubMenu(hMenuLoad, 0);

    uEnable = (MF_BYCOMMAND | ((nIndex >= 0) ? MF_ENABLED : (MF_GRAYED | MF_DISABLED)));
    EnableMenuItem(hMenu, ID_CMD_SERVER_DETAILS, uEnable);
    EnableMenuItem(hMenu, ID_CMD_QR_CODE, uEnable);
    EnableMenuItem(hMenu, ID_CMD_SERVER_TO_JSON, uEnable);
    EnableMenuItem(hMenu, ID_CMD_DELETE, uEnable);

    TrackPopupMenu(hMenu,
        TPM_LEFTALIGN | TPM_RIGHTBUTTON,
        GET_X_LPARAM(lParam),
        GET_Y_LPARAM(lParam),
        0,
        hWnd,
        NULL);

    DestroyMenu(hMenuLoad);

    return TRUE;
}

static INT_PTR CALLBACK QrCodeDlgProc(HWND hDlg, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
    static struct server_config* config;
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrW(hDlg, GWLP_HINSTANCE);
    HICON hIconApp;
    static char* qrcode_str;
    static HBITMAP hBmp = NULL;
    HDC dc, mdc;
    RECT rc;
    HMENU hMenuLoad, hMenu;
    HGLOBAL hglbCopy;
    LPSTR lptstrCopy;
    HBITMAP hBmpCopy;
    OPENFILENAMEW saveFileDialog = { 0 };
    wchar_t szSaveFileName[MAX_PATH] = { 0 };

    switch (uMessage)
    {
    case WM_INITDIALOG:
        RestoreWindowPos(hDlg);
        config = (struct server_config*)lParam;
        hIconApp = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SSRWIN));
        SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIconApp);
        qrcode_str = ssr_qr_code_encode(config, &malloc);
        hBmp = generate_qr_code_image(qrcode_str);
        return TRUE;
    case WM_CONTEXTMENU:
        hMenuLoad = LoadMenuW(hInstance, MAKEINTRESOURCEW(IDR_MENU_QRCODE));
        hMenu = GetSubMenu(hMenuLoad, 0);
        TrackPopupMenu(hMenu,
            TPM_LEFTALIGN | TPM_RIGHTBUTTON,
            GET_X_LPARAM(lParam),
            GET_Y_LPARAM(lParam),
            0,
            hDlg,
            NULL);
        DestroyMenu(hMenuLoad);
        break;
    case WM_DESTROY:
        DeleteObject(hBmp);
        free(qrcode_str);
        break;
    case WM_PAINT:
        GetClientRect(hDlg, &rc);
        dc = GetDC(hDlg), mdc = CreateCompatibleDC(dc);
        SelectObject(mdc, hBmp);
        BitBlt(dc, 10, 10, rc.right - rc.left, rc.bottom - rc.top, mdc, 0, 0, SRCCOPY);
        ReleaseDC(hDlg, dc);
        DeleteDC(mdc);
        break;
    case WM_COMMAND:
        switch (wParam)
        {
        case ID_CMD_COPY_TEXT:
            OpenClipboard(hDlg);
            EmptyClipboard();
            hglbCopy = GlobalAlloc(GMEM_MOVEABLE, (lstrlenA(qrcode_str) + 1) * sizeof(char));
            if (hglbCopy == NULL) {
                CloseClipboard();
                return FALSE;
            }
            lptstrCopy = (char*)GlobalLock(hglbCopy);
            lstrcpyA(lptstrCopy, qrcode_str);
            GlobalUnlock(hglbCopy);
            SetClipboardData(CF_TEXT, hglbCopy);
            CloseClipboard();
            break;
        case ID_CMD_COPY_IMAGE:
            hBmpCopy = (HBITMAP)CopyImage(hBmp, IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
            OpenClipboard(hDlg);
            EmptyClipboard();
            SetClipboardData(CF_BITMAP, hBmpCopy);
            CloseClipboard();
            break;
        case ID_CMD_SAVE_IMAGE_FILE:
            //Save Dialog
            saveFileDialog.lStructSize = sizeof(saveFileDialog);
            saveFileDialog.hwndOwner = hDlg;
            saveFileDialog.lpstrFilter = L"PNG Files (*.png)\0*.png\0Bitmap Files (*.bmp)\0*.bmp\0All Files (*.*)\0*.*\0\0";
            saveFileDialog.lpstrFile = szSaveFileName;
            saveFileDialog.nMaxFile = ARRAYSIZE(szSaveFileName);
            saveFileDialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
            saveFileDialog.lpstrDefExt = L"bmp";
            if (GetSaveFileNameW(&saveFileDialog)) {
                LPCWSTR end = StrRChrW(szSaveFileName, NULL, L'.');
                if (StrCmpW(end, L".png") == 0) {
                    save_bitmap_to_png_file(hBmp, szSaveFileName);
                }
                else if (StrCmpW(end, L".bmp") == 0) {
                    save_bitmap_to_bmp_file(hBmp, szSaveFileName);
                }
                else {
                    DebugBreak();
                }
            }
            break;
        case IDOK:
        case IDCANCEL:
            config = NULL;
            EndDialog(hDlg, wParam);
            break;
        }
        return TRUE;
    }
    return FALSE;
}

BOOL on_cmd_server_export_to_json(HWND hWnd) {
    struct main_wnd_data* wnd_data = (struct main_wnd_data*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);
    int nIndex = ListView_GetNextItem(wnd_data->hListView, -1, LVNI_SELECTED);
    struct server_config* config;
    if (nIndex < 0) {
        return FALSE;
    }
    config = retrieve_config_from_list_view(wnd_data->hListView, nIndex);
    if (config == NULL) {
        return FALSE;
    }
    {
        OPENFILENAMEA saveFileDialog = { 0 };
        char szSaveFileName[MAX_PATH] = { 0 };

        saveFileDialog.lStructSize = sizeof(saveFileDialog);
        saveFileDialog.hwndOwner = hWnd;
        saveFileDialog.lpstrFilter = "JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0\0";
        saveFileDialog.lpstrFile = szSaveFileName;
        saveFileDialog.nMaxFile = ARRAYSIZE(szSaveFileName);
        saveFileDialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
        saveFileDialog.lpstrDefExt = "json";
        if (GetSaveFileNameA(&saveFileDialog)) {
            save_single_config_to_json_file(config, szSaveFileName);
        }
    }
    SetFocus(wnd_data->hListView);
    return TRUE;
}

BOOL on_cmd_qr_code(HWND hWnd) {
    struct main_wnd_data* wnd_data = (struct main_wnd_data*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);
    int nIndex = ListView_GetNextItem(wnd_data->hListView, -1, LVNI_SELECTED);
    struct server_config* config;
    if (nIndex < 0) {
        return FALSE;
    }
    config = retrieve_config_from_list_view(wnd_data->hListView, nIndex);
    if (config == NULL) {
        return FALSE;
    }
    DialogBoxParamW(hInstance, MAKEINTRESOURCEW(IDD_QR_CODE), hWnd, QrCodeDlgProc, (LPARAM)config);
    SetFocus(wnd_data->hListView);
    return TRUE;
}

BOOL on_delete_item(HWND hWnd) {
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);
    struct main_wnd_data* wnd_data = (struct main_wnd_data*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    wchar_t AppName[MAX_PATH] = { 0 };
    int nIndex = ListView_GetNextItem(wnd_data->hListView, -1, LVNI_SELECTED);
    if (nIndex < 0) {
        MessageBeep(0);
        return FALSE;
    }
    LoadStringW(hInstance, IDS_APP_NAME, AppName, ARRAYSIZE(AppName));
    if (IDOK == MessageBoxW(hWnd, L"Delete the item", AppName, MB_OKCANCEL)) {
        ListView_DeleteItem(wnd_data->hListView, nIndex);
    }
    SetFocus(wnd_data->hListView);
    return TRUE;
}

BOOL handle_WM_NOTIFY_from_list_view(HWND hWnd, int ctlID, LPNMHDR pnmHdr)
{
    BOOL msgHandled = FALSE;
    NMLVDISPINFOW* plvdi;
    LPNMLISTVIEW pnmlv;
    LPNMLVKEYDOWN pnmlvkd;
    HWND hWndList;
    struct server_config* config;
    wchar_t tmp[MAX_PATH] = { 0 };
    if (pnmHdr->idFrom != LIST_VIEW_ID) {
        return FALSE;
    }
    hWndList = pnmHdr->hwndFrom;
    switch (pnmHdr->code)
    {
    case LVN_GETDISPINFO:
        plvdi = (NMLVDISPINFOW*)pnmHdr;
        config = (struct server_config*)plvdi->item.lParam;
        on_list_view_notification_get_disp_info(plvdi, config);
        msgHandled = TRUE;
        break;
    case LVN_DELETEITEM:
        pnmlv = (LPNMLISTVIEW)pnmHdr;
        config = (struct server_config*)pnmlv->lParam;
        config_release(config);
        msgHandled = TRUE;
        break;
    case LVN_KEYDOWN:
        pnmlvkd = (LPNMLVKEYDOWN)pnmHdr;
        if (pnmlvkd->wVKey == VK_DELETE) {
            on_delete_item(hWnd);
        }
        if (pnmlvkd->wVKey == VK_ESCAPE) {
            ShowWindowSimple(hWnd, FALSE);
        }
        break;
    case NM_DBLCLK:
    case NM_RETURN:
        msgHandled = TRUE;
        view_server_details(hWnd, hWndList);
        break;
    default:
        break;
    }
    return msgHandled;
}

static void view_server_details(HWND hWnd, HWND hWndList) {
    HINSTANCE hInstance;
    struct server_config* config;
    int nIndex;
    nIndex = ListView_GetNextItem(hWndList, -1, LVNI_SELECTED);
    if (nIndex < 0) {
        return;
    }
    config = retrieve_config_from_list_view(hWndList, nIndex);
    if (config == NULL) {
        return;
    }
    hInstance = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);
    if (IDOK == DialogBoxParamW(hInstance,
        MAKEINTRESOURCEW(IDD_CONFIG_DETAILS),
        hWnd, ConfigDetailsDlgProc, (LPARAM)config))
    {
        ListView_RedrawItems(hWndList, nIndex, nIndex);
    }
    SetFocus(hWndList);
}

static void on_list_view_notification_get_disp_info(NMLVDISPINFOW* plvdi, const struct server_config* config)
{
    LPWSTR pszText = plvdi->item.pszText;
    int cchTextMax = plvdi->item.cchTextMax;
    wchar_t tmp[MAX_PATH] = { 0 };
    switch (plvdi->item.iSubItem)
    {
    case 0:
        // L"Remarks"
        lstrcpynW(pszText, utf8_to_wchar_string(config->remarks, tmp, ARRAYSIZE(tmp)), cchTextMax);
        break;
    case 1:
        // L"Server Address"
        lstrcpynW(pszText, utf8_to_wchar_string(config->remote_host, tmp, ARRAYSIZE(tmp)), cchTextMax);
        break;
    case 2:
        // L"Server Port"
        wsprintfW(pszText, L"%d", (int)config->remote_port);
        break;
    case 3:
        // L"Method"
        lstrcpynW(pszText, utf8_to_wchar_string(config->method, tmp, ARRAYSIZE(tmp)), cchTextMax);
        break;
    case 4:
        // L"Password"
        lstrcpynW(pszText, utf8_to_wchar_string(config->password, tmp, ARRAYSIZE(tmp)), cchTextMax);
        break;
    case 5:
        // L"Protocol"
        lstrcpynW(pszText, utf8_to_wchar_string(config->protocol, tmp, ARRAYSIZE(tmp)), cchTextMax);
        break;
    case 6:
        // L"Protocol Param"
        lstrcpynW(pszText, utf8_to_wchar_string(config->protocol_param, tmp, ARRAYSIZE(tmp)), cchTextMax);
        break;
    case 7:
        // L"Obfs"
        lstrcpynW(pszText, utf8_to_wchar_string(config->obfs, tmp, ARRAYSIZE(tmp)), cchTextMax);
        break;
    case 8:
        // L"Obfs Param"
        lstrcpynW(pszText, utf8_to_wchar_string(config->obfs_param, tmp, ARRAYSIZE(tmp)), cchTextMax);
        break;
    case 9:
        // L"SSRoT Enable"
        lstrcpynW(pszText, config->over_tls_enable ? L"True" : L"False", cchTextMax);
        break;
    case 10:
        // L"SSRoT Domain"
        lstrcpynW(pszText, utf8_to_wchar_string(config->over_tls_server_domain, tmp, ARRAYSIZE(tmp)), cchTextMax);
        break;
    case 11:
        // L"SSRoT Path"
        lstrcpynW(pszText, utf8_to_wchar_string(config->over_tls_path, tmp, ARRAYSIZE(tmp)), cchTextMax);
        break;
    default:
        break;
    }
}

static INT_PTR CALLBACK ConfigDetailsDlgProc(HWND hDlg, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
    static struct server_config* config;
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrW(hDlg, GWLP_HINSTANCE);
    HICON hIconApp;
    switch (uMessage)
    {
    case WM_INITDIALOG:
        config_dlg_init(hDlg);
        RestoreWindowPos(hDlg);
        config = (struct server_config*)lParam;
        if (config) {
            load_config_to_dlg(hDlg, config);
        }
        hIconApp = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SSRWIN));
        SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIconApp);
        return TRUE;
    case WM_COMMAND:
        switch (wParam)
        {
        case IDOK:
            if (config) {
                save_dlg_to_config(hDlg, config);
            }
            // fall through.
        case IDCANCEL:
            config = NULL;
            EndDialog(hDlg, wParam);
            break;
        }
        return TRUE;
    }
    return FALSE;
}

static INT_PTR CALLBACK OptionsDlgProc(HWND hDlg, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
    static struct main_wnd_data* wnd_data = NULL;
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrW(hDlg, GWLP_HINSTANCE);
    HICON hIconApp;
    switch (uMessage)
    {
    case WM_INITDIALOG:
        RestoreWindowPos(hDlg);
        wnd_data = (struct main_wnd_data*)lParam;
        hIconApp = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SSRWIN));
        SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIconApp);

        CheckDlgButton(hDlg, IDC_CHK_AUTO_RUN, wnd_data->auto_run);
        CheckDlgButton(hDlg, IDC_CHK_AUTO_CONN, wnd_data->auto_connect);
        SetDlgItemTextA(hDlg, IDC_EDT_SSR_HOST, wnd_data->ssr_listen_host);
        SetDlgItemInt(hDlg, IDC_EDT_SSR_PORT, wnd_data->ssr_listen_port, FALSE);
        SetDlgItemInt(hDlg, IDC_EDT_PRIVOXY_PORT, wnd_data->privoxy_listen_port, FALSE);
        SetDlgItemInt(hDlg, IDC_EDT_DELAY_MS, wnd_data->delay_quit_ms, FALSE);

        return TRUE;
    case WM_COMMAND:
        switch (wParam)
        {
        case IDC_BTN_RESET:
            CheckDlgButton(hDlg, IDC_CHK_AUTO_RUN, FALSE);
            CheckDlgButton(hDlg, IDC_CHK_AUTO_CONN, FALSE);
            SetDlgItemTextW(hDlg, IDC_EDT_SSR_HOST, L"127.0.0.1");
            SetDlgItemTextW(hDlg, IDC_EDT_SSR_PORT, L"0");
            SetDlgItemInt(hDlg, IDC_EDT_PRIVOXY_PORT, PRIVOXY_LISTEN_PORT, FALSE);
            SetDlgItemInt(hDlg, IDC_EDT_DELAY_MS, SSR_DELAY_QUIT_MIN, FALSE);
            break;
        case IDOK:
            assert(wnd_data);
            wnd_data->auto_run = IsDlgButtonChecked(hDlg, IDC_CHK_AUTO_RUN);
            wnd_data->auto_connect = IsDlgButtonChecked(hDlg, IDC_CHK_AUTO_CONN);
            GetDlgItemTextA(hDlg, IDC_EDT_SSR_HOST, wnd_data->ssr_listen_host, sizeof(wnd_data->ssr_listen_host));
            wnd_data->ssr_listen_port = GetDlgItemInt(hDlg, IDC_EDT_SSR_PORT, NULL, FALSE);
            wnd_data->privoxy_listen_port = GetDlgItemInt(hDlg, IDC_EDT_PRIVOXY_PORT, NULL, FALSE);
            wnd_data->delay_quit_ms = GetDlgItemInt(hDlg, IDC_EDT_DELAY_MS, NULL, FALSE);
            // fall through.
        case IDCANCEL:
            wnd_data = NULL;
            EndDialog(hDlg, wParam);
            break;
        }
        return TRUE;
    }
    return FALSE;
}

static void config_dlg_init(HWND hDlg)
{
    wchar_t tmp[MAX_PATH] = { 0 };

    HWND hMethod = GetDlgItem(hDlg, IDC_CMB_ENCRYPTION);
    HWND hProtocol = GetDlgItem(hDlg, IDC_CMB_PROTOCOL);
    HWND hObfs = GetDlgItem(hDlg, IDC_CMB_OBFS);
    HWND hOtEnable = GetDlgItem(hDlg, IDC_STC_OT_ENABLE);

    enum ss_cipher_type iterMethod = ss_cipher_none;
    enum ssr_protocol iterProtocol = ssr_protocol_origin;
    enum ssr_obfs iterObfs = ssr_obfs_plain;

    for (iterMethod = ss_cipher_none; iterMethod < ss_cipher_max; ++iterMethod) {
        const char* name = ss_cipher_name_of_type(iterMethod);
        if (name) {
            utf8_to_wchar_string(name, tmp, ARRAYSIZE(tmp));
            ComboBox_AddString(hMethod, tmp);
        }
    }

    for (iterProtocol = ssr_protocol_origin; iterProtocol < ssr_protocol_max; ++iterProtocol) {
        const char* name = ssr_protocol_name_of_type(iterProtocol);
        if (name) {
            utf8_to_wchar_string(name, tmp, ARRAYSIZE(tmp));
            ComboBox_AddString(hProtocol, tmp);
        }
    }

    for (iterObfs = ssr_obfs_plain; iterObfs < ssr_obfs_max; ++iterObfs) {
        const char* name = ssr_obfs_name_of_type(iterObfs);
        if (name) {
            utf8_to_wchar_string(name, tmp, ARRAYSIZE(tmp));
            ComboBox_AddString(hObfs, tmp);
        }
    }

    CheckableGroupBox_SubclassWindow(hOtEnable);
}

static void load_config_to_dlg(HWND hDlg, const struct server_config* config)
{
    wchar_t tmp[MAX_PATH] = { 0 };

    utf8_to_wchar_string(config->remarks, tmp, ARRAYSIZE(tmp));
    SetWindowTextW(GetDlgItem(hDlg, IDC_EDT_REMARKS), tmp);

    utf8_to_wchar_string(config->remote_host, tmp, ARRAYSIZE(tmp));
    SetWindowTextW(GetDlgItem(hDlg, IDC_EDT_SERVER_ADDR), tmp);

    wsprintfW(tmp, L"%d", (int)config->remote_port);
    SetWindowTextW(GetDlgItem(hDlg, IDC_EDT_SERVER_PORT), tmp);

    utf8_to_wchar_string(config->password, tmp, ARRAYSIZE(tmp));
    SetWindowTextW(GetDlgItem(hDlg, IDC_EDT_PASSWORD), tmp);

    utf8_to_wchar_string(config->method, tmp, ARRAYSIZE(tmp));
    combo_box_set_cur_sel(GetDlgItem(hDlg, IDC_CMB_ENCRYPTION), tmp);

    utf8_to_wchar_string(config->protocol, tmp, ARRAYSIZE(tmp));
    combo_box_set_cur_sel(GetDlgItem(hDlg, IDC_CMB_PROTOCOL), tmp);

    utf8_to_wchar_string(config->protocol_param, tmp, ARRAYSIZE(tmp));
    SetWindowTextW(GetDlgItem(hDlg, IDC_EDT_PROTOCOL_PARAM), tmp);

    utf8_to_wchar_string(config->obfs, tmp, ARRAYSIZE(tmp));
    combo_box_set_cur_sel(GetDlgItem(hDlg, IDC_CMB_OBFS), tmp);

    utf8_to_wchar_string(config->obfs_param, tmp, ARRAYSIZE(tmp));
    SetWindowTextW(GetDlgItem(hDlg, IDC_EDT_OBFS_PARAM), tmp);

    Button_SetCheck(GetDlgItem(hDlg, IDC_STC_OT_ENABLE), config->over_tls_enable ? TRUE : FALSE);

    utf8_to_wchar_string(config->over_tls_server_domain, tmp, ARRAYSIZE(tmp));
    SetWindowTextW(GetDlgItem(hDlg, IDC_EDT_OT_DOMAIN), tmp);

    utf8_to_wchar_string(config->over_tls_path, tmp, ARRAYSIZE(tmp));
    SetWindowTextW(GetDlgItem(hDlg, IDC_EDT_OT_PATH), tmp);
}

static void save_dlg_to_config(HWND hDlg, struct server_config* config)
{
    wchar_t w_tmp[MAX_PATH] = { 0 };
    char a_tmp[MAX_PATH] = { 0 };

    HWND hRemarks = GetDlgItem(hDlg, IDC_EDT_REMARKS);
    HWND hServerAddr = GetDlgItem(hDlg, IDC_EDT_SERVER_ADDR);
    HWND hServerPort = GetDlgItem(hDlg, IDC_EDT_SERVER_PORT);
    HWND hPassword = GetDlgItem(hDlg, IDC_EDT_PASSWORD);
    HWND hMethod = GetDlgItem(hDlg, IDC_CMB_ENCRYPTION);
    HWND hProtocol = GetDlgItem(hDlg, IDC_CMB_PROTOCOL);
    HWND hProtocolParam = GetDlgItem(hDlg, IDC_EDT_PROTOCOL_PARAM);
    HWND hObfs = GetDlgItem(hDlg, IDC_CMB_OBFS);
    HWND hObfsParam = GetDlgItem(hDlg, IDC_EDT_OBFS_PARAM);
    HWND hOtEnable = GetDlgItem(hDlg, IDC_STC_OT_ENABLE);
    HWND hOtDomain = GetDlgItem(hDlg, IDC_EDT_OT_DOMAIN);
    HWND hOtPath = GetDlgItem(hDlg, IDC_EDT_OT_PATH);

    GetWindowTextW(hRemarks, w_tmp, ARRAYSIZE(w_tmp));
    wchar_string_to_utf8(w_tmp, a_tmp, ARRAYSIZE(a_tmp));
    string_safe_assign(&config->remarks, a_tmp);

    GetWindowTextW(hServerAddr, w_tmp, ARRAYSIZE(w_tmp));
    wchar_string_to_utf8(w_tmp, a_tmp, ARRAYSIZE(a_tmp));
    string_safe_assign(&config->remote_host, a_tmp);

    GetWindowTextA(hServerPort, a_tmp, ARRAYSIZE(a_tmp));
    config->remote_port = (unsigned short)strtol(a_tmp, NULL, 10);

    GetWindowTextW(hPassword, w_tmp, ARRAYSIZE(w_tmp));
    wchar_string_to_utf8(w_tmp, a_tmp, ARRAYSIZE(a_tmp));
    string_safe_assign(&config->password, a_tmp);

    GetWindowTextA(hMethod, a_tmp, ARRAYSIZE(a_tmp));
    string_safe_assign(&config->method, a_tmp);

    GetWindowTextA(hProtocol, a_tmp, ARRAYSIZE(a_tmp));
    string_safe_assign(&config->protocol, a_tmp);

    GetWindowTextW(hProtocolParam, w_tmp, ARRAYSIZE(w_tmp));
    wchar_string_to_utf8(w_tmp, a_tmp, ARRAYSIZE(a_tmp));
    string_safe_assign(&config->protocol_param, a_tmp);

    GetWindowTextA(hObfs, a_tmp, ARRAYSIZE(a_tmp));
    string_safe_assign(&config->obfs, a_tmp);

    GetWindowTextW(hObfsParam, w_tmp, ARRAYSIZE(w_tmp));
    wchar_string_to_utf8(w_tmp, a_tmp, ARRAYSIZE(a_tmp));
    string_safe_assign(&config->obfs_param, a_tmp);

    config->over_tls_enable = Button_GetCheck(hOtEnable) ? true : false;

    GetWindowTextW(hOtDomain, w_tmp, ARRAYSIZE(w_tmp));
    wchar_string_to_utf8(w_tmp, a_tmp, ARRAYSIZE(a_tmp));
    string_safe_assign(&config->over_tls_server_domain, a_tmp);

    GetWindowTextW(hOtPath, w_tmp, ARRAYSIZE(w_tmp));
    wchar_string_to_utf8(w_tmp, a_tmp, ARRAYSIZE(a_tmp));
    string_safe_assign(&config->over_tls_path, a_tmp);
}

static void combo_box_set_cur_sel(HWND hCombo, const wchar_t* cur_sel)
{
    int index;
    wchar_t lbstr[MAX_PATH] = { 0 };
    for (index = 0; index < ComboBox_GetCount(hCombo); ++index) {
        ComboBox_GetLBText(hCombo, index, lbstr);
        if (lstrcmpW(lbstr, cur_sel) == 0) {
            ComboBox_SetCurSel(hCombo, index);
            break;
        }
    }
}

static void save_config_to_file(HWND hListView, const char* settings_file) {
    struct config_json_saver* saver = config_json_saver_create(settings_file);
    int index;
    struct server_config* config;
    for (index = 0; index < ListView_GetItemCount(hListView); ++index) {
        config = retrieve_config_from_list_view(hListView, index);
        config_json_saver_add_item(saver, config);
    }
    config_json_saver_write_file(saver);
    config_json_saver_release(saver);
}

static struct server_config* retrieve_config_from_list_view(HWND hListView, int index) {
    struct server_config* config = NULL;
    LVITEMW item = { 0 };
    item.mask = LVIF_PARAM;
    item.iItem = index;
    if (ListView_GetItem(hListView, &item)) {
        config = (struct server_config*)item.lParam;
    }
    return config;
}

static void json_config_iter(struct server_config* config, void* p) {
    struct json_iter_data* iter_data = (struct json_iter_data*)p;
    InsertListViewItem(iter_data->wnd_data->hListView, iter_data->index, config);
    ++iter_data->index;
}

static char* retrieve_string_from_clipboard(HWND hWnd, void* (*allocator)(size_t)) {
    char* result = NULL;
    UINT format = 0;
    if (allocator == NULL) {
        return NULL;
    }
    if (!OpenClipboard(hWnd)) {
        return NULL;
    }
    if (IsClipboardFormatAvailable(CF_TEXT)) {
        format = CF_TEXT;
    }
    else if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        format = CF_UNICODETEXT;
    }
    if (format != 0) {
        HGLOBAL hglb = GetClipboardData(format);
        if (hglb != NULL) {
            char* lptstr = (char*)GlobalLock(hglb);
            if (lptstr != NULL) {
                if (format == CF_UNICODETEXT) {
                    size_t len = (lstrlenW((wchar_t*)lptstr) + 1) * 2;
                    char* tmp = (char*)calloc(len, sizeof(*tmp));
                    if (tmp) {
                        lptstr = wchar_string_to_utf8((wchar_t*)lptstr, tmp, len);
                    }
                    else {
                        lptstr = NULL;
                    }
                }
                if (lptstr) {
                    result = (char*)allocator(lstrlenA(lptstr) + 1);
                    if (result) {
                        lstrcpyA(result, lptstr);
                    }
                }
                if (format == CF_UNICODETEXT) {
                    free(lptstr);
                }
                GlobalUnlock(hglb);
            }
        }
    }
    CloseClipboard();
    return result;
}

static void SetFocusToPreviousInstance(const wchar_t* windowClass, const wchar_t* windowCaption) {
    HWND hWnd = FindWindowW(windowClass, windowCaption);
    if (hWnd != NULL) {
        HWND hPopupWnd = GetLastActivePopup(hWnd);
        if (hPopupWnd != NULL && IsWindowEnabled(hPopupWnd)) {
            hWnd = hPopupWnd;
        }
        SetForegroundWindow(hWnd);
        if (IsIconic(hWnd) || FALSE == IsWindowVisible(hWnd)) {
            ShowWindow(hWnd, SW_RESTORE);
        }
    }
}
