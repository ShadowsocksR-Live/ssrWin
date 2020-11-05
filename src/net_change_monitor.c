#include <winsock2.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <windows.h>
#include <assert.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

struct net_change_ctx {
    HANDLE hThread;
    HANDLE hEventExit;
    void (*notification)(void* p);
    void* param;
};

static DWORD WINAPI net_monitor_thread_proc(LPVOID lpParam)
{
    DWORD ret = 0;
    struct net_change_ctx* ctx = (struct net_change_ctx*)lpParam;

    HANDLE hand = NULL;
    BOOL exit_flag = FALSE;

    Sleep(2000);

    do {
        HANDLE hEvent = WSACreateEvent();
        {
            OVERLAPPED overlap = { 0 };
            overlap.hEvent = hEvent;
            ret = NotifyAddrChange(&hand, &overlap);
        }
        if (ret != NO_ERROR) {
            if (WSAGetLastError() != WSA_IO_PENDING) {
                WSACloseEvent(hEvent);
                printf("NotifyAddrChange error...%d\n", WSAGetLastError());
                assert(0);
                break;
            }
        }

        {
            HANDLE ghEvents[2] = {
                hEvent,
                ctx->hEventExit,
            };
            ret = WaitForMultipleObjects(ARRAYSIZE(ghEvents), ghEvents, FALSE, INFINITE);
            switch (ret) {
            case WAIT_OBJECT_0 + 0:
                printf("IP Address table changed..\n");
                if (ctx->notification) {
                    ctx->notification(ctx->param);
                }
                Sleep(2000);
                break;
            case WAIT_OBJECT_0 + 1:
                printf("Exit event was signaled.\n");
                exit_flag = TRUE;
                break;
            default:
                printf("Wait error: %d\n", GetLastError());
                break;
            }
        }
        WSACloseEvent(hEvent);
    } while (exit_flag == FALSE);
    return 0;
}

struct net_change_ctx* net_status_start_monitor(void (*notification)(void* p), void* p)
{
    struct net_change_ctx* ctx = (struct net_change_ctx*)calloc(1, sizeof(*ctx));
    // auto-reset event object, initial state is non-signaled,
    ctx->hEventExit = CreateEvent(NULL, FALSE, FALSE, NULL);
    ctx->notification = notification;
    ctx->param = p;
    ctx->hThread = CreateThread(NULL, 0, net_monitor_thread_proc, ctx, 0, NULL);

    return ctx;
}

void net_status_stop_monitor(struct net_change_ctx* ctx)
{
    if (ctx == NULL) {
        return;
    }
    if (ctx->hEventExit == NULL || ctx->notification == NULL || ctx->hThread == NULL) {
        assert(0);
        return;
    }
    SetEvent(ctx->hEventExit);
    WaitForSingleObject(ctx->hThread, INFINITE);

    CloseHandle(ctx->hEventExit);
    CloseHandle(ctx->hThread);

    free(ctx);
}
