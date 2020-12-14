#include <Windows.h>

#include "capture_screen.h"

HBITMAP capture_screen(void) {
    HDC hdc = GetDC(NULL); // get the desktop device context
    HDC hDest = CreateCompatibleDC(hdc); // create a device context to use yourself

    // get the height and width of the screen
    int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);

    // create a bitmap
    HBITMAP hbDesktop = CreateCompatibleBitmap( hdc, width, height);

    // use the previously created device context with the bitmap
    SelectObject(hDest, hbDesktop);

    // copy from the desktop device context to the bitmap device context
    // call this once per 'frame'
    BitBlt(hDest, 0,0, width, height, hdc, 0, 0, SRCCOPY);

    // after the recording is done, release the desktop context you got..
    ReleaseDC(NULL, hdc);

    // ..delete the bitmap you were using to capture frames..
    //DeleteObject(hbDesktop);

    // ..and delete the context you created
    DeleteDC(hDest);

    return hbDesktop;
}
