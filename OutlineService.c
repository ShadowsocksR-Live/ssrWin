#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define SVC_NAME "OutlineService"
#define PIPE_NAME "OutlineServicePipe"

#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"") // no console window

SERVICE_STATUS _service_status;
SERVICE_STATUS_HANDLE _service_status_handle;
DWORD _thread_id;

//////////////////////////////////////////////////////////////////////////
// pipe server API

#define BUFSIZE 512

HANDLE pipe_create(const char *pipe_name) {
    char buffer[MAX_PATH] = { 0 };
    HANDLE handle;

    sprintf(buffer, "\\\\.\\Pipe\\%s", pipe_name);
    handle = CreateNamedPipeA(buffer, 
        PIPE_ACCESS_DUPLEX, // open mode
        PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT, // pipe mode
        PIPE_UNLIMITED_INSTANCES, // Num. of MaxInstances: between 1 and PIPE_UNLIMITED_INSTANCES
        BUFSIZE, // out buffer size
        BUFSIZE, // in buffer size
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
BOOL pipe_exit_flag = FALSE;

void pipe_exit_loop(const char *pipe_name) {
    HANDLE handle;
    DWORD done_size;
    char buffer[MAX_PATH] = { 0 };

    pipe_exit_flag = TRUE;

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

struct pipe_cxt {
    HANDLE pipe;
    fn_process_message callback;
    void *p;
};

DWORD __stdcall client_thread(LPVOID lpvParam);

void pipe_infinite_loop(fn_process_message process_message, void *p) {
    if (process_message==NULL) {
        return;
    }

    while(pipe_exit_flag == FALSE) {
        BOOL   fConnected = FALSE;
        HANDLE hThread = NULL;
        DWORD dwThreadId = 0;
        struct pipe_cxt *ctx = NULL;
        char buffer[MAX_PATH] = { 0 };
        HANDLE pipe = NULL;

        pipe = pipe_create(PIPE_NAME);
        if (pipe==NULL || pipe==INVALID_HANDLE_VALUE) {
            break;
        }

        //
        // https://docs.microsoft.com/zh-cn/windows/desktop/ipc/multithreaded-pipe-server
        //
        fConnected = ConnectNamedPipe(pipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (!fConnected) { 
            // The client could not connect, so close the pipe. 
            CloseHandle(pipe);
            continue;
        }

        ctx = (struct pipe_cxt *)calloc(1, sizeof(struct pipe_cxt));
        ctx->callback = process_message;
        ctx->p = p;
        ctx->pipe = pipe;

        // Create a thread for this client. 
        hThread = CreateThread(NULL, 0, client_thread, (LPVOID)ctx, 0, &dwThreadId);
        if (hThread == NULL) {
            sprintf(buffer, "CreateThread failed, GLE=%d.\n", GetLastError());
            OutputDebugStringA(buffer);
            CloseHandle(ctx->pipe);
            free(ctx);
            continue;
        }
        CloseHandle(hThread);
    }
}

DWORD __stdcall client_thread(LPVOID lpvParam) {
    struct pipe_cxt *ctx = (struct pipe_cxt *)lpvParam;
    do {
        if (ctx==NULL || ctx->callback==NULL || ctx->pipe==NULL) {
            break;
        }
        while (1) {
            DWORD done_size = 0;
            BYTE buffer[BUFSIZE] = { 0 };
            BYTE result[BUFSIZE] = { 0 };
            size_t result_size = 0;
            BOOL fSuccess = FALSE;
            fSuccess = ReadFile(ctx->pipe, buffer, sizeof(buffer), &done_size, NULL);
            if (!fSuccess || done_size == 0) {
                if (GetLastError() == ERROR_BROKEN_PIPE) {
                    sprintf((char *)buffer, "client disconnected.\n");
                } else {
                    sprintf((char *)buffer, "ReadFile failed with error %d\n", GetLastError());
                }
                OutputDebugStringA((char *)buffer);
                break;
            }
            if (memcmp(buffer, PIPE_EXIT_MSG, strlen(PIPE_EXIT_MSG)) == 0) {
                break;
            }

            result_size = sizeof(result);
            if(ctx->callback(buffer, (size_t)done_size, result, &result_size, ctx->p) == FALSE) {
                break;
            }

            done_size = 0;
            fSuccess = WriteFile(ctx->pipe, result, result_size, &done_size, NULL);

            if (!fSuccess || result_size != done_size) {
                sprintf((char *)buffer, "WriteFile failed, GLE=%d.\n", GetLastError());
                OutputDebugStringA((char *)buffer);
                break;
            }
        }
    } while(0);
    if (ctx) {
        if (ctx->pipe) {
            FlushFileBuffers(ctx->pipe); 
            DisconnectNamedPipe(ctx->pipe); 
            CloseHandle(ctx->pipe);
        }
        free(ctx);
    }
    return 0;
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
    pipe_infinite_loop(outline_svc_message, lpvParam);
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
    MSG msg = { 0 };
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
