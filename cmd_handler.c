#include "net_adapter_enum.h"
#include <tchar.h>
#include <windows.h>

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <tchar.h>

#include "cmd_handler.h"
#include "json.h"
#include "smartdnsblock.h"

/*
 * Windows Service, part of the SSR Windows client, to configure routing.
 * Modifying the system routes requires admin permissions, so this service must be installed
 * and started as admin.
 *
 * The service listens on a named pipe and supports the following JSON API:
 *
 * Requests
 *
 * configureRouting: Modifies the system's routing table to route all traffic through the TAP device
 * except that destined for proxyIp. Disables IPv6 traffic.
 *    { action: "configureRouting", parameters: {"proxyIp": <IPv4 address>, "isAutoConnect": "false" }}
 *
 * resetRouting: Restores the system's default routing.
 *    { action: "resetRouting"}
 *
 * Response
 *
 *  { statusCode: <int>, action: <string>, errorMessage?: <string> }
 *  
 *  The service will send connection status updates if the pipe connection is kept
 *  open by the client. Such responses have the form:
 *  
 *  { statusCode: <int>, action: "statusChanged", connectionStatus: <int> }
 */

#define CMD_BUFF_SIZE 128

#define s_action "action"
#define s_configureRouting "configureRouting"
#define s_resetRouting "resetRouting"
#define s_statusChanged "statusChanged"
#define s_parameters "parameters"
#define s_proxyIp "proxyIp"
#define s_isAutoConnect "isAutoConnect"
#define s_statusCode "statusCode"
#define s_errorMessage "errorMessage"

struct service_request {
    char action[CMD_BUFF_SIZE];
    char proxyIp[CMD_BUFF_SIZE];
    int isAutoConnect;
};

enum error_code {
    e_c_success = 0,
    e_c_genericFailure = 1,
    e_c_unsupported_routing_table = 2,
};

struct service_response {
    char action[CMD_BUFF_SIZE];
    enum error_code statusCode;
    char errorMessage[CMD_BUFF_SIZE];
    int connectionStatus;
};

void parse_request(const char* json, struct service_request* request);
int handle_request(struct service_request* request);
void build_response(int code, const char* msg, char* out_buf, size_t size);

static const wchar_t* IPV4_SUBNETS[] = {
    L"0.0.0.0/1",
    L"128.0.0.0/1",
};
static const wchar_t* IPV6_SUBNETS[] = {
    L"fc00::/7",
    L"2000::/4",
    L"3000::/4",
};
static const wchar_t* IPV4_RESERVED_SUBNETS[] = {
    L"0.0.0.0/8",
    L"10.0.0.0/8",
    L"100.64.0.0/10",
    L"169.254.0.0/16",
    L"172.16.0.0/12",
    L"192.0.0.0/24",
    L"192.0.2.0/24",
    L"192.31.196.0/24",
    L"192.52.193.0/24",
    L"192.88.99.0/24",
    L"192.168.0.0/16",
    L"192.175.48.0/24",
    L"198.18.0.0/15",
    L"198.51.100.0/24",
    L"203.0.113.0/24",
    L"240.0.0.0/4",
};
static const wchar_t* CMD_NETSH = L"netsh";
static const wchar_t* CMD_ROUTE = L"route";

struct router_info {
    wchar_t tapDeviceName[MAX_PATH / 4];
    wchar_t proxyIp[MAX_PATH];
    wchar_t gatewayIp[MAX_PATH];
    wchar_t gatewayInterfaceName[MAX_PATH];
};

struct router_info g_router_info = { 0 };

void pick_live_adapter(int* stop, PIP_ADAPTER_ADDRESSES pCurrAddresses, void* p);

int configure_routing(const char* proxyIp, int isAutoConnect);
int reset_routing(const wchar_t* proxyIp, const wchar_t* proxyInterfaceName, const wchar_t* tapDeviceName);
int run_command_wrapper(const wchar_t* cmd, const wchar_t* argv_fmt, ...);
int delete_proxy_route(const wchar_t* proxyIp, const wchar_t* proxyInterfaceName);
void set_gateway_properties(const wchar_t* p);
int add_proxy_route(const struct router_info* router);
int add_ipv4_redirect(const struct router_info* router);
int stop_routing_ipv6(void);
int remove_ipv4_redirect(const wchar_t* tapDeviceName);
int start_routing_ipv6(void);
int add_reserved_subnet_bypass(const struct router_info* router);
int remove_reserved_subnet_bypass(const wchar_t* interfaceName);
int run_command(const wchar_t* cmd, const wchar_t* args);

