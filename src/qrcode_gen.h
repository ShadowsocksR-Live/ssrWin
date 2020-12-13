#ifndef __QRCODE_GEN_H__
#define __QRCODE_GEN_H__

#include <Windows.h>

BOOL generate_qr_code_image_file(const char* szSourceSring, const char* szFilePath);
HBITMAP generate_qr_code_image(const char* szSourceSring);
HBITMAP generate_qr_code_image_2(const char* szSourceSring);

#endif // __QRCODE_GEN_H__
