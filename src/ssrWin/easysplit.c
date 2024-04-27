/*
*                    EASY SPLITTER v2.0
*          Draggable splitter window custom control
*          For Pelles C,  By L. D. Blake 2005, 2006
*/

#include "easysplit.h"

// Easy splitter control main code

// data kept for each instance 
typedef struct t_ESDATA {
    HWND      Parent;       // parent window handle
    INT       Position;     // current location of splitter
    INT       Undo;         // position when focused or captured
    HCURSOR   Cursor;       // cursor used by splitter
    COLORREF  FGColor;      // pen color for drawing
    COLORREF  BGColor;      // background for drawing
    INT       TLBorder;     // top or left border
    INT       BRBorder;     // bottom or right border
    INT       MagBorder;    // number of pixels for docking
    ULONG     Captured : 1,   // true if mouse capture set
              Focused : 1,    // true if keyboard focus set
              Disabled : 1,   // true if window is inactive
              Vertical : 1,   // true means vertical splitter
              FgLine : 1,     // true enables center line            
              Track : 1,      // true enables real time tracking 
              Dock : 1,       // true enables magnetic borders
              KBMove : 1;     // true enables keyboard
} ESDATA, * PESDATA;

// send WM_SIZE to parent window   
void es_NotifyParent(PESDATA esd) {
    RECT pr;
    GetClientRect(esd->Parent, &pr);
    PostMessage(esd->Parent, WM_SIZE, SIZE_RESTORED, MAKELPARAM(pr.bottom, pr.right));
}

// do not allow moves outside parent window
INT es_CheckBorders(PESDATA esd, INT pos) {
    INT mb;
    RECT pr;
    // get movement limits
    GetClientRect(esd->Parent, &pr);
    pr.left += esd->TLBorder;
    pr.top += esd->TLBorder;
    pr.right -= esd->BRBorder;
    pr.bottom -= esd->BRBorder;
    // set magnetic border value
    mb = 0;
    if (esd->Dock) {
        mb = esd->MagBorder;
    }
    // keep position within borders
    if (esd->Vertical) {
        pos = pos < (pr.left + mb) ? pr.left : pos;
        pos = pos > (pr.right - mb) ? pr.right : pos;
    } else {
        pos = pos < (pr.top + mb) ? pr.top : pos;
        pos = pos > (pr.bottom - mb) ? pr.bottom : pos;
    }
    esd->Position = pos;
    return pos;
}


// set undo point
void es_SetUndo(PESDATA esd) {
    esd->Undo = esd->Position;
}


// get mouse position relative to parent window
void es_GetMousePos(HWND win, PPOINT mc) {
    GetCursorPos(mc);
    ScreenToClient(win, mc);
}

// ----------------- Private message handlers --------------------------

// report current position
LRESULT WINAPI es_ReportPosition(PESDATA esd) {
    return es_CheckBorders(esd, esd->Position);
}

// programatically move the splitter
LRESULT WINAPI es_MoveSplitter(PESDATA esd, INT pos) {
    es_CheckBorders(esd, pos);
    es_NotifyParent(esd);
    return esd->Position;
}


// return bars to previous position
LRESULT WINAPI es_HandleUndo(PESDATA esd) {
    return es_MoveSplitter(esd, esd->Undo);
}

// stop a tracking operation
LRESULT WINAPI es_StopTracking(PESDATA esd, HWND win) {
    if (esd->Captured) {
        ReleaseCapture();
        esd->Captured = FALSE;
    }
    return 0;
}

// load a new cursor
LRESULT WINAPI es_SetCursor(PESDATA esd, LPARAM lp) {
    esd->Cursor = (HCURSOR)lp;
    if (esd->Captured) {
        SetCursor(esd->Cursor);
    }
    return 0;
}

// forward declaration
LRESULT WINAPI es_DrawWindow(PESDATA esd, HWND win);

// Set new line and background colors
LRESULT WINAPI es_SetColors(PESDATA esd, HWND win, WPARAM wp, LPARAM lp) {
    HPEN pen, ppen;
    HBRUSH br, pbr;
    HDC dc = GetDC(win);
    esd->FGColor = (COLORREF)(wp & 0xFFFFFFFF);
    esd->BGColor = (COLORREF)(lp & 0xFFFFFFFF);
    pen = CreatePen(PS_SOLID, 2, esd->FGColor);
    ppen = (HPEN)SelectObject(dc, pen);
    DeleteObject(ppen);
    br = CreateSolidBrush(esd->BGColor);
    pbr = (HBRUSH)SelectObject(dc, br);
    DeleteObject(pbr);
    es_DrawWindow(esd, win);
    return 0;
}

