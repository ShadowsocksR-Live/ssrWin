#include "read_file_timeout.h"

typedef BOOL (WINAPI *pfn_GetOverlappedResultEx)(
    HANDLE hFile,
    LPOVERLAPPED lpOverlapped,
    LPDWORD lpNumberOfBytesTransferred,
    DWORD dwMilliseconds,
    BOOL bAlertable);

//
// https://stackoverflow.com/questions/59555924/using-the-timeout-in-getoverlappedresultex-to-simulate-a-wait-with-timeout
//
ULONG ReadFileTimeout(HANDLE hFile,
    PVOID lpBuffer,
    ULONG nNumberOfBytesToRead,
    PULONG lpNumberOfBytesRead,
    PLARGE_INTEGER ByteOffset,
    ULONG dwMilliseconds)
{
    ULONG dwError = NOERROR;

    OVERLAPPED ov;
    ov.Offset = ByteOffset->LowPart;
    ov.OffsetHigh = ByteOffset->HighPart;

    if (ov.hEvent = CreateEvent(0, 0, 0, 0)) {
        dwError = ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, &ov) ? NOERROR : GetLastError();

        if (dwError == ERROR_IO_PENDING) {
            HINSTANCE hDLL = LoadLibraryW(L"Kernel32.dll");
            pfn_GetOverlappedResultEx pfn = (pfn_GetOverlappedResultEx)
                GetProcAddress(hDLL, "GetOverlappedResultEx");
            if (pfn) {
                // yes, the first param do not need hFile in GetOverlappedResultEx.
                dwError = pfn(0, &ov, lpNumberOfBytesRead, dwMilliseconds, FALSE) ? NOERROR : GetLastError();
            } else {
                WaitForSingleObject(ov.hEvent, dwMilliseconds);
                dwError = GetOverlappedResult(hFile, &ov, lpNumberOfBytesRead, FALSE);
            }
        }

        CloseHandle(ov.hEvent);
    } else {
        dwError = GetLastError();
    }

    return dwError;
}
