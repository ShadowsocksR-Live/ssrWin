#include <json-c/json.h>
#include "settings_json.h"
#include "ssr_executive.h"

#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <assert.h>


bool json_iter_extract_object(const char* key, const struct json_object_iter* iter, const struct json_object** value) {
    bool result = false;
    do {
        struct json_object* val;
        if (key == NULL || iter == NULL || value == NULL) {
            break;
        }
        *value = NULL;
        if (strcmp(iter->key, key) != 0) {
            break;
        }
        val = iter->val;
        if (json_type_object != json_object_get_type(val)) {
            break;
        }
        *value = val; // json_object_get(val);
        result = true;
    } while (0);
    return result;
}

bool json_iter_extract_string(const char* key, const struct json_object_iter* iter, const char** value) {
    bool result = false;
    do {
        struct json_object* val;
        if (key == NULL || iter == NULL || value == NULL) {
            break;
        }
        *value = NULL;
        if (strcmp(iter->key, key) != 0) {
            break;
        }
        val = iter->val;
        if (json_type_string != json_object_get_type(val)) {
            break;
        }
        *value = json_object_get_string(val);
        result = true;
    } while (0);
    return result;
}

bool json_iter_extract_int(const char* key, const struct json_object_iter* iter, int* value) {
    bool result = false;
    do {
        struct json_object* val;

        if (key == NULL || iter == NULL || value == NULL) {
            break;
        }
        *value = 0;
        if (strcmp(iter->key, key) != 0) {
            break;
        }
        val = iter->val;
        if (json_type_int != json_object_get_type(val)) {
            break;
        }
        *value = json_object_get_int(val);
        result = true;
    } while (0);
    return result;
}

bool json_iter_extract_bool(const char* key, const struct json_object_iter* iter, bool* value) {
    bool result = false;
    do {
        struct json_object* val;
        if (key == NULL || iter == NULL || value == NULL) {
            break;
        }
        *value = false;
        if (strcmp(iter->key, key) != 0) {
            break;
        }
        val = iter->val;
        if (json_type_boolean != json_object_get_type(val)) {
            break;
        }
        *value = (bool)json_object_get_boolean(val);
        result = true;
    } while (0);
    return result;
}

struct server_config* parse_config_from_json(const json_object* jso);

void parse_settings_file(const char* file, json_config_fn fn, void* p)
{
    json_object* jso = NULL;
    do {
        size_t ii, c;

        jso = json_object_from_file(file);
        if (jso == NULL || json_object_get_type(jso) != json_type_array) {
            break;
        }

        c = json_object_array_length(jso);
        for (ii = 0; ii < c; ii++) {
            json_object* obj = json_object_array_get_idx(jso, ii);
            struct server_config* config = parse_config_from_json(obj);
            if (fn) {
                fn(config, p);
            }
            else {
                config_release(config);
            }
        }
    } while (0);
    if (jso) {
        json_object_put(jso);
    }
}

struct server_config* parse_config_from_json(const json_object* jso)
{
    bool result = false;
    struct server_config* config = NULL;
    do {
        struct json_object_iter iter = { NULL };

        if (jso == NULL) {
            break;
        }

        config = config_create();
        if (config == NULL) {
            break;
        }

        json_object_object_foreachC(jso, iter) {
            int obj_int = 0;
            bool obj_bool = false;
            const char* obj_str = NULL;
            const struct json_object* obj_obj = NULL;

            if (json_iter_extract_string("remarks", &iter, &obj_str)) {
                string_safe_assign(&config->remarks, obj_str);
                continue;
            }

            if (json_iter_extract_string("password", &iter, &obj_str)) {
                string_safe_assign(&config->password, obj_str);
                continue;
            }
            if (json_iter_extract_string("method", &iter, &obj_str)) {
                string_safe_assign(&config->method, obj_str);
                continue;
            }
            if (json_iter_extract_string("protocol", &iter, &obj_str)) {
                if (obj_str && strcmp(obj_str, "verify_sha1") == 0) {
                    obj_str = NULL;
                }
                string_safe_assign(&config->protocol, obj_str);
                continue;
            }
            if (json_iter_extract_string("protocol_param", &iter, &obj_str)) {
                string_safe_assign(&config->protocol_param, obj_str);
                continue;
            }
            if (json_iter_extract_string("obfs", &iter, &obj_str)) {
                string_safe_assign(&config->obfs, obj_str);
                continue;
            }
            if (json_iter_extract_string("obfs_param", &iter, &obj_str)) {
                string_safe_assign(&config->obfs_param, obj_str);
                continue;
            }

            if (json_iter_extract_bool("udp", &iter, &obj_bool)) {
                config->udp = obj_bool;
                continue;
            }
            if (json_iter_extract_int("idle_timeout", &iter, &obj_int)) {
                config->idle_timeout = obj_int * MILLISECONDS_PER_SECOND;
                continue;
            }
            if (json_iter_extract_int("connect_timeout", &iter, &obj_int)) {
                config->connect_timeout_ms = ((uint64_t)obj_int) * MILLISECONDS_PER_SECOND;
                continue;
            }
            if (json_iter_extract_int("udp_timeout", &iter, &obj_int)) {
                config->udp_timeout = ((uint64_t)obj_int) * MILLISECONDS_PER_SECOND;
                continue;
            }

            if (json_iter_extract_object("client_settings", &iter, &obj_obj)) {
                struct json_object_iter iter2 = { NULL };
                json_object_object_foreachC(obj_obj, iter2) {
                    const char* obj_str2 = NULL;

                    if (json_iter_extract_string("server", &iter2, &obj_str2)) {
                        string_safe_assign(&config->remote_host, obj_str2);
                        continue;
                    }
                    if (json_iter_extract_int("server_port", &iter2, &obj_int)) {
                        config->remote_port = obj_int;
                        continue;
                    }
                    if (json_iter_extract_string("listen_address", &iter2, &obj_str2)) {
                        string_safe_assign(&config->listen_host, obj_str2);
                        continue;
                    }
                    if (json_iter_extract_int("listen_port", &iter2, &obj_int)) {
                        config->listen_port = obj_int;
                        continue;
                    }
                }
                continue;
            }

            if (json_iter_extract_object("over_tls_settings", &iter, &obj_obj)) {
                struct json_object_iter iter2 = { NULL };
                json_object_object_foreachC(obj_obj, iter2) {
                    const char* obj_str2 = NULL;
                    obj_bool = false;

                    if (json_iter_extract_bool("enable", &iter2, &obj_bool)) {
                        config->over_tls_enable = obj_bool;
                        continue;
                    }
                    if (json_iter_extract_string("server_domain", &iter2, &obj_str2)) {
                        string_safe_assign(&config->over_tls_server_domain, obj_str2);
                        continue;
                    }
                    if (json_iter_extract_string("path", &iter2, &obj_str2)) {
                        string_safe_assign(&config->over_tls_path, obj_str2);
                        continue;
                    }
                    if (json_iter_extract_string("root_cert_file", &iter2, &obj_str2)) {
                        string_safe_assign(&config->over_tls_root_cert_file, obj_str2);
                        continue;
                    }
                }
                continue;
            }
        }
        result = true;
    } while (0);
    if (result == false) {
        config_release(config);
        config = NULL;
    }
    return config;
}

