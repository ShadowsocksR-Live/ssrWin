#pragma once

#ifndef __RUN_SSR_CLIENT_H__
#define __RUN_SSR_CLIENT_H__ 1

#ifdef __cplusplus
extern "C" {
#endif

struct ssr_client_ctx;
struct server_config;

struct ssr_client_ctx* ssr_client_begin_run(struct server_config* config);
void ssr_client_terminate(struct ssr_client_ctx* ctx);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __RUN_SSR_CLIENT_H__
