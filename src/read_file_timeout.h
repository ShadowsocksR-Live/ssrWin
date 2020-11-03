#pragma once

#include <Windows.h>

ULONG ReadFileTimeout(HANDLE hFile,
    PVOID lpBuffer,
    ULONG nNumberOfBytesToRead,
    PULONG lpNumberOfBytesRead,
    PLARGE_INTEGER ByteOffset,
    ULONG dwMilliseconds);
