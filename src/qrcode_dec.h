#pragma once

#ifndef __QRCODE_DEC_H__
#define __QRCODE_DEC_H__

#include <stddef.h>

typedef void(*get_img_data_cb)(void* ctx, void* (*allocator)(size_t), int* pWidth, int* pHeight, unsigned char** pData);
char* qr_code_decoder(get_img_data_cb get_data, void* ctx, void* (*allocator)(size_t));

#endif // __QRCODE_DEC_H__
