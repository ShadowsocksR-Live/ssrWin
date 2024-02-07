#pragma once

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdbool.h>

bool socks_hub_lib_initialize(void);
void socks_hub_lib_unload(void);

bool socks_hub_run_loop_begin(
    const char* local_addr,
    const char* server_addr,
    void(*listen_port_callback)(int listen_port, void* ctx),
    void* ctx);

void socks_hub_stop(void);

#ifdef  __cplusplus
}
#endif
