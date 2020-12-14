#pragma once

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
