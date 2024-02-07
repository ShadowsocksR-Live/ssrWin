#include <Windows.h>
#include <stdio.h>
#include <ssr_client_api.h>
#include <assert.h>
#include "run_ssr_client.h"

HINSTANCE hOverTls = NULL;

bool overtls_lib_initialize(void) {
    if (hOverTls != NULL) {
        return true;
    }
    hOverTls = LoadLibraryW(L"overtls.dll");
    return (hOverTls != NULL) ? true : false;
}

void overtls_lib_unload(void) {
    if (hOverTls != NULL) {
        FreeLibrary(hOverTls);
        hOverTls = NULL;
    }
}

typedef int (*p_over_tls_client_run)(const char* config_path,
    char verbose,
    void (*callback)(int listen_port, void* ctx),
    void* ctx);
typedef int (*p_over_tls_client_stop)(void);
typedef void (*p_overtls_set_log_callback)(void (*callback)(int log_level, const char* log, void* ctx), void* ctx);

const char* get_temp_json_file_path(char json_file_name[MAX_PATH * 2]);
bool write_content_to_file(const char* content, const char* file_path);

bool overtls_run_loop_begin(const struct server_config* cf,
    void(*listen_port_callback)(int listen_port, void* ctx),
    void* ctx)
{
    struct ssr_client_ctx* _ctx = (struct ssr_client_ctx*)ctx;
    const struct main_wnd_data* data = get_main_wnd_data_from_ctx(_ctx);
    p_log_callback callback = get_log_callback_ptr(data);
    char* content = NULL;
    int res = 0;
    p_over_tls_client_run over_tls_client_run;
    p_overtls_set_log_callback overtls_set_log_callback;
    char json_file_name[MAX_PATH * 2] = { 0 };

    get_temp_json_file_path(json_file_name);

    overtls_set_log_callback = (p_overtls_set_log_callback)GetProcAddress(hOverTls, "overtls_set_log_callback");
    if (overtls_set_log_callback == NULL) {
        return false;
    }
    // enable overtls log dumping
    overtls_set_log_callback(callback, (void*)data);

    content = config_convert_to_overtls_json(cf, &malloc);
    if (content == NULL) {
        return false;
    }
    if (false == write_content_to_file(content, json_file_name)) {
        free(content);
        return false;
    }
    free(content);

    over_tls_client_run = (p_over_tls_client_run)GetProcAddress(hOverTls, "over_tls_client_run");
    if (over_tls_client_run == NULL) {
        return false;
    }
    // the overtls dead loop
    res = over_tls_client_run(json_file_name, (char)TRUE, listen_port_callback, ctx);
    assert(res == 0);

    overtls_set_log_callback(NULL, NULL);

    return true;
}

const char* get_temp_json_file_path(char json_file_name[MAX_PATH * 2]) {
    char temp_path[MAX_PATH];
    char temp_file_name[MAX_PATH];

    GetTempPathA(MAX_PATH, temp_path);
    GetTempFileNameA(temp_path, "temp", 0, temp_file_name);
    sprintf(json_file_name, "%s.json", temp_file_name);
    return json_file_name;
}

bool write_content_to_file(const char* content, const char* file_path) {
    HANDLE hFile;
    DWORD bytesWritten = 0;
    if (content == NULL || file_path == NULL) {
        return false;
    }
    hFile = CreateFileA(file_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    if (!WriteFile(hFile, content, (DWORD)strlen(content), &bytesWritten, NULL)) {
        CloseHandle(hFile);
        return false;
    }
    CloseHandle(hFile);
    return true;
}

void overtls_client_stop(void) {
    p_over_tls_client_stop over_tls_client_stop;
    over_tls_client_stop = (p_over_tls_client_stop)GetProcAddress(hOverTls, "over_tls_client_stop");
    if (over_tls_client_stop != NULL) {
        // stop overtls client dead loop
        over_tls_client_stop();
    }
}
