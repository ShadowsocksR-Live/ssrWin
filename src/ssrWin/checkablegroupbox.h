#pragma once

#include <Windows.h>
#include <WindowsX.h>

#ifdef __cplusplus
extern "C" {
#endif

//
// make a group box control with a check box.
// usage:
//
//    HWND hBox = ::GetDlgItem(this->GetSafeHwnd(), IDC_GROUPBOX3);
//    CheckableGroupBox_SubclassWindow(hBox);
//    Button_SetCheck(hBox, FALSE);
//    BOOL chk = Button_GetCheck(hBox);
//

BOOL CheckableGroupBox_SubclassWindow(HWND hWnd);

#ifdef __cplusplus
}
#endif
