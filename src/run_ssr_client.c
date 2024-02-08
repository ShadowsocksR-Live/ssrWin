#include <WinSock2.h>
#include <Windows.h>
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#include <assert.h>
#include <stdio.h>
#include "socks_hub.h"
#include <ssr_client_api.h>
#include "run_ssr_client.h"
#include "proxy_settings.h"
#include "server_connectivity.h"
#include "utf8_to_wchar.h"
#include "overtls.h"

static DWORD WINAPI SsrClientThread(LPVOID lpParam);
static uint16_t retrieve_socket_port(SOCKET socket);
static DWORD WINAPI HttpProxyThread(LPVOID lpParam);

struct ssr_client_ctx {
    HANDLE hSsrClient;
    bool bOverTlsDll;
    HANDLE hHttpProxySvr;
    HANDLE hEvtS5Port;
    struct server_config* config;
    struct ssr_client_state* state;
    int delay_ms;
    uint16_t real_s5_listen_port;
    HANDLE hEvtHttpPort;
    uint16_t real_http_proxy_listen_port;
    char http_proxy_listen_host[MAX_PATH];
    uint16_t http_proxy_listen_port;
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
    const char* http_proxy_listen_host = get_http_proxy_listen_host(data);
    int http_proxy_listen_port = get_http_proxy_listen_port(data);
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

    if (http_proxy_listen_port != 0) {
        if (tcp_port_is_occupied("0.0.0.0", http_proxy_listen_port) ||
            tcp_port_is_occupied("127.0.0.1", http_proxy_listen_port))
        {
            wnsprintfA(error_info, ARRAYSIZE(error_info), "tcp_port_is_occupied for proxy_listen_port: %d\n", http_proxy_listen_port);
            return NULL;
        }
    }

    ctx = (struct ssr_client_ctx*)calloc(1, sizeof(*ctx));
    ctx->wnd_data = data;

    ctx->config = config_clone(config);

    // if we change listen_port to 0, then we will get a dynamic listen port.
    ctx->config->listen_port = (unsigned short)ssr_listen_port;
    string_safe_assign(&ctx->config->listen_host, ssr_listen_host);

    strcpy(ctx->http_proxy_listen_host, http_proxy_listen_host);
    ctx->http_proxy_listen_port = (uint16_t)http_proxy_listen_port;
    ctx->delay_ms = (delay_quit_ms < SSR_DELAY_QUIT_MIN) ? SSR_DELAY_QUIT_MIN : delay_quit_ms;

    if (overtls_lib_initialize()) {
        ctx->bOverTlsDll = true;
    }
    ctx->hEvtS5Port = CreateEvent(NULL, TRUE, FALSE, NULL);
    ctx->hSsrClient = CreateThread(NULL, 0, SsrClientThread, ctx, 0, &threadId);
    WaitForSingleObject(ctx->hEvtS5Port, INFINITE);
    CloseHandle(ctx->hEvtS5Port);
    ctx->hEvtS5Port = NULL;

    if (ctx->bOverTlsDll && config_is_overtls(ctx->config)) {
        error_code = 0;
    }
    else {
        error_code = ssr_get_client_error_code(ctx->state);
    }
    if (error_code == 0) {
        if (false == socks_hub_lib_initialize()) {
            DebugBreak();
            wnsprintfA(error_info, ARRAYSIZE(error_info), "missing dll file \"%s\"\n", "socks_hub.dll");
            // FIXME: resource leak here.
            return NULL;
        }
        ctx->hEvtHttpPort = CreateEvent(NULL, TRUE, FALSE, NULL);
        ctx->hHttpProxySvr = CreateThread(NULL, 0, HttpProxyThread, ctx, 0, &threadId);
        WaitForSingleObject(ctx->hEvtHttpPort, INFINITE);
        CloseHandle(ctx->hEvtHttpPort);
        ctx->hEvtHttpPort = NULL;
        if (change_inet_opts != 0 && ctx->real_http_proxy_listen_port != 0) {
            wchar_t*p = utf8_to_wchar_string(ctx->http_proxy_listen_host, &malloc);
            enable_system_proxy(p, ctx->real_http_proxy_listen_port);
            free(p);
        } else {
            disable_system_proxy();
        }
    } else {
        WaitForSingleObject(ctx->hSsrClient, INFINITE);
        CloseHandle(ctx->hSsrClient);
        ctx->hSsrClient = NULL;
        if (ctx->bOverTlsDll) {
            overtls_lib_unload();
        }
        config_release(ctx->config);
        free(ctx);
        ctx = NULL;
        wnsprintfA(error_info, ARRAYSIZE(error_info), "SsrClientThread for error: %d\n", error_code);
    }

