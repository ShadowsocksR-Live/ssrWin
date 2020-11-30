#include <Windows.h>
#include <WindowsX.h>
#include <assert.h>
#include "resource.h"

HINSTANCE hinst;

int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
    MSG msg = { 0 };
    BOOL bRet = FALSE;
    WNDCLASSW wc = { 0 };
    UNREFERENCED_PARAMETER(lpszCmdLine);

    hinst = hInstance;

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
