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
#include "win_cmd_wrapper.h"
#include "utf8_to_wchar.h"
#include "net_change_monitor.h"

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
int handle_request(struct router_info* router, struct service_request* request);
void build_response(const char* action, int code, const char* msg, char* out_buf, size_t size);

static const wchar_t* TAP_DEVICE_GATEWAY_IP = L"10.0.85.1";

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
    wchar_t tapGatewayIp[MAX_PATH];
    int tapInterfaceIndex;
    wchar_t remoteProxyIp[MAX_PATH];
    wchar_t gatewayIp[MAX_PATH];
    wchar_t gatewayInterfaceName[MAX_PATH];
    int gatewayInterfaceIndex;
    struct net_change_ctx* monitor;
    int shut_down;
};

struct router_info g_router_info = { 0 };
void* handler_data_p = (void*)&g_router_info;

void pick_live_adapter(int* stop, PIP_ADAPTER_ADDRESSES pCurrAddresses, void* p);
void get_system_ipv4_gateway(int* stop, int total, const MIB_IPFORWARDROW* row, void* p);
void retrieve_tap_gateway_ip(int* stop, PIP_ADAPTER_ADDRESSES pCurrAddresses, void* p);

int configure_routing(struct router_info* pInfo, const char* proxyIp, int isAutoConnect);
int reset_routing(const struct router_info* pInfo);
int delete_proxy_route(const wchar_t* proxyIp);
void set_gateway_properties(const wchar_t* p);
int add_ipv4_redirect(const struct router_info* router);
int stop_routing_ipv6(void);
int add_or_update_proxy_route(const struct router_info* router);
int add_or_update_reserved_subnet_bypass(const struct router_info* router);
int remove_ipv4_redirect(const wchar_t* tapDeviceName);
int start_routing_ipv6(void);
void remove_reserved_subnet_bypass(void);

