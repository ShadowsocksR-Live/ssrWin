/*
*                        EASY SPLITTER v2.0
*            Splitter window custom control in Pelles-C
*                     By L. D. Blake 2005, 2006
*              This is open source, freeware ... enjoy!
*      Please forward comments and suggestions to:  fware@start.ca
*
* http://www.catch22.net/tuts/win32/splitter-windows
*/

// Include file

#ifndef __EASY_SPLIT_H__
#define __EASY_SPLIT_H__

#define WIN32_DEFAULT_LIBS
//#pragma comment(lib,"easysplit.lib")

#include <windows.h>

// Constants

// messages you can send to a splitter
#define ESM_UNDO        WM_USER  + 1090 // undo move
#define ESM_STOP        ESM_UNDO + 1    // abort mouse capture
#define ESM_SETCURSOR   ESM_UNDO + 2    // change cursor
#define ESM_SETCOLORS   ESM_UNDO + 3    // set line and bg color 
#define ESM_SETBORDER   ESM_UNDO + 4    // set movement limits
#define ESM_SETTRACKING ESM_UNDO + 5    // enable/disable mouse tracking 
#define ESM_SETKBMOVE   ESM_UNDO + 6    // enable/disable keyboard moves
#define ESM_SETLINE     ESM_UNDO + 7    // enable/disable center line
#define ESM_SETDOCKING  ESM_UNDO + 9    // enable/disable magnetic borders
#define ESM_GETPOS      ESM_UNDO + 11   // return position of splitter
#define ESM_SETPOS      ESM_UNDO + 12   // force the splitter location

/*    Message Data for use with the SendMessage api call
--------------------------------------------------------------------------
Message               WParam                    LParam
--------------------------------------------------------------------------
ESM_UNDO              0                         0
ESM_STOP              0                         0
ESM_SETCURSOR         0                         Cursor Handle
ESM_SETCOLORS         Foreground Color          Background Color
ESM_SETBORDER         Border width in pixels    Border Constant
ESM_SETDOCKING        Width of magnetic border  TRUE = ON, FALSE = off
ESM_SETTRACKING       0                         TRUE = on, FALSE = off
ESM_SETKBMOVE         0                         TRUE = on, False = off
ESM_SETLINE           0                         TRUE = on, FALSE = off
ESM_SETPOS            0                         Splitter Position
ESM_GETPOS            0                         0

ESM_UNDO, ESM_SETPOS and ESM_GETPOS return the current splitter position
relative to the parent window, in client coordinates.  All other messages
return 0.

WARNING:  Easy Splitter emits WM_SIZE messages whenever it's position
          is changed.  To move a spiltter from inside a WM_SIZE handler
          DO NOT use ESM_SETPOS or ES_UNDO. This will cause a message
          race and may cause system lockups. Use the MoveWindow or
          SetWindowPos api calls instead.  ESM_SETPOS and ESM_UNDO are
          safe when used ouside of WM_SIZE handlers.
-------------------------------------------------------------------------
*/

// ESM_SETBORDER Constants
#define ESB_TOP    0          // sets no-move zones for splitters
#define ESB_BOTTOM 1          // WPARAM = border constant
#define ESB_LEFT   2          // LPARAM = border width
#define ESB_RIGHT  3          // LPARAM = 0 means no border
#define ESB_ALL    4

// splitter styles you can mix with other window styles
#define ES_HORZ       0x00000000  // horizontal splitter
#define ES_VERT       0x00000001  // vertical splitter
#define ES_TRACK      0x00000002  // track with mouse
#define ES_LINE       0x00000004  // draw a line in the splitter
#define ES_DOCK       0x00000008  // borders are magnetic

#define EASYSPLIT TEXT("EASYSPLIT")

// Functions

// initializes the EASYSPLIT control class
// you must do this before creating splitter windows
ATOM RegisterEasySplit(HINSTANCE inst);


// fills a RECT structure with the coordinates of a child
// window inside it's parent's client area.
BOOL WINAPI GetChildRect(HWND hWin, PRECT pRect);

#endif // __EASY_SPLIT_H__