// set tracking limits for splitters
LRESULT WINAPI es_SetBorder(PESDATA esd, WPARAM wp, LPARAM lp)
{
    switch (lp)
    {
    case ESB_TOP:
        if (!esd->Vertical) {
            esd->TLBorder = (INT)wp;
        }
        return 0;
    case ESB_BOTTOM:
        if (!esd->Vertical) {
            esd->BRBorder = (INT)wp;
        }
        return 0;
    case ESB_LEFT:
        if (esd->Vertical) {
            esd->TLBorder = (INT)wp;
        }
        return 0;
    case ESB_RIGHT:
        if (esd->Vertical) {
            esd->BRBorder = (INT)wp;
        }
        return 0;
    case ESB_ALL:
        esd->BRBorder = (INT)wp;
        esd->TLBorder = (INT)wp;
        return 0;
    default:
        return 0;
    }
}

// turn mouse tracking on and off
LRESULT WINAPI es_SetTracking(PESDATA esd, LPARAM lp) {
    esd->Track = (lp != 0);
    return 0;
}

// turn border traps on and off
LRESULT WINAPI es_SetDocking(PESDATA esd, WPARAM wp, LPARAM lp) {
    esd->Dock = (lp != 0);
    if (esd->Dock) {
        esd->MagBorder = (INT)wp;
    } else {
        esd->MagBorder = 0;
    }
    return 0;
}

// turn keyboard moves on and off
LRESULT WINAPI es_SetKBMove(PESDATA esd, LPARAM lp) {
    esd->KBMove = (lp != 0);
    return 0;
}

// turn the foreground line on and off
LRESULT WINAPI es_SetLine(PESDATA esd, LPARAM lp) {
    esd->FgLine = (lp != 0);
    return 0;
}

//-------------  Windows message handlers ------------------------

// redraw the splitter window
LRESULT WINAPI es_DrawWindow(PESDATA esd, HWND win) {
    RECT    cr;
    HDC     dc;
    HPEN    pen, ppen;
    int   LinePos;
    GetClientRect(win, &cr);
    ValidateRect(win, &cr);
    dc = GetDC(win);
    // erase background
    if (esd->Focused) {
        pen = CreatePen(PS_DOT, 1, esd->FGColor);
        ppen = (HPEN)SelectObject(dc, pen);
        Rectangle(dc, cr.left, cr.top, cr.right, cr.bottom);
        SelectObject(dc, ppen);
        DeleteObject(pen);
    } else {
        FillRect(dc, &cr, (HBRUSH)GetCurrentObject(dc, OBJ_BRUSH));
    }
    // draw foreground line
    if (esd->FgLine && (!esd->Disabled)) {
        if (esd->Vertical) {
            LinePos = (cr.right - cr.left) / 2;
            MoveToEx(dc, LinePos, (cr.top + 5), NULL);
            LineTo(dc, LinePos, (cr.bottom - 10));
        } else {
            LinePos = (cr.bottom - cr.top) / 2;
            MoveToEx(dc, (cr.left + 5), LinePos, NULL);
            LineTo(dc, (cr.right - 10), LinePos);
        }
    }
    ReleaseDC(win, dc);
    return 0;
}

// update splitter's position 
LRESULT WINAPI es_UpdatePosition(PESDATA esd, LPARAM lp) {
    LPWINDOWPOS wp = (LPWINDOWPOS)lp;
    if (esd->Vertical) {
        esd->Position = wp->x + (wp->cx / 2);
    } else {
        esd->Position = wp->y + (wp->cy / 2);
    }
    return 0;
}


// Set mouse capture
LRESULT WINAPI es_SetCapture(PESDATA esd, HWND win) {
    es_SetUndo(esd);
    SetCapture(win);
    esd->Captured = TRUE;
    return 0;
}

