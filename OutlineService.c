#include <windows.h>
#include <stdio.h>

#define SVC_NAME "OutlineService"
#define PIPE_NAME "OutlineServicePipe"

#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"") // no console window

SERVICE_STATUS _service_status;
SERVICE_STATUS_HANDLE _service_status_handle;
DWORD _thread_id;

//////////////////////////////////////////////////////////////////////////
// pipe server API

HANDLE pipe_create(const char *pipe_name) {
    char buffer[256];
    HANDLE handle;

    sprintf(buffer, "\\\\.\\Pipe\\%s", pipe_name);
    handle = CreateNamedPipeA(buffer, 
        PIPE_ACCESS_DUPLEX, // open mode
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE, // pipe mode
        PIPE_UNLIMITED_INSTANCES, // Num. of MaxInstances: between 1 and PIPE_UNLIMITED_INSTANCES
        0, // out buffer size
        0, // in buffer size
        1000, // timeout
        NULL // Security descriptor
        );
    if (handle == INVALID_HANDLE_VALUE) {
        sprintf(buffer, "CreateNamedPipe failed with error %d\n", GetLastError());
        OutputDebugStringA(buffer);
    }
    return handle;
}

#define PIPE_EXIT_MSG "CBC28B3B-897E-4DD4-8531-AD5DAD64AD88"

void pipe_exit_loop(const char *pipe_name) {
    HANDLE handle;
    DWORD done_size;
    char buffer[256] = { 0 };

    sprintf(buffer, "\\\\.\\Pipe\\%s", pipe_name);
    if (WaitNamedPipeA(buffer, NMPWAIT_WAIT_FOREVER) == 0) {
        sprintf(buffer, "WaitNamedPipe failed with error %d\n", GetLastError());
        OutputDebugStringA(buffer);
        return;
    }

    // Open the named pipe file handle
    handle = CreateFileA(buffer, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        sprintf(buffer, "CreateFile failed with error %d\n", GetLastError());
        OutputDebugStringA(buffer);
        return;
    }
    WriteFile(handle, PIPE_EXIT_MSG, strlen(PIPE_EXIT_MSG) + 1, &done_size, NULL);
    CloseHandle(handle);
}

typedef BOOL(*fn_process_message)(const BYTE *msg, size_t msg_size, BYTE *result, size_t *result_size, void *p);

void pipe_loop(HANDLE handle, fn_process_message process_message, void *p) {
    if (handle==NULL || handle==INVALID_HANDLE_VALUE || process_message==NULL) {
        return;
    }

    while ((ConnectNamedPipe(handle, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED))) {
        DWORD done_size = 0;
        BYTE buffer[256] = { 0 };
        BYTE result[256] = { 0 };
        size_t result_size = 0;
        BOOL fSuccess = FALSE;

        fSuccess = ReadFile(handle, buffer, sizeof(buffer), &done_size, NULL);
        if (!fSuccess || done_size == 0) {
            if (GetLastError() == ERROR_BROKEN_PIPE) {
                sprintf((char *)buffer, "client disconnected.\n");
                continue;
            } else {
                sprintf((char *)buffer, "ReadFile failed with error %d\n", GetLastError());
                OutputDebugStringA(buffer);
            }
            break;
        }

        if (memcmp(buffer, PIPE_EXIT_MSG, strlen(PIPE_EXIT_MSG)) == 0) {
            break;
        }

        result_size = sizeof(result);
        if(process_message(buffer, (size_t)done_size, result, &result_size, p) == FALSE) {
            continue;
        }

        fSuccess = WriteFile(handle, result, result_size, &done_size, NULL);

        if (!fSuccess || result_size != done_size) {
            sprintf((char *)buffer, "WriteFile failed, GLE=%d.\n", GetLastError());
            OutputDebugStringA(buffer);
            break;
        }
    }
}

