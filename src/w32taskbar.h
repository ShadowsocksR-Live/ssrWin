#ifndef W32TASKBAR_H_INCLUDED
#define W32TASKBAR_H_INCLUDED
/*********************************************************************
*
* File        :  $Source: w32taskbar.h,v $
*
* Purpose     :  Functions for creating, setting and destroying the
*                workspace tray icon
*
*********************************************************************/

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

extern HWND CreateTrayWindow(HINSTANCE hInstance, HMENU hMenuTray, HWND hwndReciever);
extern void TraySetDblClkCallback(HWND hwnd, void(*cb)(void*p), void*p);
extern void TraySetClickCallback(HWND hwnd, void(*cb)(void*p), void*p);
extern BOOL TrayAddIcon(HWND hwnd, UINT uID, HICON hicon, const wchar_t* pszToolTip);
extern BOOL TraySetIcon(HWND hwnd, UINT uID, HICON hicon);
extern BOOL TrayDeleteIcon(HWND hwnd, UINT uID);

#ifdef __cplusplus
}
#endif

#endif /* W32TASKBAR_H_INCLUDED */
