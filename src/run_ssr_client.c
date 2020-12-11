#include <WinSock2.h>
#include <Windows.h>
#include <assert.h>
#include <stdio.h>
#include <exe_file_path.h>
#include <ssr_executive.h>
#include <privoxyexports.h>
#include <ssr_client_api.h>
#include "run_ssr_client.h"

#define PRIVOXY_LISTEN_PORT 8118

#define PRIVOXY_CONFIG_CONTENT_FMT \
    "listen-address  127.0.0.1:%d\r\n" \
    "forward-socks5 / 127.0.0.1:%d .\r\n"

static DWORD WINAPI SsrClientThread(LPVOID lpParam);
static uint16_t retrieve_socket_port(SOCKET socket);
static DWORD WINAPI PrivoxyThread(LPVOID lpParam);

struct ssr_client_ctx {
    HANDLE hSsrClient;
    HANDLE hPrivoxySvr;
    HANDLE hOrderKeeper;
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

    ctx->hOrderKeeper = CreateEvent(NULL, TRUE, FALSE, NULL);
    ctx->hSsrClient = CreateThread(NULL, 0, SsrClientThread, ctx, 0, &threadId);
    WaitForSingleObject(ctx->hOrderKeeper, INFINITE);
    CloseHandle(ctx->hOrderKeeper);

    ctx->hPrivoxySvr = CreateThread(NULL, 0, PrivoxyThread, ctx, 0, &threadId);

    return ctx;
}

void ssr_client_terminate(struct ssr_client_ctx* ctx) {
    if (ctx) {
        ssr_run_loop_shutdown(ctx->state);
        WaitForSingleObject(ctx->hSsrClient, INFINITE);
        CloseHandle(ctx->hSsrClient);
        config_release(ctx->config);

        privoxy_shutdown();
        if (WAIT_OBJECT_0 != WaitForSingleObject(ctx->hPrivoxySvr, 1000)) {
            TerminateThread(ctx->hPrivoxySvr, -1);
        }
        CloseHandle(ctx->hPrivoxySvr);

        free(ctx);
    }
}

static void ssr_feedback_state(struct ssr_client_state *state, void *p) {
    struct ssr_client_ctx* ctx = (struct ssr_client_ctx*)p;
    SOCKET socket;
    state_set_force_quit(state, true); // force_quit flag
    socket = (SOCKET)ssr_get_listen_socket_fd(state);
    ctx->real_listen_port = retrieve_socket_port(socket);
    ctx->state = state;
    SetEvent(ctx->hOrderKeeper);
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

static DWORD WINAPI PrivoxyThread(LPVOID lpParam) {
    struct ssr_client_ctx* ctx = (struct ssr_client_ctx*)lpParam;
    char privoxy_config_file[MAX_PATH] = { 0 };
    char content[MAX_PATH * 2] = { 0 };
    HANDLE hFile;
    DWORD dwBytesToWrite = 0, dwBytesWritten = 0;
    char* p, * tmp = exe_file_path(&malloc);
    if (tmp && (p = strrchr(tmp, '\\'))) {
        *p = '\0';
        sprintf(privoxy_config_file, "%s/privoxy_config.txt", tmp);
        free(tmp);
    } else {
        assert(0);
    }

    assert(ctx->real_listen_port != 0);
    sprintf(content, PRIVOXY_CONFIG_CONTENT_FMT, PRIVOXY_LISTEN_PORT, ctx->real_listen_port);

    hFile = CreateFileA(privoxy_config_file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        assert(!"Terminal failure: Unable to open file for write.\n");
    }
    dwBytesToWrite = lstrlenA(content);
    WriteFile(hFile, content, dwBytesToWrite, &dwBytesWritten, NULL);
    CloseHandle(hFile);

    privoxy_main_entry(privoxy_config_file, NULL, NULL);

    return 0;
}
