#include "last_error_to_string.h"
#include <Windows.h>

wchar_t* get_last_error_to_string(DWORD errorMessageID, void* (*allocator)(size_t))
{
    wchar_t* message = NULL;
    LPWSTR messageBuffer = NULL;
    DWORD size;
    do {
        DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
        DWORD languageId = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);

        if (allocator == NULL) {
            break;
        }

        size = FormatMessageW(flags, NULL, errorMessageID, languageId, (LPWSTR)&messageBuffer, 0, NULL);

        message = (wchar_t*)allocator((size + 1) * sizeof(wchar_t));
        if (message == NULL) {
            break;
        }
        memset(message, 0, (size + 1) * sizeof(wchar_t));
        if (messageBuffer) {
            lstrcpyW(message, messageBuffer);
        }
    } while (0);

    if (messageBuffer) {
        //Free the buffer.
        LocalFree(messageBuffer);
    }

    return message;
}