// Release mouse capture
LRESULT WINAPI es_ReleaseCapture(PESDATA esd, HWND win) {
    POINT mc;
    if (!esd->Captured) {
        return 1;
    }
    es_GetMousePos(esd->Parent, &mc);
    if (esd->Vertical) {
        es_MoveSplitter(esd, mc.x);
    } else {
        es_MoveSplitter(esd, mc.y);
    }
    ReleaseCapture();
    esd->Captured = FALSE;
    return 0;
}

// Deal with mouse move messages    
LRESULT WINAPI es_HandleMouseMoves(PESDATA esd, HWND win) {
    POINT mc;
    if (!esd->Captured) {
        SetCursor(esd->Cursor);
        return 0;
    }
    if (esd->Track) {
        es_GetMousePos(esd->Parent, &mc);
        if (esd->Vertical) {
            es_MoveSplitter(esd, mc.x);
        } else {
            es_MoveSplitter(esd, mc.y);
        }
    }
    return 0;
}

// deal with arrow keys
LRESULT WINAPI es_HandleKeyboard(PESDATA esd, WPARAM wp) {
    INT mb, x;
    RECT wr;
    if ((!esd->Focused) || (!esd->KBMove)) {
        return 0;
    }
    // need to know parent size
    GetClientRect(esd->Parent, &wr);
    // check docking
    mb = 1;
    if (esd->Dock) {
        mb = esd->MagBorder + 1;
    }
    // get position
    x = esd->Position;
    // process keystrokes
    if (esd->Vertical) {
        switch (wp) {
        case VK_LEFT:
            if (x == (wr.right - esd->BRBorder)) {
                return es_MoveSplitter(esd, x - mb);
            } else {
                return es_MoveSplitter(esd, --x);
            }
        case VK_RIGHT:
            if (x == (wr.left + esd->TLBorder)) {
                return es_MoveSplitter(esd, x + mb);
            } else {
                return es_MoveSplitter(esd, ++x);
            }
        case VK_HOME:
            return es_MoveSplitter(esd, 0);
        case VK_END:
            return es_MoveSplitter(esd, wr.right);
        }
    } else
        switch (wp) {
        case VK_UP:
            if (x == (wr.bottom - esd->BRBorder)) {
                return es_MoveSplitter(esd, x - mb);
            } else {
                return es_MoveSplitter(esd, --x);
            }
        case VK_DOWN:
            if (x == (wr.top + esd->TLBorder)) {
                return es_MoveSplitter(esd, x + mb);
            } else {
                return es_MoveSplitter(esd, ++x);
            }
        case VK_PRIOR:
            return es_MoveSplitter(esd, 0);
        case VK_NEXT:
            return es_MoveSplitter(esd, wr.bottom);
        }
    return 0;
}

// deal with focus activation
LRESULT WINAPI es_GainFocus(PESDATA esd, HWND win) {
    esd->Focused = TRUE;
    es_DrawWindow(esd, win);
    return 0;
}

// deal with focus deactivation
LRESULT WINAPI es_LoseFocus(PESDATA esd, HWND win) {
    esd->Focused = FALSE;
    es_DrawWindow(esd, win);
    return 0;
}

// deal with enable and disable    
LRESULT WINAPI es_EnableSplitter(PESDATA esd, HWND win, WPARAM wp) {
    esd->Disabled = (wp == FALSE);
    es_DrawWindow(esd, win);
    return 0;
}

// free up splitter window data
LRESULT WINAPI es_TrashWindow(PESDATA esd) {
    free(esd);
    return 0;
}

