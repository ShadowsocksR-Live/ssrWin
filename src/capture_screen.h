#pragma once

#ifndef __CAPTURE_SCREEN_H__
#define __CAPTURE_SCREEN_H__

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

//
// To release the HBITMAP, please call DeleteObject.
//

HBITMAP capture_screen(void);

#ifdef __cplusplus
}
#endif

#endif // __CAPTURE_SCREEN_H__