    return ctx;
}

void ssr_client_terminate(struct ssr_client_ctx* ctx) {
    if (ctx) {
        if (ctx->bOverTlsDll && config_is_overtls(ctx->config)) {
            assert(ctx->state == NULL);
            overtls_client_stop();
        }else {
            ssr_run_loop_shutdown(ctx->state);
        }
        WaitForSingleObject(ctx->hSsrClient, INFINITE);
        CloseHandle(ctx->hSsrClient);
        ctx->hSsrClient = NULL;
        if (ctx->bOverTlsDll) {
            overtls_lib_unload();
        }
        config_release(ctx->config);

        socks_hub_stop();
        WaitForSingleObject(ctx->hHttpProxySvr, INFINITE);
        CloseHandle(ctx->hHttpProxySvr);
        ctx->hHttpProxySvr = NULL;
        socks_hub_lib_unload();

        free(ctx);

        disable_system_proxy();
    }
}

static void ssr_feedback_state(struct ssr_client_state* state, void* p) {
    struct ssr_client_ctx* ctx = (struct ssr_client_ctx*)p;
    SOCKET socket;
    state_set_force_quit(state, true, ctx->delay_ms); // force_quit flag
    socket = (SOCKET)ssr_get_listen_socket_fd(state);
    ctx->real_s5_listen_port = retrieve_socket_port(socket);
    ctx->state = state;
    SetEvent(ctx->hEvtS5Port);
}

static void s5_port_cb(int listen_port, void* p) {
    struct ssr_client_ctx* ctx = (struct ssr_client_ctx*)p;
    ctx->real_s5_listen_port = listen_port;
    SetEvent(ctx->hEvtS5Port);
}

static DWORD WINAPI SsrClientThread(LPVOID lpParam) {
    struct ssr_client_ctx* ctx = (struct ssr_client_ctx*)lpParam;
    if (ctx->bOverTlsDll && config_is_overtls(ctx->config)) {
        // run overtls thread
        if (false == overtls_run_loop_begin(ctx->config, &s5_port_cb, ctx)) {
            s5_port_cb(0, ctx);
        }
    }
    else {
        typedef void(*ssr_callback)(int dump_level, const char* info, void* p);
        ssr_callback cb = (ssr_callback)get_log_callback_ptr(ctx->wnd_data);
        set_dump_info_callback(cb, (void*)ctx->wnd_data);
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

static void http_port_cb(int listen_port, void* p) {
    struct ssr_client_ctx* ctx = (struct ssr_client_ctx*)p;
    ctx->real_http_proxy_listen_port = listen_port;
    SetEvent(ctx->hEvtHttpPort);
}

static DWORD WINAPI HttpProxyThread(LPVOID lpParam) {
    struct ssr_client_ctx* ctx = (struct ssr_client_ctx*)lpParam;
    char http_proxy_addr[MAX_PATH] = { 0 };
    char socks5_proxy_addr[MAX_PATH] = { 0 };

    assert(ctx->real_s5_listen_port != 0);
    sprintf(http_proxy_addr, "%s:%d", ctx->http_proxy_listen_host, ctx->http_proxy_listen_port);
    sprintf(socks5_proxy_addr, "127.0.0.1:%d", ctx->real_s5_listen_port);

    if (false == socks_hub_run_loop_begin(http_proxy_addr, socks5_proxy_addr, http_port_cb, ctx)) {
        http_port_cb(0, ctx);
    }

    return 0;
}