BOOL svc_message_handler(const BYTE* msg, size_t msg_size, BYTE* result, size_t* result_size, void* p)
{
    struct router_info *router = (struct router_info*)p;
    struct service_request request = { 0 };
    size_t size = result_size ? (*result_size) : 0;
    enum error_code code = e_c_success;

    if (msg == NULL) {
        // force exiting because of pipe broken.
        lstrcpyA(request.action, s_resetRouting);
        handle_request(router, &request);
        return TRUE;
    }
    parse_request((char*)msg, &request);
    if (handle_request(router, &request) != 0) {
        code = e_c_genericFailure;
    }
    build_response(request.action, code, "", (char*)result, size);
    *result_size = lstrlenA((char*)result);
    if (router->shut_down) {
        router->shut_down = 0;
        return FALSE;
    }
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

static void net_change_notification(void* p) {
    wchar_t tmp[MAX_PATH * 2] = { 0 };
    struct router_info* pInfo = (struct router_info*)p;
    swprintf(tmp, ARRAYSIZE(tmp), L"NotifyAddrChange %s\n", pInfo->tapDeviceName);
    OutputDebugStringW(tmp);
}

int handle_request(struct router_info* pInfo, struct service_request* request)
{
    int result = -1;
    net_status_stop_monitor(pInfo->monitor);
    pInfo->monitor = NULL;
    if (strcmp(request->action, s_configureRouting) == 0) {
        result = configure_routing(pInfo, request->proxyIp, request->isAutoConnect);
        pInfo->monitor = net_status_start_monitor(net_change_notification, pInfo);
    }
    if (strcmp(request->action, s_resetRouting) == 0) {
        if (lstrlenW(pInfo->remoteProxyIp) == 0) {
            pInfo->shut_down = 1;
            result = 0;
        } else {
            result = reset_routing(pInfo);
            ZeroMemory(pInfo, sizeof(struct router_info));
        }
    }
    return result;
}

#define STUPID_WAY_GET_GATEWAY 1

struct GET_IPFORWARDROW {
    int tapInterfaceIndex;
    MIB_IPFORWARDROW best_row;
};

int configure_routing(struct router_info* pInfo, const char* proxyIp, int isAutoConnect)
{
    int result = -1;
    (void)isAutoConnect;
    do {
        wchar_t tmp[MAX_PATH] = { 0 };

        if (proxyIp == NULL || strlen(proxyIp) == 0) {
            break;
        }

        result = begin_smart_dns_block(TAP_DEVICE_NAME, FILTER_PROVIDER_NAME);
        if (result != 0) {
            break;
        }

        lstrcpyW(pInfo->tapDeviceName, TAP_DEVICE_NAME);
        lstrcpyW(pInfo->remoteProxyIp, utf8_to_wchar_string(proxyIp, tmp, ARRAYSIZE(tmp)));

        enum_adapter_info(AF_UNSPEC, retrieve_tap_gateway_ip, pInfo);
        if (lstrlenW(pInfo->tapGatewayIp) == 0) {
            lstrcpyW(pInfo->tapGatewayIp, TAP_DEVICE_GATEWAY_IP);
        }

        enum_adapter_info(AF_UNSPEC, pick_live_adapter, pInfo);
#if (STUPID_WAY_GET_GATEWAY)
        {
            static const MIB_IPFORWARDROW dummy = { 0 };
            struct in_addr gatewayIp;

            struct GET_IPFORWARDROW tmp = { pInfo->tapInterfaceIndex, { 0 } };
            enum_ip_forward_table(get_system_ipv4_gateway, &tmp);
            if (memcmp(&tmp.best_row, &dummy, sizeof(dummy)) == 0) {
                break;
            }

            pInfo->gatewayInterfaceIndex = (int)tmp.best_row.dwForwardIfIndex;

            gatewayIp.s_addr = (ULONG)tmp.best_row.dwForwardNextHop;
            utf8_to_wchar_string(inet_ntoa(gatewayIp), pInfo->gatewayIp, ARRAYSIZE(pInfo->gatewayIp));
        }
#endif

        if ((result = add_ipv4_redirect(pInfo)) != 0) {
            break;
        }

        if ((result = stop_routing_ipv6()) != 0) {
            break;
        }

        if ((result = add_or_update_proxy_route(pInfo)) != 0) {
            break;
        }

        if ((result = add_or_update_reserved_subnet_bypass(pInfo)) != 0) {
            break;
        }

    } while (0);

    if (result != 0) {
        end_smart_dns_block();
    }

    return result;
}

int reset_routing(const struct router_info* router)
{
    int result = -1;
    do {
        if (router == NULL || lstrlenW(router->tapDeviceName) == 0) {
            result = 0;
            break;
        }

        if ((result = remove_ipv4_redirect(router->tapDeviceName)) != 0) {
            // log("failed to remove IPv4 redirect: " + e.Message)
        }
        if ((result = start_routing_ipv6()) != 0) {
            // log("failed to unblock IPv6: " + e.Message)
        }

        if (router->remoteProxyIp && lstrlenW(router->remoteProxyIp)) {
            if ((result = delete_proxy_route(router->remoteProxyIp)) != 0) {
                // log("failed to remove route to the proxy server: {e.Message}")
            }
            //memset(router->remoteProxyIp, 0, sizeof(router->remoteProxyIp));
        } else {
            // log("cannot remove route to proxy server, have not previously set")
        }

        remove_reserved_subnet_bypass();
        //memset(router->gatewayIp, 0, sizeof(router->gatewayIp));
    } while (0);

    end_smart_dns_block();

    return result;
}

void send_connection_status_change(int status)
{
    struct service_response resp = { { 0 }, e_c_success };
    strcpy(resp.action, s_statusChanged);
    resp.connectionStatus = status;
}

void set_gateway_properties(const wchar_t* p)
{
}

int delete_proxy_route(const wchar_t* proxyIp)
{
    static const wchar_t* fmt = L"delete %s";
    return run_command_wrapper(CMD_ROUTE, fmt, proxyIp);
}

int add_ipv4_redirect(const struct router_info* router)
{
    const wchar_t* add_fmt = L"interface ipv4 add route %s nexthop=%s interface=%s metric=0 store=active";
    const wchar_t* set_fmt = L"interface ipv4 set route %s nexthop=%s interface=%s metric=0 store=active";
    int result = 0;
    int i = 0;
    if (router == NULL) {
        return -1;
    }
    for (i = 0; i < ARRAYSIZE(IPV4_SUBNETS); ++i) {
        const wchar_t* subnet = IPV4_SUBNETS[i];
        result = run_command_wrapper(CMD_NETSH, add_fmt, subnet, router->tapGatewayIp, router->tapDeviceName);
        if (result != 0) {
            result = run_command_wrapper(CMD_NETSH, set_fmt, subnet, router->tapGatewayIp, router->tapDeviceName);
            if (result != 0) {
                // "could not change default gateway: {0}"
                break;
            }
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
    const wchar_t* add_fmt = L"interface ipv6 add route %s interface=%d metric=0 store=active";
    const wchar_t* set_fmt = L"interface ipv6 set route %s interface=%d metric=0 store=active";
    int result = 0;
    int i = 0;
    int index = IPv6LoopbackInterfaceIndex();
    for (i = 0; i < ARRAYSIZE(IPV6_SUBNETS); ++i) {
        result = run_command_wrapper(CMD_NETSH, add_fmt, IPV6_SUBNETS[i], index);
        if (result != 0) {
            result = run_command_wrapper(CMD_NETSH, set_fmt, IPV6_SUBNETS[i], index);
            if (result != 0) {
                // "could not disable IPv6: {0}"
                break;
            }
        }
    }
    return result;
}

int add_or_update_proxy_route(const struct router_info* router)
{
    // "netsh interface ipv4 set route" does *not* work for us here
    // because it can only be used to change a route's *metric*.
    const wchar_t* chg_fmt = L"change %s %s if %d";
    const wchar_t* add_fmt = L"interface ipv4 add route %s/32 nexthop=%s interface=\"%d\" metric=0 store=active";
    int result = 0;

    result = run_command_wrapper(CMD_ROUTE, chg_fmt, router->remoteProxyIp, router->gatewayIp, router->gatewayInterfaceIndex);
    if (result != 0) {
        result = run_command_wrapper(CMD_NETSH, add_fmt, router->remoteProxyIp, router->gatewayIp, router->gatewayInterfaceIndex);
    }
    return result;
}

// Routes reserved and private subnets through the default gateway so they bypass the VPN.
int add_or_update_reserved_subnet_bypass(const struct router_info* router)
{
    const wchar_t* chg_fmt = L"change %s %s if %d";
    const wchar_t* add_fmt = L"interface ipv4 add route %s nexthop=%s interface=\"%d\" metric=0 store=active";
    int result = 0;
    int index = 0;
    for (index = 0; index < ARRAYSIZE(IPV4_RESERVED_SUBNETS); ++index) {
        result = run_command_wrapper(CMD_ROUTE, chg_fmt, IPV4_RESERVED_SUBNETS[index], router->gatewayIp, router->gatewayInterfaceIndex);
        if (result != 0) {
            result = run_command_wrapper(CMD_NETSH, add_fmt, IPV4_RESERVED_SUBNETS[index], router->gatewayIp, router->gatewayInterfaceIndex);
            if (result != 0) {
                break;
            }
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

// Removes reserved subnet routes created to bypass the VPN.
void remove_reserved_subnet_bypass(void)
{
    const wchar_t* fmt = L"delete route %s";
    int i;
    for (i = 0; i < ARRAYSIZE(IPV4_RESERVED_SUBNETS); ++i) {
        run_command_wrapper(CMD_ROUTE, fmt, IPV4_RESERVED_SUBNETS[i]);
    }
}

void build_response(const char* action, int code, const char* msg, char* out_buf, size_t size)
{
    sprintf(out_buf, "{\"action\":\"%s\", \"statusCode\": %d, \"errorMessage\": \"%s\", \"connectionStatus\":0 }", action, code, msg);
}

static void interfaces_with_ipv4_gateways(PIP_ADAPTER_ADDRESSES pCurrAddresses, wchar_t *gatewayIp, size_t size)
{
    PIP_ADAPTER_UNICAST_ADDRESS pUnicast = NULL;
    PIP_ADAPTER_ANYCAST_ADDRESS pAnycast = NULL;
    PIP_ADAPTER_MULTICAST_ADDRESS pMulticast = NULL;
    IP_ADAPTER_DNS_SERVER_ADDRESS* pDnServer = NULL;
    PIP_ADAPTER_GATEWAY_ADDRESS_LH gateway = NULL;
    IP_ADAPTER_PREFIX* pPrefix = NULL;
    wchar_t s[INET6_ADDRSTRLEN] = { 0 };
    int i;

    gateway = pCurrAddresses->FirstGatewayAddress;
    while (gateway) {
        if (gateway->Address.lpSockaddr->sa_family == AF_INET) {
            draft_inet_ntop(gateway->Address.lpSockaddr, s, ARRAYSIZE(s));
            wcsncpy(gatewayIp, s, size);
            break;
        }
        gateway = gateway->Next;
    }

#if 0
    i = 0;
    pUnicast = pCurrAddresses->FirstUnicastAddress;
    while (pUnicast != NULL) {
        printf("\t\tUnicast Addresses %d: %wS\n",
            i, draft_inet_ntop(pUnicast->Address.lpSockaddr, s, ARRAYSIZE(s)));
        pUnicast = pUnicast->Next;
        ++i;
    }
#endif
    (void)i;
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
            lstrcpyW(info->gatewayInterfaceName, pCurrAddresses->FriendlyName);
#if !(STUPID_WAY_GET_GATEWAY)
            info->gatewayInterfaceIndex = (int)pCurrAddresses->IfIndex;
            interfaces_with_ipv4_gateways(pCurrAddresses, info->gatewayIp, ARRAYSIZE(info->gatewayIp));
#endif
            if (stop) {
                *stop = TRUE;
            }
            return;
        }
        gateway = gateway->Next;
    }
}

void retrieve_tap_gateway_ip(int* stop, PIP_ADAPTER_ADDRESSES pCurrAddresses, void* p)
{
    struct router_info* info = (struct router_info*)p;
    PIP_ADAPTER_GATEWAY_ADDRESS_LH gateway = NULL;
    if (lstrcmpW(pCurrAddresses->FriendlyName, info->tapDeviceName) != 0) {
        return;
    }
    info->tapInterfaceIndex = (int)pCurrAddresses->IfIndex;
    gateway = pCurrAddresses->FirstGatewayAddress;
    while (gateway) {
        if (gateway->Address.lpSockaddr->sa_family == AF_INET) {
            interfaces_with_ipv4_gateways(pCurrAddresses, info->tapGatewayIp, ARRAYSIZE(info->tapGatewayIp));
            if (stop) {
                *stop = TRUE;
            }
            return;
        }
        gateway = gateway->Next;
    }
}

void get_system_ipv4_gateway(int* stop, int total, const MIB_IPFORWARDROW* row, void* p)
{
    static const MIB_IPFORWARDROW dummy = { 0 };
    struct GET_IPFORWARDROW* tmp = (struct GET_IPFORWARDROW*)p;
    // Must be a gateway (see note above on how we can improve this).
    if (row->dwForwardDest != 0) {
        return;
    }

    // Must not be the TAP device.
    if (row->dwForwardIfIndex == tmp->tapInterfaceIndex) {
        return;
    }

    if ((memcmp(&tmp->best_row, &dummy, sizeof(dummy)) == 0) || row->dwForwardMetric1 < tmp->best_row.dwForwardMetric1) {
        tmp->best_row = *row;
    }
}
