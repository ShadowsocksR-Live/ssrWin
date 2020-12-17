#pragma once

#include <Windows.h>

HRESULT save_bitmap_to_bmp_file(HBITMAP bitmap, const wchar_t* pathname);
HRESULT save_bitmap_to_png_file(HBITMAP bitmap, const wchar_t* pathname);
HRESULT save_bitmap_to_jpg_file(HBITMAP bitmap, const wchar_t* pathname);

HBITMAP bitmap_clone(HBITMAP srcBmp);
void bitmap_grayscale(HBITMAP hbitmap);

void extract_bitmap_in_grayscale_8bpp(HBITMAP hbitmap, int* pWidth, int* pHeight, BYTE** pData);

