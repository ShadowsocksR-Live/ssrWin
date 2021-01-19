#pragma once

#include <stddef.h>
#include <wchar.h>

char* wchar_string_to_utf8(const wchar_t* wstr, void* (*allocator)(size_t));
wchar_t* utf8_to_wchar_string(const char* str, void* (*allocator)(size_t));