BOOL svc_message_handler(const BYTE* msg, size_t msg_size, BYTE* result, size_t* result_size, void* p)
{
    struct service_request request = { 0 };
    size_t size = *result_size;
    enum error_code code = e_c_success;
    parse_request((char*)msg, &request);
    if (handle_request(&request) != 0) {
        code = e_c_genericFailure;
    }
    build_response(code, "", (char*)result, size);
    *result_size = lstrlenA((char*)result);
    return TRUE;
}

static void process_json_generic(struct json_object* value, struct service_request* request)
{
    int length, x;
    assert(value);
    assert(value->type == json_type_object);
    length = value->u.object.length;
    for (x = 0; x < length; x++) {
        struct json_object_entry* entry = value->u.object.entries + x;

        if (strcmp(entry->name, s_action) == 0) {
            assert(entry->object->type == json_type_string);
            strcpy(request->action, entry->object->u.string.ptr);
            continue;
        }

        if (strcmp(entry->name, s_parameters) == 0) {
            size_t len2, y;
            struct json_object* sub2 = entry->object;
            assert(sub2->type == json_type_object);
            len2 = sub2->u.object.length;
            for (y = 0; y < len2; ++y) {
                struct json_object_entry* entry2 = sub2->u.object.entries + y;
                if (strcmp(entry2->name, s_proxyIp) == 0) {
                    assert(entry2->object->type == json_type_string);
                    strcpy(request->proxyIp, entry2->object->u.string.ptr);
                    continue;
                }
                if (strcmp(entry2->name, s_isAutoConnect) == 0) {
                    assert(entry2->object->type == json_type_boolean);
                    request->isAutoConnect = entry2->object->u.boolean;
                    continue;
                }
            }
            continue;
        }
    }
}

void parse_request(const char* json, struct service_request* request)
{
    struct json_object* value;
    value = json_parse((json_char*)json, strlen(json));
    if (value) {
        process_json_generic(value, request);
        json_object_free(value);
    }
}

int handle_request(struct service_request* request)
{
    int result = -1;
    if (strcmp(request->action, s_configureRouting) == 0) {
        result = configure_routing(request->proxyIp, request->isAutoConnect);
    }
    if (strcmp(request->action, s_resetRouting) == 0) {
        struct router_info* pInfo = &g_router_info;
        result = reset_routing(pInfo->proxyIp, pInfo->gatewayInterfaceName, pInfo->tapDeviceName);
        ZeroMemory(pInfo, sizeof(struct router_info));
    }
    return result;
}

int configure_routing(const char* proxyIp, int isAutoConnect)
{
    int result = -1;
    (void)isAutoConnect;
    do {
        struct router_info* pInfo = &g_router_info;
        wchar_t tmp[MAX_PATH] = { 0 };

        result = begin_smart_dns_block(TAP_DEVICE_NAME, FILTER_PROVIDER_NAME);
        if (result != 0) {
            break;
        }

        if (proxyIp == NULL || strlen(proxyIp) == 0) {
            break;
        }

        lstrcpyW(pInfo->tapDeviceName, TAP_DEVICE_NAME);

        enum_adapter_info(AF_UNSPEC, pick_live_adapter, pInfo);

        lstrcpyW(pInfo->proxyIp, utf8_to_wchar_string(proxyIp, tmp, ARRAYSIZE(tmp)));

        result = run_command_wrapper(CMD_NETSH, L"interface ip set interface %s metric=0", pInfo->tapDeviceName);
        if (result != 0) {
            break;
        }

        if (lstrlenW(pInfo->gatewayIp) != 0) {
            if ((result = add_proxy_route(pInfo)) != 0) {
                break;
            }
            if ((result = add_ipv4_redirect(pInfo)) != 0) {
                break;
            }
            add_reserved_subnet_bypass(pInfo);
        }
        if ((result = stop_routing_ipv6()) != 0) {
            break;
        }
    } while (0);
    return result;
}

int reset_routing(const wchar_t* proxyIp, const wchar_t* proxyInterfaceName, const wchar_t* tapDeviceName)
{
    if (proxyIp && lstrlenW(proxyIp)) {
        if (delete_proxy_route(proxyIp, proxyInterfaceName) != 0) {
            // log("failed to remove route to the proxy server: {e.Message}")
        }
    } else {
        // log("cannot remove route to proxy server, have not previously set")
    }
    remove_ipv4_redirect(tapDeviceName);
    remove_reserved_subnet_bypass(proxyInterfaceName);
    start_routing_ipv6();

    end_smart_dns_block();

    return 0;
}

