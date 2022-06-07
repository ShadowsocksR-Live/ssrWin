#pragma once

#ifndef __RUN_SSR_CLIENT_H__
#define __RUN_SSR_CLIENT_H__ 1

#ifdef __cplusplus
extern "C" {
#endif

#define SSR_DELAY_QUIT_MIN 500

#define PRIVOXY_LISTEN_ADDR "127.0.0.1"
#define PRIVOXY_LISTEN_PORT 8118

struct ssr_client_ctx;
struct server_config;

struct ssr_client_ctx* ssr_client_begin_run(struct server_config* config, const char* ssr_listen_host, int ssr_listen_port, const char* proxy_listen_host, int proxy_listen_port, int delay_quit_ms, int change_inet_opts);
void ssr_client_terminate(struct ssr_client_ctx* ctx);
const char* ssr_client_error_string(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __RUN_SSR_CLIENT_H__
