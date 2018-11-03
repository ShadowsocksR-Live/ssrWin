#include <Windows.h>
#include <stdio.h>

#include "comm_with_named_pipe.h"

int comm_with_named_pipe(const char *pipe_name, uint8_t *data, size_t size, int need_response, fn_comm_result pfn, void *p) {
    HANDLE handle = INVALID_HANDLE_VALUE;
    DWORD done_size;
    size_t buffer_size = MAX_PATH*10;
    char *buffer = (char *)calloc(buffer_size, sizeof(char));
    DWORD error_num = 0;
    do {
        sprintf(buffer, "\\\\.\\Pipe\\%s", pipe_name);
        if (WaitNamedPipeA(buffer, NMPWAIT_WAIT_FOREVER) == 0) {
            error_num = GetLastError();
            sprintf(buffer, "WaitNamedPipe failed with error %d\n", error_num);
            break;
        }

        // Open the named pipe file handle
        handle = CreateFileA(buffer, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        buffer[0] = 0;
        if (handle == INVALID_HANDLE_VALUE) {
            error_num = GetLastError();
            sprintf(buffer, "CreateFile failed with error %d\n", error_num);
            break;
        }
        if (WriteFile(handle, data, size, &done_size, NULL) == FALSE) {
            error_num = GetLastError();
            sprintf(buffer, "WriteFile failed with error %d\n", error_num);
            break;
        }
        if (need_response == 0) {
            break;
        }
        // TODO: overlapped read to implement timeout.
        if (ReadFile(handle, buffer, buffer_size, &done_size, NULL) == FALSE) {
            error_num = GetLastError();
            sprintf(buffer, "ReadFile failed with error %d\n", error_num);
            break;
        }
    } while(0);
    if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
    }

    if (pfn) {
        if (error_num != 0) {
            done_size = lstrlenA(buffer);
        }
        pfn(error_num, (uint8_t *)buffer, (size_t)done_size, p);
    }

    free(buffer);
    return (int)error_num;
}
