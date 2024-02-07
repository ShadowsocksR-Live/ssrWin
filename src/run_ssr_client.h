#pragma once

#ifndef __RUN_SSR_CLIENT_H__
#define __RUN_SSR_CLIENT_H__ 1

#ifdef __cplusplus
extern "C" {
#endif

#define SSR_DELAY_QUIT_MIN 500

#define HTTP_PROXY_LISTEN_ADDR "127.0.0.1"
#define HTTP_PROXY_LISTEN_PORT 8118

struct ssr_client_ctx;
struct server_config;
struct main_wnd_data;

struct ssr_client_ctx* ssr_client_begin_run(struct server_config* config, const struct main_wnd_data* data);
void ssr_client_terminate(struct ssr_client_ctx* ctx);
const char* ssr_client_error_string(void);

const struct main_wnd_data* get_main_wnd_data_from_ctx(const struct ssr_client_ctx *ctx);

typedef void (*p_log_callback)(int log_level, const char* log, void* ctx);
p_log_callback get_log_callback_ptr(const struct main_wnd_data* data);

const char* get_ssr_listen_host(const struct main_wnd_data* data);
int get_ssr_listen_port(const struct main_wnd_data* data);
const char* get_http_proxy_listen_host(const struct main_wnd_data* data);
int get_http_proxy_listen_port(const struct main_wnd_data* data);
int get_delay_quit_ms(const struct main_wnd_data* data);
int get_change_inet_opts(const struct main_wnd_data* data);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __RUN_SSR_CLIENT_H__
