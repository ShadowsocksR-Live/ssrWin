#include <Windows.h>
#include <WindowsX.h>
#include <commctrl.h>
#include <assert.h>

#include "checkablegroupbox.h"

#pragma comment(lib, "Comctl32.lib")

#define ID_CHECKBOX     0xFFFE
#define LEFT_OFFSET     12
#define CHECKBOX_SIZE   16

typedef struct CheckableGroupBoxData {
    HWND hCheckBox;
} CheckableGroupBoxData;

static void CheckableGroupBox_OnClicked(HWND hWnd);
static int CheckableGroupBox_GetCheck(HWND hWnd);
static int CheckableGroupBox_SetCheck(HWND hWnd, int nCheck);
static void CheckableGroupBox_EnableControls(HWND hWnd, BOOL bActivate);
static int CheckableGroupBox_MoveTitle(HWND hWnd);
static void CheckableGroupBox_OnEnable(HWND hWnd, BOOL bEnable);

LRESULT CALLBACK CheckableGroupBoxProc(HWND hWnd, UINT uMsg, WPARAM wParam,
    LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    LRESULT rs = 0;
    CheckableGroupBoxData* data = (CheckableGroupBoxData*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        HWND hChkBox = GET_WM_COMMAND_HWND(wParam, lParam);
        int chkBoxID = (int)GET_WM_COMMAND_ID(wParam, lParam);
        if (chkBoxID == ID_CHECKBOX) {
            assert(data->hCheckBox == hChkBox);
            CheckableGroupBox_OnClicked(hWnd);
        }
        break;
    }
    case BM_GETCHECK:
        return CheckableGroupBox_GetCheck(hWnd);
        break;
    case BM_SETCHECK:
        CheckableGroupBox_SetCheck(hWnd, (int)wParam);
        return 0;
        break;
    case WM_ENABLE:
        CheckableGroupBox_OnEnable(hWnd, (BOOL)wParam);
        break;
    case WM_SETFOCUS:
        rs = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        SetFocus(data->hCheckBox);
        return 0;
    case WM_NCDESTROY:
        RemoveWindowSubclass(hWnd, CheckableGroupBoxProc, 0);
        free(data);
        break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

static int CWnd_GetCheck(HWND hWnd) {
    assert(IsWindow(hWnd));
    return (int)SendMessage(hWnd, BM_GETCHECK, 0, 0);
}

static void CWnd_SetCheck(HWND hWnd, int nCheck) {
    assert(IsWindow(hWnd));
    SendMessage(hWnd, BM_SETCHECK, (WPARAM)nCheck, 0);
}

static int CheckableGroupBox_GetCheck(HWND hWnd) {
    CheckableGroupBoxData* data = (CheckableGroupBoxData*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    assert(IsWindow(hWnd));
    return (int)CWnd_GetCheck(data->hCheckBox);
}

static HFONT CWnd_GetFont(HWND hWnd) {
    assert(IsWindow(hWnd));
    return (HFONT)SendMessage(hWnd, WM_GETFONT, 0, 0);
}

static void CWnd_SetFont(HWND hWnd, HFONT pFont, BOOL bRedraw) {
    assert(IsWindow(hWnd));
    SendMessage(hWnd, WM_SETFONT, (WPARAM)pFont, bRedraw);
}

static BOOL _Afx_ModifyStyle(HWND hWnd, int nStyleOffset, DWORD dwRemove, DWORD dwAdd, UINT nFlags)
{
    DWORD dwStyle, dwNewStyle;
    assert(hWnd != NULL);
    dwStyle = GetWindowLong(hWnd, nStyleOffset);
    dwNewStyle = (dwStyle & ~dwRemove) | dwAdd;
    if (dwStyle == dwNewStyle) {
        return FALSE;
    }
    SetWindowLong(hWnd, nStyleOffset, dwNewStyle);
    if (nFlags != 0) {
        SetWindowPos(hWnd, NULL, 0, 0, 0, 0,
            SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | nFlags);
    }
    return TRUE;
}

static void CheckableGroupBox_OnClicked(HWND hWnd) {
    CheckableGroupBox_EnableControls(hWnd, CheckableGroupBox_GetCheck(hWnd));
    SendMessage(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(hWnd), BN_CLICKED), (LPARAM)hWnd);
}

static void CheckableGroupBox_EnableControls(HWND hWnd, BOOL bActivate)
{
    // Enable/Disable all controls that fall whole inside of me.
    // Brute force: check every control in the dialog.
    RECT rCtrl, rTest = { 0 };
    RECT rGroup;

    HWND pParent = GetParent(hWnd);
    HWND pCtrl = GetWindow(pParent, GW_CHILD); // Gets first child

    GetWindowRect(hWnd, &rGroup);

    while (IsWindow(pCtrl)) {
        // Don't do myself !!
        if (pCtrl == hWnd) {
            pCtrl = GetWindow(pCtrl, GW_HWNDNEXT);
            continue;
        }

        /*
        #ifndef IDC_STATIC
        #define IDC_STATIC      (-1)
        #endif

        if ((short)GetDlgCtrlID(pCtrl) == IDC_STATIC) {
            pCtrl = GetWindow(pCtrl, GW_HWNDNEXT);
            continue;
        }
        */

        GetWindowRect(pCtrl, &rCtrl);

        if (IntersectRect(&rTest, &rGroup, &rCtrl)) {
            EnableWindow(pCtrl, bActivate);
        }
        pCtrl = GetWindow(pCtrl, GW_HWNDNEXT);
    }
}

BOOL CheckableGroupBox_SubclassWindow(HWND hWnd) {
    // Make sure that this control has the BS_ICON style set.
    // If not, it behaves very strangely:
    //  It erases itself if the user TABs to controls in the dialog,
    //  unless the user first clicks it. Very strange!!
    wchar_t strTitle[MAX_PATH] = { 0 };
    wchar_t szClassName[MAX_PATH] = { 0 };
    LONG lStyle;
    int nWidth;
    RECT r = { 0 };
    HWND hCheckBox = NULL;
    CheckableGroupBoxData* data = NULL;

    if (IsWindow(hWnd) == FALSE) {
        assert(FALSE);
        return FALSE;
    }

    // get class name
    GetClassNameW(hWnd, szClassName, ARRAYSIZE(szClassName));
    // get window style
    lStyle = GetWindowLongW(hWnd, GWL_STYLE);
    if ((_wcsicmp(szClassName, L"Button") != 0) || ((lStyle & BS_GROUPBOX) != BS_GROUPBOX)) {
        assert(FALSE);
        return FALSE;
    }

    _Afx_ModifyStyle(hWnd, GWL_STYLE, 0, BS_ICON | WS_TABSTOP | WS_GROUP, 0);

    GetWindowTextW(hWnd, strTitle, ARRAYSIZE(strTitle));

    nWidth = CheckableGroupBox_MoveTitle(hWnd);

    GetWindowRect(hWnd, &r);
    ScreenToClient(hWnd, (LPPOINT)&r);
    ScreenToClient(hWnd, ((LPPOINT)&r) + 1);

    OffsetRect(&r, LEFT_OFFSET, 0);
    r.bottom = r.top + CHECKBOX_SIZE;
    r.right = r.left + CHECKBOX_SIZE + nWidth + 5;

    hCheckBox = CreateWindowW(L"BUTTON", strTitle,
        WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        r.left, r.top, r.right - r.left, r.bottom - r.top,
        hWnd, (HMENU)ID_CHECKBOX,
        NULL,
        NULL);

    if (hCheckBox == NULL) {
        assert(FALSE);
        return FALSE;
    }

    CWnd_SetFont(hCheckBox, CWnd_GetFont(hWnd), TRUE);
    ShowWindow(hCheckBox, SW_SHOW);

    data = (CheckableGroupBoxData*)calloc(1, sizeof(*data));
    assert(data);
    data->hCheckBox = hCheckBox;

    SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)data);

    SetWindowSubclass(hWnd, CheckableGroupBoxProc, 0, 0);

    return TRUE;
}

