#include <windows.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <tchar.h>

#include "win_cmd_wrapper.h"
#include "create_pipe_ex.h"
#include "read_file_timeout.h"

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
#define BUFSIZE 4096
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD exit_code = 0;
    size_t len = 0;
    wchar_t* buffer = NULL;

    HANDLE g_hChildStd_OUT_Rd = INVALID_HANDLE_VALUE;
    HANDLE g_hChildStd_OUT_Wr = INVALID_HANDLE_VALUE;
    SECURITY_ATTRIBUTES saAttr = { sizeof(saAttr), NULL, TRUE };

    DWORD dwRead = 0;
    CHAR chBuf[BUFSIZE] = { 0 };
    LARGE_INTEGER ByteOffset = { 0 };

    // Create a pipe for the child process's STDOUT.
    if (!MyCreatePipeEx(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0, FILE_FLAG_OVERLAPPED, 0)) {
        return -1;
    }
    // Ensure the read handle to the pipe for STDOUT is not inherited.
    if (!SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
        return -1;
    }

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdError = g_hChildStd_OUT_Wr;
    /*
    si.hStdOutput = g_hChildStd_OUT_Wr;
    si.hStdInput = g_hChildStd_IN_Rd;
    */

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
            TRUE, // Set handle inheritance to TRUE
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

    ReadFileTimeout(g_hChildStd_OUT_Rd, chBuf, BUFSIZE, &dwRead, &ByteOffset, 100);

    // Close process and thread handles.
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    CloseHandle(g_hChildStd_OUT_Wr);
    CloseHandle(g_hChildStd_OUT_Rd);

    if ((exit_code == 0) && (lstrlenA(chBuf)!=0) ){
        exit_code = -1;
    }

    free(buffer);

    return (int)exit_code;
}
