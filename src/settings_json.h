#pragma once

#ifndef __SETTINGS_JSON_H__
#define __SETTINGS_JSON_H__

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct server_config;

typedef void(*json_config_fn)(struct server_config* config, void* p);
void parse_settings_file(const char* file, json_config_fn fn, void* p);

struct server_config* config_create_ssr_win(void);

struct config_json_saver;
struct config_json_saver* config_json_saver_create(const char* file_path);
void config_json_saver_add_item(struct config_json_saver* saver, struct server_config* config);
void config_json_saver_write_file(struct config_json_saver* saver);
void config_json_saver_release(struct config_json_saver* saver);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __SETTINGS_JSON_H__
