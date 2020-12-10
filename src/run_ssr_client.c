#include <WinSock2.h>
#include <Windows.h>
#include <ssr_executive.h>
#include <privoxyexports.h>
#include <ssr_client_api.h>
#include "run_ssr_client.h"

static DWORD WINAPI SsrClientThread(LPVOID lpParam);
static uint16_t retrieve_socket_port(SOCKET socket);

struct ssr_client_ctx {
    HANDLE hSsrClient;
    HANDLE hInitCompleteEvent;
    struct server_config* config;
    struct ssr_client_state* state;
    uint16_t real_listen_port;
};

struct ssr_client_ctx* ssr_client_begin_run(struct server_config* config) {
    struct ssr_client_ctx* ctx = (struct ssr_client_ctx*)calloc(1, sizeof(*ctx));
    DWORD threadId = 0;

    ctx->config = config_clone(config);

    // change listen_port to 0, thus we will get a dynamic listen port.
    ctx->config->listen_port = 0;

    ctx->hInitCompleteEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    ctx->hSsrClient = CreateThread(NULL, 0, SsrClientThread, ctx, 0, &threadId);
    WaitForSingleObject(ctx->hInitCompleteEvent, INFINITE);
    CloseHandle(ctx->hInitCompleteEvent);

    return ctx;
}

void ssr_client_terminate(struct ssr_client_ctx* ctx) {
    if (ctx) {
        ssr_run_loop_shutdown(ctx->state);
        WaitForSingleObject(ctx->hSsrClient, INFINITE);
        CloseHandle(ctx->hSsrClient);
        config_release(ctx->config);
        free(ctx);
    }
}

static void ssr_feedback_state(struct ssr_client_state *state, void *p) {
    struct ssr_client_ctx* ctx = (struct ssr_client_ctx*)p;
    SOCKET socket;
    state_set_force_quit(state, true);
    socket = (SOCKET)ssr_get_listen_socket_fd(state);
    ctx->real_listen_port = retrieve_socket_port(socket);
    ctx->state = state;
    SetEvent(ctx->hInitCompleteEvent);
}

static DWORD WINAPI SsrClientThread(LPVOID lpParam) {
    struct ssr_client_ctx* ctx = (struct ssr_client_ctx*)lpParam;
    ssr_run_loop_begin(ctx->config, &ssr_feedback_state, ctx);
    return 0;
}

static uint16_t retrieve_socket_port(SOCKET socket) {
    char tmp[256] = { 0 }; // buffer size must big enough
    int len = sizeof(tmp);
    if (getsockname(socket, (struct sockaddr*)tmp, &len) != 0) {
        return 0;
    } else {
        return ntohs( ((struct sockaddr_in*)tmp)->sin_port);
    }
}
