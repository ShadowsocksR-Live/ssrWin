#pragma once

#ifndef __QRCODE_DEC_H__
#define __QRCODE_DEC_H__

#include <Windows.h>

char* qr_code_decoder(HBITMAP hBmp, void* (*allocator)(size_t));

#endif // __QRCODE_DEC_H__
