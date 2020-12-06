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


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __SETTINGS_JSON_H__
