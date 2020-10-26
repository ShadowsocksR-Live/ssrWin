#pragma once

#include <stddef.h>
#include <wchar.h>

char* wchar_string_to_utf8(const wchar_t* wstr, char* receiver, size_t size);
wchar_t* utf8_to_wchar_string(const char* str, wchar_t* receiver, size_t size);
