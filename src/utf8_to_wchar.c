#include <windows.h>
#include "utf8_to_wchar.h"

char* wchar_string_to_utf8(const wchar_t* wstr, char* receiver, size_t size)
{
    int ol = lstrlenW(wstr);
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, ol, NULL, 0, NULL, NULL);
    if ((int)size < size_needed) {
        return NULL;
    }
    ZeroMemory(receiver, size * sizeof(char));
    WideCharToMultiByte(CP_UTF8, 0, wstr, ol, receiver, size_needed, NULL, NULL);
    return receiver;
}

wchar_t* utf8_to_wchar_string(const char* str, wchar_t* receiver, size_t size)
{
    int ol = lstrlenA(str);
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str, ol, NULL, 0);
    if (ol < size_needed) {
        return NULL;
    }
    ZeroMemory(receiver, size * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, str, ol, receiver, size_needed);
    return receiver;
}
