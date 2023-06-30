#include <WinSock2.h>
#include <Windows.h>
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#include <assert.h>
#include <stdio.h>
#include <privoxyexports.h>
#include <ssr_client_api.h>
#include "run_ssr_client.h"
#include "proxy_settings.h"
#include "server_connectivity.h"
#include "utf8_to_wchar.h"
#include "overtls.h"

#define PRIVOXY_CONFIG_CONTENT_FMT \
    "listen-address  %s:%d\r\n" \
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
    int delay_ms;
    uint16_t real_listen_port;
    char privoxy_listen_host[MAX_PATH];
    uint16_t privoxy_listen_port;
    const struct main_wnd_data* wnd_data;
};

const struct main_wnd_data* get_main_wnd_data_from_ctx(const struct ssr_client_ctx* ctx) {
    return ctx ? ctx->wnd_data : NULL;
}

static char error_info[MAX_PATH * 4] = { 0 };

const char* ssr_client_error_string(void) {
    return error_info;
}

struct ssr_client_ctx* ssr_client_begin_run(struct server_config* config, const struct main_wnd_data* data)
{
    struct ssr_client_ctx* ctx = NULL;
    DWORD threadId = 0;
    int error_code = 0;

    const char* ssr_listen_host = get_ssr_listen_host(data);
    int ssr_listen_port = get_ssr_listen_port(data);
    const char* proxy_listen_host = get_privoxy_listen_host(data);
    int proxy_listen_port = get_privoxy_listen_port(data);
    int delay_quit_ms = get_delay_quit_ms(data);
    int change_inet_opts = get_change_inet_opts(data);

    error_info[0] = '\0';

    if (ssr_listen_port != 0) {
        if (tcp_port_is_occupied("0.0.0.0", ssr_listen_port) ||
            tcp_port_is_occupied("127.0.0.1", ssr_listen_port))
        {
            wnsprintfA(error_info, ARRAYSIZE(error_info), "tcp_port_is_occupied for ssr_listen_port: %d\n", ssr_listen_port);
            return NULL;
        }
    }

    if (proxy_listen_port != 0) {
        if (tcp_port_is_occupied("0.0.0.0", proxy_listen_port) ||
            tcp_port_is_occupied("127.0.0.1", proxy_listen_port))
        {
            wnsprintfA(error_info, ARRAYSIZE(error_info), "tcp_port_is_occupied for proxy_listen_port: %d\n", proxy_listen_port);
            return NULL;
        }
    } else {
        // TODO: Privoxy not allow listen port is ZERO recently.
        DebugBreak();
        wnsprintfA(error_info, ARRAYSIZE(error_info), "tcp_port_is_occupied for proxy_listen_port: %d\n", proxy_listen_port);
        return NULL;
    }

    ctx = (struct ssr_client_ctx*)calloc(1, sizeof(*ctx));
    ctx->wnd_data = data;

    ctx->config = config_clone(config);

    // if we change listen_port to 0, then we will get a dynamic listen port.
    ctx->config->listen_port = (unsigned short)ssr_listen_port;
    string_safe_assign(&ctx->config->listen_host, ssr_listen_host);

    strcpy(ctx->privoxy_listen_host, proxy_listen_host);
    ctx->privoxy_listen_port = (uint16_t)proxy_listen_port;
    ctx->delay_ms = (delay_quit_ms < SSR_DELAY_QUIT_MIN) ? SSR_DELAY_QUIT_MIN : delay_quit_ms;

    ctx->hOrderKeeper = CreateEvent(NULL, TRUE, FALSE, NULL);
    ctx->hSsrClient = CreateThread(NULL, 0, SsrClientThread, ctx, 0, &threadId);
    WaitForSingleObject(ctx->hOrderKeeper, INFINITE);
    CloseHandle(ctx->hOrderKeeper);

    if (overtls_lib_initialize() && config_is_overtls(ctx->config)) {
        error_code = 0;
    }
    else {
        error_code = ssr_get_client_error_code(ctx->state);
    }
    if (error_code == 0) {
        ctx->hPrivoxySvr = CreateThread(NULL, 0, PrivoxyThread, ctx, 0, &threadId);
        if (change_inet_opts != 0) {
            wchar_t*p = utf8_to_wchar_string(ctx->privoxy_listen_host, &malloc);
            enable_system_proxy(p, ctx->privoxy_listen_port);
            free(p);
        } else {
            disable_system_proxy();
        }
    } else {
        WaitForSingleObject(ctx->hSsrClient, INFINITE);
        CloseHandle(ctx->hSsrClient);
        config_release(ctx->config);
        free(ctx);
        ctx = NULL;
        wnsprintfA(error_info, ARRAYSIZE(error_info), "SsrClientThread for error: %d\n", error_code);
    }

    return ctx;
}

void ssr_client_terminate(struct ssr_client_ctx* ctx) {
    if (ctx) {
        if (overtls_lib_initialize() && config_is_overtls(ctx->config)) {
            assert(ctx->state == NULL);
            overtls_client_stop();
        }else {
            ssr_run_loop_shutdown(ctx->state);
        }
        WaitForSingleObject(ctx->hSsrClient, INFINITE);
        CloseHandle(ctx->hSsrClient);
        config_release(ctx->config);

        privoxy_shutdown();
        WaitForSingleObject(ctx->hPrivoxySvr, INFINITE);
        CloseHandle(ctx->hPrivoxySvr);

        free(ctx);

        disable_system_proxy();
    }
}

static void ssr_feedback_state(struct ssr_client_state* state, void* p) {
    struct ssr_client_ctx* ctx = (struct ssr_client_ctx*)p;
    SOCKET socket;
    state_set_force_quit(state, true, ctx->delay_ms); // force_quit flag
    socket = (SOCKET)ssr_get_listen_socket_fd(state);
    ctx->real_listen_port = retrieve_socket_port(socket);
    ctx->state = state;
    SetEvent(ctx->hOrderKeeper);
}

static void listen_port_callback(int listen_port, void* p) {
    struct ssr_client_ctx* ctx = (struct ssr_client_ctx*)p;
    ctx->real_listen_port = listen_port;
    SetEvent(ctx->hOrderKeeper);
}

static DWORD WINAPI SsrClientThread(LPVOID lpParam) {
    struct ssr_client_ctx* ctx = (struct ssr_client_ctx*)lpParam;
    if (overtls_lib_initialize() && config_is_overtls(ctx->config)) {
        // run overtls thread
        if (false == overtls_run_loop_begin(ctx->config, &listen_port_callback, ctx)) {
            listen_port_callback(0, ctx);
        }
    }
    else {
        set_dump_info_callback(get_log_callback_ptr(ctx->wnd_data), (void*)ctx->wnd_data);
        ssr_run_loop_begin(ctx->config, &ssr_feedback_state, ctx);
        set_dump_info_callback(NULL, NULL);
    }
    return 0;
}

static uint16_t retrieve_socket_port(SOCKET socket) {
    char tmp[256] = { 0 }; // buffer size must big enough
    int len = sizeof(tmp);
    if (getsockname(socket, (struct sockaddr*)tmp, &len) != 0) {
        return 0;
    }
    else {
        return ntohs(((struct sockaddr_in*)tmp)->sin_port);
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
    }
    else {
        assert(0);
    }

    assert(ctx->real_listen_port != 0);
    sprintf(content, PRIVOXY_CONFIG_CONTENT_FMT, ctx->privoxy_listen_host, ctx->privoxy_listen_port, ctx->real_listen_port);

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
