#pragma once


bool overtls_lib_initialize(void);
void overtls_lib_unload(void);

bool overtls_run_loop_begin(const struct server_config* cf,
    void(*listen_port_callback)(int listen_port, void* ctx),
    void* ctx);
void overtls_client_stop(void);
