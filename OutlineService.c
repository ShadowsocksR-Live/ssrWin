#include <windows.h>
#include <stdio.h>

#define SVC_NAME "OutlineService"

#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"") // no console window

SERVICE_STATUS _service_status;
SERVICE_STATUS_HANDLE _service_status_handle;
DWORD _thread_id;		

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

    run_message_loop();

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
