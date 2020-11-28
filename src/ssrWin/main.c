#include <Windows.h>
#include "resource.h"

HINSTANCE hinst;
HWND hwndMain;

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
    MSG msg = { 0 };
    BOOL bRet = FALSE;
    WNDCLASSW wc = { 0 };
    UNREFERENCED_PARAMETER(lpszCmdLine);

    hinst = hInstance;

    wc.style = 0;
    wc.lpfnWndProc = (WNDPROC)WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hinst;
    wc.hIcon = LoadIconW((HINSTANCE)hinst, (LPCWSTR)IDI_SSRWIN);
    wc.hCursor = LoadCursorW((HINSTANCE)NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName = L"MainMenu";
    wc.lpszClassName = L"MainWndClass";
    if (!RegisterClassW(&wc)) {
        return FALSE;
    }

    hwndMain = CreateWindowW(L"MainWndClass", L"Sample",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, (HWND)NULL,
        (HMENU)NULL, hinst, (LPVOID)NULL);
    if (!hwndMain) {
        return FALSE;
    }

    ShowWindow(hwndMain, nCmdShow);
    UpdateWindow(hwndMain);

    while ((bRet = GetMessageW(&msg, NULL, 0, 0)) != FALSE) {
        if (bRet == -1) {
            // handle the error and possibly exit
        }
        else {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT paintStruct;
    HDC hDC;
    wchar_t string[] = L"Hello, World!";
    switch (message)
    {
    case WM_CREATE:
        return 0;
        break;
    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;
        break;
    case WM_PAINT:
        hDC = BeginPaint(hwnd, &paintStruct);
        SetTextColor(hDC, (COLORREF)(0x00FF0000));
        TextOutW(hDC, 150, 150, string, lstrlenW(string));
        EndPaint(hwnd, &paintStruct);
        return 0;
        break;
    default:
        break;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}
