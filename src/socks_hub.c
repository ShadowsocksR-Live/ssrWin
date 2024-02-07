#include <Windows.h>
#include <stdio.h>
#include <ssr_client_api.h>
#include <assert.h>
#include "run_ssr_client.h"

//
// https://github.com/ssrlive/socks-hub/releases
//

HINSTANCE hSocksHub = NULL;

bool socks_hub_lib_initialize(void) {
    if (hSocksHub != NULL) {
        return true;
    }
    hSocksHub = LoadLibraryW(L"socks_hub.dll");
    return (hSocksHub != NULL) ? true : false;
}

void socks_hub_lib_unload(void) {
    if (hSocksHub != NULL) {
        FreeLibrary(hSocksHub);
        hSocksHub = NULL;
    }
}

typedef enum ArgVerbosity {
    Off = 0,
    Error,
    Warn,
    Info,
    Debug,
    Trace,
} ArgVerbosity;

typedef enum SourceType {
    Http = 0,
    Socks5,
} SourceType;

typedef void (*p_sock_hub_log_callback)(enum ArgVerbosity, const char*, void*);
typedef void (*p_socks_hub_set_log_callback)(p_sock_hub_log_callback callback, void* ctx);

typedef int (*p_socks_hub_run)(enum SourceType type, const char* local_addr, const char* server_addr,
    enum ArgVerbosity verbosity,
    void (*callback)(int, void*),
    void* ctx);

typedef int (*p_socks_hub_stop)(void);

bool socks_hub_run_loop_begin(
    const char* local_addr,
    const char* server_addr,
    void(*listen_port_callback)(int listen_port, void* ctx),
    void* ctx)
{
    struct ssr_client_ctx* _ctx = (struct ssr_client_ctx*)ctx;
    const struct main_wnd_data* data = get_main_wnd_data_from_ctx(_ctx);
    p_log_callback callback = get_log_callback_ptr(data);
    int res = 0;
    p_socks_hub_run _socks_hub_run;
    p_socks_hub_set_log_callback _socks_hub_set_log_callback;

    _socks_hub_set_log_callback = (p_socks_hub_set_log_callback)GetProcAddress(hSocksHub, "socks_hub_set_log_callback");
    if (_socks_hub_set_log_callback == NULL) {
        return false;
    }
    // enable socks-tub log dumping
    _socks_hub_set_log_callback((p_sock_hub_log_callback)callback, (void*)data);

    _socks_hub_run = (p_socks_hub_run)GetProcAddress(hSocksHub, "socks_hub_run");
    if (_socks_hub_run == NULL) {
        return false;
    }
    // the socks-tub dead loop
    res = _socks_hub_run(Http, local_addr, server_addr, Info, listen_port_callback, ctx);
    assert(res == 0);

    _socks_hub_set_log_callback(NULL, NULL);

    return true;
}

void socks_hub_stop(void) {
    p_socks_hub_stop _socks_hub_stop;
    _socks_hub_stop = (p_socks_hub_stop)GetProcAddress(hSocksHub, "socks_hub_stop");
    if (_socks_hub_stop != NULL) {
        // stop socks-hub stop dead loop
        _socks_hub_stop();
    }
}
