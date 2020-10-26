#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <tchar.h>
#include <windows.h>

#include "win_cmd_wrapper.h"

int run_command_wrapper(const wchar_t* cmd, const wchar_t* argv_fmt, ...)
{
    wchar_t buffer[MAX_PATH * 2] = { 0 };
    va_list arg;
    va_start(arg, argv_fmt);
    vswprintf(buffer, ARRAYSIZE(buffer), argv_fmt, arg);
    va_end(arg);
    return run_command(cmd, buffer);
}

int run_command(const wchar_t* cmd, const wchar_t* args)
{
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD exit_code = 0;
    size_t len = 0;
    wchar_t* buffer = NULL;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    len = lstrlenW(cmd) + lstrlenW(args) + 10;
    buffer = (wchar_t*)calloc(len, sizeof(wchar_t));
    if (cmd[0] == L'"') {
        swprintf(buffer, len, L"%s %s", cmd, args);
    } else {
        swprintf(buffer, len, L"\"%s\" %s", cmd, args);
    }

    // Start the child process.
    if (!CreateProcessW(NULL, // No module name (use command line)
            buffer, // Command line
            NULL, // Process handle not inheritable
            NULL, // Thread handle not inheritable
            FALSE, // Set handle inheritance to FALSE
            0, // No creation flags
            NULL, // Use parent's environment block
            NULL, // Use parent's starting directory
            &si, // Pointer to STARTUPINFO structure
            &pi) // Pointer to PROCESS_INFORMATION structure
    ) {
        wprintf(L"CreateProcess failed (%d).\n", GetLastError());
        free(buffer);
        return -1;
    }

    // Wait until child process exits.
    WaitForSingleObject(pi.hProcess, INFINITE);

    GetExitCodeProcess(pi.hProcess, &exit_code);

    // Close process and thread handles.
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    free(buffer);

    return (int)exit_code;
}