void send_connection_status_change(int status)
{
    struct service_response resp = { { 0 }, e_c_success };
    strcpy(resp.action, s_statusChanged);
    resp.connectionStatus = status;
}

int run_command_wrapper(const wchar_t* cmd, const wchar_t* argv_fmt, ...)
{
    wchar_t buffer[MAX_PATH * 2] = { 0 };
    va_list arg;
    va_start(arg, argv_fmt);
    vswprintf(buffer, ARRAYSIZE(buffer), argv_fmt, arg);
    va_end(arg);
    return run_command(cmd, buffer);
}

void set_gateway_properties(const wchar_t* p)
{
}

int add_proxy_route(const struct router_info* router)
{
    const wchar_t* fmt1 = L"interface ipv4 add route %s/32 nexthop=%s interface=\"%s\" metric=0";
    const wchar_t* fmt2 = L"interface ipv4 set route %s/32 nexthop=%s interface=\"%s\" metric=0";
    int result = run_command_wrapper(CMD_NETSH, fmt1,
        router->proxyIp, router->gatewayIp, router->gatewayInterfaceName);
    if (result != 0) {
        result = run_command_wrapper(CMD_NETSH, fmt2,
            router->proxyIp, router->gatewayIp, router->gatewayInterfaceName);
    }
    return result;
}

int delete_proxy_route(const wchar_t* proxyIp, const wchar_t* proxyInterfaceName)
{
    static const wchar_t* fmt = L"interface ipv4 delete route %s/32 interface=\"%s\"";
    return run_command_wrapper(CMD_NETSH, fmt, proxyIp, proxyInterfaceName);
}

int add_ipv4_redirect(const struct router_info* router)
{
    const wchar_t* fmt = L"interface ipv4 add route %s nexthop=%s interface=\"%s\" metric=0";
    int result = 0;
    int i = 0;
    for (i = 0; i < ARRAYSIZE(IPV4_SUBNETS); ++i) {
        const wchar_t* subnet = IPV4_SUBNETS[i];
        result = run_command_wrapper(CMD_NETSH, fmt, subnet, /*router->routerIp, */ router->tapDeviceName);
        if (result != 0) {
            // "could not change default gateway: {0}"
            break;
        }
    }
    return result;
}

int remove_ipv4_redirect(const wchar_t* tapDeviceName)
{
    const wchar_t* fmt = L"interface ipv4 delete route %s interface=%s";
    int i = 0;
    int result = 0;
    for (i = 0; i < ARRAYSIZE(IPV4_SUBNETS); ++i) {
        result = run_command_wrapper(CMD_NETSH, fmt, IPV4_SUBNETS[i], tapDeviceName);
        if (result != 0) {
            // "failed to remove {0}: {1}"
            break;
        }
    }
    return result;
}

int stop_routing_ipv6(void)
{
    const wchar_t* fmt = L"interface ipv6 add route %s interface=%d metric=0";
    int result = 0;
    int i = 0;
    int index = IPv6LoopbackInterfaceIndex();
    for (i = 0; i < ARRAYSIZE(IPV6_SUBNETS); ++i) {
        result = run_command_wrapper(CMD_NETSH, fmt, IPV6_SUBNETS[i], index);
        if (result != 0) {
            // "could not disable IPv6: {0}"
            break;
        }
    }
    return result;
}

int start_routing_ipv6(void)
{
    const wchar_t* fmt = L"interface ipv6 delete route %s interface=%d";
    int result = 0, i = 0, index = IPv6LoopbackInterfaceIndex();
    for (i = 0; i < ARRAYSIZE(IPV6_SUBNETS); ++i) {
        result = run_command_wrapper(CMD_NETSH, fmt, IPV6_SUBNETS[i], index);
        if (result != 0) {
            // "failed to remove {0}: {1}"
            break;
        }
    }
    return result;
}

// Routes reserved and private subnets through the default gateway so they bypass the VPN.
int add_reserved_subnet_bypass(const struct router_info* router)
{
    const wchar_t* fmt = L"interface ipv4 add route %s nexthop=%s interface=\"%s\" metric=0";
    int i, result = -1;
    for (i = 0; i < ARRAYSIZE(IPV4_RESERVED_SUBNETS); ++i) {
        result = run_command_wrapper(CMD_NETSH, fmt, IPV4_RESERVED_SUBNETS[i], router->gatewayIp, router->gatewayInterfaceName);
        if (result != 0) {
            break;
        }
    }
    return result;
}

