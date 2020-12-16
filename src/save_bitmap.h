#pragma once

#include <Windows.h>

HRESULT save_bitmap_to_bmp_file(HBITMAP bitmap, const wchar_t* pathname);
HRESULT save_bitmap_to_png_file(HBITMAP bitmap, const wchar_t* pathname);
HRESULT save_bitmap_to_jpg_file(HBITMAP bitmap, const wchar_t* pathname);

