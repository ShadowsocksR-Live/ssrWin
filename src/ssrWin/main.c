#include <Windows.h>
#include <WindowsX.h>
#include <tchar.h>
#include <CommCtrl.h>
#include <assert.h>
#include "resource.h"
#include "w32taskbar.h"
#include <privoxyexports.h>
#include <exe_file_path.h>
#include <ssr_executive.h>
#include <ssr_cipher_names.h>
#include "settings_json.h"
#include "utf8_to_wchar.h"
#include "checkablegroupbox.h"

HWND hTrayWnd = NULL;

struct main_wnd_data {
    HWND hMainDlg;
    HWND hListView;
    char settings_file[MAX_PATH];
};

#define TRAY_ICON_ID 1
#define LIST_VIEW_ID 55

ATOM RegisterWndClass(HINSTANCE hInstance, const wchar_t* szWindowClass);
HWND InitInstance(HINSTANCE hInstance, const wchar_t* wndClass, const wchar_t* title, int nCmdShow);
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
static void TrayClickCb(void* p);
static void ShowWindowSimple(HWND hWnd, BOOL bShow);
static void RestoreWindowPos(HWND hWnd);
static HWND create_list_view(HWND hwndParent, HINSTANCE hinstance);
BOOL InitListViewColumns(HWND hWndListView);
BOOL InsertListViewItem(HWND hWndListView, int index, struct server_config* config);
BOOL handle_WM_NOTIFY_from_list_view(HWND hWnd, WPARAM wParam, LPARAM lParam);
static INT_PTR CALLBACK ConfigDetailsDlgProc(HWND hDlg, UINT uMessage, WPARAM wParam, LPARAM lParam);
static void config_dlg_init(HWND hDlg);
static void load_config_to_dlg(HWND hDlg, const struct server_config* config);
static void save_dlg_to_config(HWND hDlg, struct server_config* config);
static void combo_box_set_cur_sel(HWND hCombo, const wchar_t* cur_sel);

static void json_config_iter(struct server_config* config, void* p);

int PASCAL wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpszCmdLine, int nCmdShow)
{
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

struct json_iter_data {
    struct main_wnd_data* wnd_data;
    int index;
};

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    struct main_wnd_data* wnd_data = NULL;
    BOOL passToNext = TRUE;
    LPCREATESTRUCTW pcs = NULL;
    wnd_data = (struct main_wnd_data*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    switch (message)
    {
    case WM_CREATE:
        assert(wnd_data == NULL);
        wnd_data = (struct main_wnd_data*)calloc(1, sizeof(*wnd_data));
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)wnd_data);
        wnd_data->hMainDlg = hWnd;
        pcs = (LPCREATESTRUCTW)lParam;
        RestoreWindowPos(hWnd);
        wnd_data->hListView = create_list_view(hWnd, pcs->hInstance);
        InitListViewColumns(wnd_data->hListView);
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
        DestroyWindow(wnd_data->hListView);
        TrayDeleteIcon(hTrayWnd, TRAY_ICON_ID);
        PostQuitMessage(0);
        passToNext = FALSE;
        free(wnd_data);
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
    case WM_NOTIFY:
        passToNext = (handle_WM_NOTIFY_from_list_view(hWnd, wParam, lParam) == FALSE);
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

BOOL handle_WM_NOTIFY_from_list_view(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    BOOL msgHandled = FALSE;
    NMLVDISPINFOW* plvdi;
    LPNMLISTVIEW pnmlv;
    LPNMHDR lpnmHdr = (LPNMHDR)lParam;
    HWND hWndList;
    struct server_config* config;
    int cchTextMax;
    int nIndex;
    LPWSTR pszText;
    wchar_t tmp[MAX_PATH] = { 0 };
    if (lpnmHdr->idFrom != LIST_VIEW_ID) {
        return FALSE;
    }
    hWndList = lpnmHdr->hwndFrom;
    switch (lpnmHdr->code)
    {
    case LVN_GETDISPINFO:
        plvdi = (NMLVDISPINFOW*)lParam;
        config = (struct server_config*)plvdi->item.lParam;
        pszText = plvdi->item.pszText;
        cchTextMax = plvdi->item.cchTextMax;
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
        msgHandled = TRUE;
        break;
    case LVN_DELETEITEM:
        pnmlv = (LPNMLISTVIEW)lParam;
        config = (struct server_config*)pnmlv->lParam;
        config_release(config);
        msgHandled = TRUE;
        break;
    case NM_DBLCLK:
        msgHandled = TRUE;
        nIndex = ListView_GetNextItem(hWndList, -1, LVNI_SELECTED);
        if (nIndex >= 0) {
            HINSTANCE hInstance;
            LVITEMW item = { 0 };
            item.mask = LVIF_PARAM;
            item.iItem = nIndex;
            if (ListView_GetItem(hWndList, &item) == FALSE) {
                break;
            }
            config = (struct server_config*)item.lParam;
            if (config == NULL) {
                break;
            }
            hInstance = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);
            if (IDOK == DialogBoxParamW(hInstance,
                MAKEINTRESOURCEW(IDD_CONFIG_DETAILS),
                hWnd, ConfigDetailsDlgProc, (LPARAM)config))
            {
                ListView_RedrawItems(hWndList, nIndex, nIndex);
            }
        }
        break;
    default:
        break;
    }
    return msgHandled;
}

static INT_PTR CALLBACK ConfigDetailsDlgProc(HWND hDlg, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
    static struct server_config* config;
    switch (uMessage)
    {
    case WM_INITDIALOG:
        config_dlg_init(hDlg);
        RestoreWindowPos(hDlg);
        config = (struct server_config*)lParam;
        if (config) {
            load_config_to_dlg(hDlg, config);
        }
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

static void json_config_iter(struct server_config* config, void* p) {
    struct json_iter_data* iter_data = (struct json_iter_data*)p;
    InsertListViewItem(iter_data->wnd_data->hListView, iter_data->index, config);
    ++iter_data->index;
}
