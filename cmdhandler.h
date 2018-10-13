#ifndef __CMD_HANDLER_H__
#define __CMD_HANDLER_H__

#define CMD_BUFF_SIZE 128

#define s_action "action"
#define s_configureRouting "configureRouting"
#define s_resetRouting "resetRouting"
#define s_parameters "parameters"
#define s_proxyIp "proxyIp"
#define s_routerIp "routerIp"
#define s_isAutoConnnect "isAutoConnnect"
#define s_statusCode "statusCode"
#define s_errorMessage "errorMessage"


struct service_request {
    char action[CMD_BUFF_SIZE];
    char proxyIp[CMD_BUFF_SIZE];
    char routerIp[CMD_BUFF_SIZE];
    int isAutoConnnect;
};

enum error_code {
    e_c_success = 0,
    e_c_genericFailure = 1,
    e_c_unsupported_routing_table = 2
};

struct service_response {
    enum error_code statusCode;
    char errorMessage[CMD_BUFF_SIZE];
};

void parse_request(const char *json, struct service_request *request);
void build_response(int code, const char *msg, char *out_buf, size_t size);

#endif // __CMD_HANDLER_H__
