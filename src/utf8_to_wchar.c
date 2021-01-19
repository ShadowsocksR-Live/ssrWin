#include <windows.h>
#include "utf8_to_wchar.h"

char* wchar_string_to_utf8(const wchar_t* wstr, void* (*allocator)(size_t))
{
    int ol, size_needed;
    char* receiver = NULL;

    if (wstr == NULL || allocator == NULL) {
        return NULL;
    }

    ol = lstrlenW(wstr);
    if (ol <= 0) {
        return NULL;
    }

    size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, ol, NULL, 0, NULL, NULL);
    if (size_needed <= 0) {
        return NULL;
    }
    receiver = (char*)allocator((size_needed + 1) * sizeof(char));
    if (receiver == NULL) {
        return NULL;
    }
    ZeroMemory(receiver, (size_needed + 1) * sizeof(char));
    WideCharToMultiByte(CP_UTF8, 0, wstr, ol, receiver, size_needed + 1, NULL, NULL);
    return receiver;
}

wchar_t* utf8_to_wchar_string(const char* str, void* (*allocator)(size_t))
{
    int size_needed, ol;
    wchar_t* receiver;
    if (str == NULL || allocator == NULL) {
        return NULL;
    }

    ol = lstrlenA(str);
    if (ol <= 0) {
        return NULL;
    }

    size_needed = MultiByteToWideChar(CP_UTF8, 0, str, ol, NULL, 0);
    if (size_needed <= 0) {
        return NULL;
    }

    receiver = (wchar_t*)allocator((size_needed + 1) * sizeof(wchar_t));
    if (receiver == NULL) {
        return NULL;
    }
    memset(receiver, 0, (size_needed + 1) * sizeof(wchar_t));

    MultiByteToWideChar(CP_UTF8, 0, str, ol, receiver, size_needed + 1);
    return receiver;
}
