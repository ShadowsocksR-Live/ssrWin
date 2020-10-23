#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "cmd_handler.h"
#include "comm_with_named_pipe.h"

#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"") // no console window
#pragma comment(lib, "Rpcrt4.lib")

SERVICE_STATUS _service_status;
SERVICE_STATUS_HANDLE _service_status_handle;
DWORD _thread_id;

void write_to_log(const char* str);

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
        write_to_log(buffer);
    }
    return handle;
}

BOOL pipe_exit_flag = FALSE;

void guid_generator(char *guid_str, size_t size) {
    GUID guid;
    RPC_CSTR str = NULL;
    CoCreateGuid(&guid);

    UuidToStringA(&guid, &str);
    if (str) {
        strncpy(guid_str, (char *)str, size);
        RpcStringFreeA(&str);
    }
}

void pipe_exit_loop(const char *pipe_name) {
    char buffer[MAX_PATH] = { 0 };

    pipe_exit_flag = TRUE;

    guid_generator(buffer, sizeof(buffer));
    comm_with_named_pipe(pipe_name, (uint8_t *)buffer, sizeof(buffer), 0, NULL, NULL);
}

typedef BOOL(*fn_process_message)(const BYTE *msg, size_t msg_size, BYTE *result, size_t *result_size, void *p);

struct pipe_cxt {
    HANDLE pipe;
    fn_process_message callback;
    void *p;
};

DWORD __stdcall client_thread(LPVOID lpvParam);

void pipe_infinite_loop(const char* pipe_name, fn_process_message process_message, void *p) {
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

        pipe = pipe_create(pipe_name);
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
            write_to_log(buffer);
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
                write_to_log((char *)buffer);
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
                write_to_log((char *)buffer);
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
// service implementation

#if 0
BOOL svc_message_handler(const BYTE *msg, size_t msg_size, BYTE *result, size_t *result_size, void *p) {
    size_t size = min(msg_size, *result_size);
    memmove(result, msg, size);
    *result_size = size;
    return TRUE;
}
#endif

DWORD __stdcall pipe_msg_server_thread(LPVOID lpvParam) {
    const char* pipe_name = (const char*)lpvParam;
    pipe_infinite_loop(pipe_name, svc_message_handler, NULL);
    return 0;
}

void write_to_log(const char* str) {
#if 0
#define LOGFILE "C:\\memstatus.txt"
    FILE *log;
    log = fopen(LOGFILE, "a+");
    if (log == NULL) {
        return -1;
    }
    fprintf(log, "%s\r\n", str);
    fclose(log);
#else
    OutputDebugStringA(str);
#endif
}

// Service initialization
int init_service() {
    int result = 0;
    write_to_log("Monitoring started.");
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
    DWORD dwThreadId = 0;
    const char* pipe_name = PIPE_NAME;

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

    msg_thread = CreateThread(NULL, 0, pipe_msg_server_thread, (LPVOID)pipe_name, 0, &dwThreadId);
    if (msg_thread==INVALID_HANDLE_VALUE || msg_thread==NULL) {
        _service_status.dwCurrentState = SERVICE_STOPPED;
        _service_status.dwWin32ExitCode = -1;
        SetServiceStatus(_service_status_handle, &_service_status);
        return;
    }

    run_message_loop();

    pipe_exit_loop(pipe_name);
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