static int CheckableGroupBox_MoveTitle(HWND hWnd)
{
    // The group box title needs to be erased, but I need to keep the border away from the check box text.
    // I create a string of spaces (' ') that is the same length as the title was plus the size of the check box.
    // Plus a little more.
    HDC dc;
    HFONT pOldFont;
    SIZE czText = { 0 };
    int nRet, nTarget, count;

    TCHAR strOldTitle[256] = { 0 }, strNewTitle[256] = { 0 };
    GetWindowText(hWnd, strOldTitle, ARRAYSIZE(strOldTitle));

    dc = GetDC(hWnd);

    pOldFont = (HFONT)SelectObject(dc, CWnd_GetFont(hWnd));

    GetTextExtentPoint32(dc, strOldTitle, (int)lstrlen(strOldTitle), &czText);

    nRet = czText.cx;
    nTarget = czText.cx + CHECKBOX_SIZE + 10;

    count = 0;
    while (czText.cx < nTarget) {
        strNewTitle[count++] = ' ';
        assert(count < ARRAYSIZE(strNewTitle));
        ZeroMemory(&czText, sizeof(czText));
        GetTextExtentPoint32(dc, strNewTitle, (int)lstrlen(strNewTitle), &czText);
    }

    SelectObject(dc, pOldFont);

    SetWindowText(hWnd, strNewTitle);

    ReleaseDC(hWnd, dc);
    return nRet;
}

static int CheckableGroupBox_SetCheck(HWND hWnd, int nCheck)
{
    CheckableGroupBoxData* data = (CheckableGroupBoxData*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    CWnd_SetCheck(data->hCheckBox, nCheck);
    CheckableGroupBox_EnableControls(hWnd, nCheck == 0 ? FALSE : TRUE);
    return 0;
}

static void CheckableGroupBox_OnEnable(HWND hWnd, BOOL bEnable)
{
    CheckableGroupBoxData* data = (CheckableGroupBoxData*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    EnableWindow(data->hCheckBox, bEnable);
    CheckableGroupBox_EnableControls(hWnd, bEnable && CheckableGroupBox_GetCheck(hWnd));
}