// set up data for a new splitter window 
LRESULT WINAPI es_NewWindow(HWND win, LPARAM lp) {
    RECT cr;
    PESDATA esd;
    // stop window creation if not WS_CHILD
    if (!((LPCREATESTRUCT)lp)->style & WS_CHILD) {
        return FALSE;
    }
    // snag some memory
    esd = (PESDATA)malloc(sizeof(ESDATA));
    if (!esd) {
        return FALSE;
    }
    // all values to 0
    memset(esd, 0, sizeof(ESDATA));
    SetWindowLongPtr(win, 0, (LONG_PTR)esd);
    // handle window styles
    esd->Parent = GetParent(win);
    esd->Disabled = (((LPCREATESTRUCT)lp)->style & WS_DISABLED) > 0;
    esd->Vertical = (((LPCREATESTRUCT)lp)->style & ES_VERT) > 0;
    esd->FgLine = (((LPCREATESTRUCT)lp)->style & ES_LINE) > 0;
    esd->Track = (((LPCREATESTRUCT)lp)->style & ES_TRACK) > 0;
    esd->Dock = (((LPCREATESTRUCT)lp)->style & ES_DOCK) > 0;
    // set default values
    esd->KBMove = 1;
    if (esd->Dock) {
        esd->MagBorder = 10;
    }
    if (esd->Vertical) {
        esd->Cursor = LoadCursor(NULL, IDC_SIZEWE);
    } else {
        esd->Cursor = LoadCursor(NULL, IDC_SIZENS);
    }
    es_SetColors(esd, win, (WPARAM)GetSysColor(COLOR_3DSHADOW),
        (LPARAM)GetSysColor(COLOR_BTNFACE));
    // initialize position and undo
    GetChildRect(win, &cr);
    if (esd->Vertical) {
        esd->Position = cr.left + ((cr.right - cr.left) / 2);
    } else {
        esd->Position = cr.top + ((cr.bottom - cr.top) / 2);
    }
    es_SetUndo(esd);
    return TRUE;
}

//----------------------  Message Loop etc ---------------------------  

// message loop for splitter windows
LRESULT CALLBACK EasySplitLoop(HWND win, UINT msg, WPARAM wp, LPARAM lp) {
    // grab control's data block
    PESDATA esd = (PESDATA)GetWindowLongPtr(win, 0);
    // do the usual
    switch (msg) {
    case ESM_GETPOS:
        return es_ReportPosition(esd);
    case ESM_SETPOS:
        return es_MoveSplitter(esd, (INT)lp);
    case ESM_UNDO:
        return es_HandleUndo(esd);
    case ESM_STOP:
        return es_StopTracking(esd, win);
    case ESM_SETCOLORS:
        return es_SetColors(esd, win, wp, lp);
    case ESM_SETCURSOR:
        return es_SetCursor(esd, lp);
    case ESM_SETBORDER:
        return es_SetBorder(esd, wp, lp);
    case ESM_SETTRACKING:
        return es_SetTracking(esd, lp);
    case ESM_SETDOCKING:
        return es_SetDocking(esd, wp, lp);
    case ESM_SETKBMOVE:
        return es_SetKBMove(esd, lp);
    case ESM_SETLINE:
        return es_SetLine(esd, lp);
    case WM_PAINT:
        return es_DrawWindow(esd, win);
    case WM_WINDOWPOSCHANGED:
        return es_UpdatePosition(esd, lp);
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        return es_SetCapture(esd, win);
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
        return es_ReleaseCapture(esd, win);
    case WM_MOUSEMOVE:
        return es_HandleMouseMoves(esd, win);
    case WM_SETFOCUS:
        return es_GainFocus(esd, win);
    case WM_KILLFOCUS:
        return es_LoseFocus(esd, win);
    case WM_KEYDOWN:
        return es_HandleKeyboard(esd, wp);
    case WM_GETDLGCODE:
        return DLGC_WANTARROWS;
    case WM_ENABLE:
        return es_EnableSplitter(esd, win, wp);
    case WM_CREATE:
        return es_NewWindow(win, lp);
    case WM_DESTROY:
        return es_TrashWindow(esd);
    case WM_ERASEBKGND:
        return 1;
    default:
        return DefWindowProc(win, msg, wp, lp);
    }
}

// register the easy splitter class
ATOM RegisterEasySplit(HINSTANCE inst) {
    WNDCLASS wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpszClassName = EASYSPLIT;
    wc.hInstance = inst;
    wc.hbrBackground = NULL;     // not used
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = &EasySplitLoop;
    wc.cbWndExtra = sizeof(PESDATA);
    return RegisterClass(&wc);
}

// Return coordinates of a child window inside it's
// parent window's client area.

BOOL WINAPI GetChildRect(HWND hWin, PRECT pRect) {
    POINT pr = { 0,0 };
    if (!(GetWindowLong(hWin, GWL_STYLE) & WS_CHILD)) {
        return 0;
    }
    // parent window's client offsets
    ClientToScreen(GetParent(hWin), &pr);
    // now we need the child rect
    GetWindowRect(hWin, pRect);
    pRect->left -= pr.x;
    pRect->top -= pr.y;
    pRect->right -= pr.x;
    pRect->bottom -= pr.y;
    return 1;
}