void pipe_destroy(HANDLE handle) {
    if (handle==NULL || handle==INVALID_HANDLE_VALUE) {
        return;
    }
    // Flush the pipe to allow the client to read the pipe's contents
    // before disconnecting. Then disconnect the pipe, and close the
    // handle to this pipe instance.
    FlushFileBuffers(handle);
    DisconnectNamedPipe(handle);
    CloseHandle(handle);
}

//////////////////////////////////////////////////////////////////////////
// outline service

BOOL outline_svc_message(const BYTE *msg, size_t msg_size, BYTE *result, size_t *result_size, void *p) {
    size_t size = min(msg_size, *result_size);
    memmove(result, msg, size);
    *result_size = size;
    return TRUE;
}

DWORD __stdcall outline_msg_server_thread(LPVOID lpvParam) {
    HANDLE pipe = pipe_create(PIPE_NAME);
    if (pipe==NULL || pipe==INVALID_HANDLE_VALUE) {
        return -1;
    }
    pipe_loop(pipe, outline_svc_message, NULL);
    pipe_destroy(pipe);
    return 0;
}

int write_to_log(const char* str) {
#if 0
#define LOGFILE "C:\\memstatus.txt"
    FILE *log;
    log = fopen(LOGFILE, "a+");
    if (log == NULL) {
        return -1;
    }
    fprintf(log, "%s\r\n", str);
    fclose(log);
#endif
    return 0;
}

// Service initialization
int init_service() {
    int result;
    result = write_to_log("Monitoring started.");
    return(result);
}

// Control handler function
void __stdcall service_control_handler(DWORD request) {
    switch (request) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        write_to_log("Monitoring stopped.");

        PostThreadMessage(_thread_id, WM_QUIT, 0, 0);

        _service_status.dwWin32ExitCode = 0;
        _service_status.dwCurrentState = SERVICE_STOPPED;
        break;
    default:
        break;
    }

    // Report current status
    SetServiceStatus(_service_status_handle, &_service_status);

    return;
}

// The worker loop of a service
void run_message_loop(void) {
    MSG msg;
    while (GetMessage(&msg, 0, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void __stdcall service_main(DWORD argc, char** argv) {
    int error;
    HANDLE msg_thread;
    DWORD dwThreadId;

    _thread_id = GetCurrentThreadId();			

    _service_status.dwServiceType = SERVICE_WIN32;
    _service_status.dwCurrentState = SERVICE_START_PENDING;
    _service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    _service_status.dwWin32ExitCode = 0;
    _service_status.dwServiceSpecificExitCode = 0;
    _service_status.dwCheckPoint = 0;
    _service_status.dwWaitHint = 0;

    _service_status_handle = 
        RegisterServiceCtrlHandlerA(SVC_NAME, service_control_handler);
    if (_service_status_handle == NULL) {
        // Registering Control Handler failed
        return;
    }
    // Initialize Service 
    error = init_service();
    if (error) {
        // Initialization failed
        _service_status.dwCurrentState = SERVICE_STOPPED;
        _service_status.dwWin32ExitCode = -1;
        SetServiceStatus(_service_status_handle, &_service_status);
        return;
    }
    // We report the running status to SCM. 
    _service_status.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(_service_status_handle, &_service_status);

    msg_thread = CreateThread(NULL, 0, outline_msg_server_thread, NULL, 0, &dwThreadId);
    if (msg_thread==INVALID_HANDLE_VALUE || msg_thread==NULL) {
        _service_status.dwCurrentState = SERVICE_STOPPED;
        _service_status.dwWin32ExitCode = -1;
        SetServiceStatus(_service_status_handle, &_service_status);
        return;
    }

    run_message_loop();

    pipe_exit_loop(PIPE_NAME);
    WaitForSingleObject(msg_thread, INFINITE);
    CloseHandle(msg_thread);

    return;
}

int main(int argc, char** argv) {
    SERVICE_TABLE_ENTRYA entry[] = {
        { SVC_NAME, service_main },
        { NULL, NULL },
    };

    // Start the control dispatcher thread for our service
    StartServiceCtrlDispatcherA(entry);
    return 0;
}
