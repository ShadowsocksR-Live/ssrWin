#pragma once

#include <Windows.h>

/*
*  Get the error message, if any.
*  errorMessageID = GetLastError();
*/
wchar_t* get_last_error_to_string(DWORD errorMessageID, void* (*allocator)(size_t));
