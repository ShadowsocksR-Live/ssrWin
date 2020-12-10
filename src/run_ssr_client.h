#pragma once

#ifndef __RUN_SSR_CLIENT_H__
#define __RUN_SSR_CLIENT_H__ 1

#ifdef __cplusplus
extern "C" {
#endif

struct server_config;

void run_ssr_client(struct server_config* config);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __RUN_SSR_CLIENT_H__