// Removes reserved subnet routes created to bypass the VPN.
int remove_reserved_subnet_bypass(const wchar_t* interfaceName)
{
    const wchar_t* fmt = L"interface ipv4 delete route %s interface=\"%s\"";
    int i, result = -1;
    for (i = 0; i < ARRAYSIZE(IPV4_RESERVED_SUBNETS); ++i) {
        result = run_command_wrapper(CMD_NETSH, fmt, IPV4_RESERVED_SUBNETS[i], interfaceName);
        if (result != 0) {
            break;
        }
    }
    return result;
}

void build_response(int code, const char* msg, char* out_buf, size_t size)
{
    sprintf(out_buf, "{ \"statusCode\": %d, \"errorMessage\": \"%s\" }", code, msg);
}

int run_command(const wchar_t* cmd, const wchar_t* args)
{
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD exit_code = 0;
    size_t len = 0;
    wchar_t* buffer = NULL;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    len = lstrlenW(cmd) + lstrlenW(args) + 10;
    buffer = (wchar_t*)calloc(len, sizeof(wchar_t));
    if (cmd[0] == L'"') {
        swprintf(buffer, len, L"%s %s", cmd, args);
    } else {
        swprintf(buffer, len, L"\"%s\" %s", cmd, args);
    }

    // Start the child process.
    if (!CreateProcessW(NULL, // No module name (use command line)
            buffer, // Command line
            NULL, // Process handle not inheritable
            NULL, // Thread handle not inheritable
            FALSE, // Set handle inheritance to FALSE
            0, // No creation flags
            NULL, // Use parent's environment block
            NULL, // Use parent's starting directory
            &si, // Pointer to STARTUPINFO structure
            &pi) // Pointer to PROCESS_INFORMATION structure
    ) {
        wprintf(L"CreateProcess failed (%d).\n", GetLastError());
        free(buffer);
        return -1;
    }

    // Wait until child process exits.
    WaitForSingleObject(pi.hProcess, INFINITE);

    GetExitCodeProcess(pi.hProcess, &exit_code);

    // Close process and thread handles.
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    free(buffer);

    return (int)exit_code;
}

static void interfaces_with_ipv4_gateways(PIP_ADAPTER_ADDRESSES pCurrAddresses, void* p)
{
    PIP_ADAPTER_UNICAST_ADDRESS pUnicast = NULL;
    PIP_ADAPTER_ANYCAST_ADDRESS pAnycast = NULL;
    PIP_ADAPTER_MULTICAST_ADDRESS pMulticast = NULL;
    IP_ADAPTER_DNS_SERVER_ADDRESS* pDnServer = NULL;
    PIP_ADAPTER_GATEWAY_ADDRESS_LH gateway = NULL;
    IP_ADAPTER_PREFIX* pPrefix = NULL;
    wchar_t s[INET6_ADDRSTRLEN] = { 0 };
    int i;

    struct router_info* info = (struct router_info*)p;

    lstrcpyW(info->gatewayInterfaceName, pCurrAddresses->FriendlyName);

    gateway = pCurrAddresses->FirstGatewayAddress;
    while (gateway) {
        if (gateway->Address.lpSockaddr->sa_family == AF_INET) {
            draft_inet_ntop(gateway->Address.lpSockaddr, s, ARRAYSIZE(s));
            lstrcpyW(info->gatewayIp, s);
        }
        gateway = gateway->Next;
    }

    i = 0;
    pUnicast = pCurrAddresses->FirstUnicastAddress;
    while (pUnicast != NULL) {
        printf("\t\tUnicast Addresses %d: %wS\n",
            i, draft_inet_ntop(pUnicast->Address.lpSockaddr, s, ARRAYSIZE(s)));
        pUnicast = pUnicast->Next;
        ++i;
    }
}

void pick_live_adapter(int* stop, PIP_ADAPTER_ADDRESSES pCurrAddresses, void* p)
{
    struct router_info* info = (struct router_info*)p;
    unsigned int i = 0;
    PIP_ADAPTER_GATEWAY_ADDRESS_LH gateway = NULL;

    if (pCurrAddresses->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
        return;
    }
    if (lstrcmpW(pCurrAddresses->FriendlyName, info->tapDeviceName) == 0) {
        return;
    }
    if (pCurrAddresses->OperStatus != IfOperStatusUp) {
        return;
    }

    i = 0;
    gateway = pCurrAddresses->FirstGatewayAddress;
    while (gateway) {
        ++i;
        if (gateway->Address.lpSockaddr->sa_family == AF_INET) {
            interfaces_with_ipv4_gateways(pCurrAddresses, p);
            if (stop) {
                *stop = TRUE;
            }
            return;
        }
        gateway = gateway->Next;
    }
}