struct json_object* build_json_from_config(struct server_config* config) {
    struct json_object* jso = json_object_new_object();
    struct json_object* jso_client;
    struct json_object* jso_ot;

    json_object_object_add(jso, "remarks", json_object_new_string(config->remarks));
    json_object_object_add(jso, "password", json_object_new_string(config->password));
    json_object_object_add(jso, "method", json_object_new_string(config->method));
    json_object_object_add(jso, "protocol", json_object_new_string(config->protocol));
    json_object_object_add(jso, "protocol_param", json_object_new_string(config->protocol_param));
    json_object_object_add(jso, "obfs", json_object_new_string(config->obfs));
    json_object_object_add(jso, "obfs_param", json_object_new_string(config->obfs_param));
    json_object_object_add(jso, "udp", json_object_new_boolean(config->udp));
    json_object_object_add(jso, "idle_timeout", json_object_new_int((int32_t)(config->idle_timeout / MILLISECONDS_PER_SECOND)));
    json_object_object_add(jso, "connect_timeout", json_object_new_int((int32_t)(config->connect_timeout_ms / MILLISECONDS_PER_SECOND)));
    json_object_object_add(jso, "udp_timeout", json_object_new_int((int32_t)(config->udp_timeout / MILLISECONDS_PER_SECOND)));

    jso_client = json_object_new_object();
    json_object_object_add(jso_client, "server", json_object_new_string(config->remote_host));
    json_object_object_add(jso_client, "server_port", json_object_new_int((int32_t)config->remote_port));
    json_object_object_add(jso_client, "listen_address", json_object_new_string(config->listen_host));
    json_object_object_add(jso_client, "listen_port", json_object_new_int((int32_t)config->listen_port));
    json_object_object_add(jso, "client_settings", jso_client);

    jso_ot = json_object_new_object();
    json_object_object_add(jso_ot, "enable", json_object_new_boolean(config->over_tls_enable));
    json_object_object_add(jso_ot, "server_domain", json_object_new_string(config->over_tls_server_domain));
    json_object_object_add(jso_ot, "path", json_object_new_string(config->over_tls_path));
    json_object_object_add(jso, "over_tls_settings", jso_ot);

    return jso;
}

bool save_single_config_to_json_file(struct server_config* config, const char* file_path) {
    struct json_object* jso;
    int flags;
    if (config == NULL || file_path == NULL) {
        return false;
    }
    jso = build_json_from_config(config);
    if (jso == NULL) {
        return false;
    }
    flags = JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY;
    if (json_object_to_file_ext(file_path, jso, flags) == -1) {
        return false;
    }
    return true;
}

struct config_json_saver {
    struct json_object* jso_array;
    char* file_path;
};

struct config_json_saver* config_json_saver_create(const char* file_path) {
    struct config_json_saver* saver = (struct config_json_saver*)calloc(1, sizeof(*saver));
    if (saver == NULL) {
        return saver;
    }
    if (file_path == NULL) {
        free(saver);
        return NULL;
    }
    saver->jso_array = json_object_new_array();
    saver->file_path = strdup(file_path);

    return saver;
}

void config_json_saver_add_item(struct config_json_saver* saver, struct server_config* config) {
    if (config && saver && saver->jso_array) {
        struct json_object* jso = build_json_from_config(config);
        if (jso) {
            json_object_array_add(saver->jso_array, jso);
        }
    }
}

void config_json_saver_write_file(struct config_json_saver* saver) {
    if (saver && saver->jso_array) {
        int flags = JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY;
        json_object_to_file_ext(saver->file_path, saver->jso_array, flags);
    }
}

void config_json_saver_release(struct config_json_saver* saver) {
    if (saver) {
        if (saver->jso_array) {
            json_object_put(saver->jso_array);
        }
        if (saver->file_path) {
            free(saver->file_path);
        }
        free(saver);
    }
}
