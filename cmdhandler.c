#include "net_adapter_enum.h"
#include <windows.h>
#include <tchar.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include "cmdhandler.h"
#include "json.h"

static const char *IPV4_SUBNETS[] = { "0.0.0.0/1", "128.0.0.0/1" };
static const char *IPV6_SUBNETS[] = { "fc00::/7", "2000::/4", "3000::/4" };
static const char *TAP_DEVICE_NAME = "outline-tap0";
static const char *CMD_NETSH = "netsh";

struct router_info {
    char proxyIp[MAX_PATH];
    char routerIp[MAX_PATH];
    char gatewayIp[MAX_PATH];
    char gatewayInterfaceName[MAX_PATH];
};

struct router_info g_router_inst = { 0 };


BOOL pick_live_adapter(PIP_ADAPTER_ADDRESSES pCurrAddresses, void *p);

void configure_routing(const char *routerIp, const char *proxyIp, int isAutoConnect);
void reset_routing(const char *proxyIp, const char *proxyInterfaceName);
int run_command_wrapper(const char *cmd, const char *argv_fmt, ...);
int delete_proxy_route(const char *proxyIp, const char *proxyInterfaceName);
void set_gateway_properties(const char *p);
void remove_ipv4_redirect(void);
void start_routing_ipv6(void);
int run_command(const char *cmd, const char *args);

static void process_json_generic(struct json_object* value, struct service_request *request) {
    int length, x;
    assert(value);
    assert(value->type == json_type_object);
    length = value->u.object.length;
    for (x = 0; x < length; x++) {
        struct json_object_entry * entry = value->u.object.entries + x;

        if (strcmp(entry->name, s_action) == 0) {
            assert(entry->object->type == json_type_string);
            strcpy(request->action, entry->object->u.string.ptr);
        }

        if (strcmp(entry->name, s_parameters) == 0) {
            size_t len2, y;
            struct json_object *sub2 = entry->object;
            assert(sub2->type == json_type_object);
            len2 = sub2->u.object.length;
            for(y=0; y<len2; ++y) {
                struct json_object_entry * entry2 = sub2->u.object.entries + y;
                if (strcmp(entry2->name, s_proxyIp) == 0) {
                    assert(entry2->object->type == json_type_string);
                    strcpy(request->proxyIp, entry2->object->u.string.ptr);
                }
                if (strcmp(entry2->name, s_routerIp) == 0) {
                    assert(entry2->object->type == json_type_string);
                    strcpy(request->routerIp, entry2->object->u.string.ptr);
                }
                if (strcmp(entry2->name, s_isAutoConnnect) == 0) {
                    assert(entry2->object->type == json_type_boolean);
                    request->isAutoConnnect = entry2->object->u.boolean;
                }
            }
        }
    }
}

void parse_request(const char *json, struct service_request *request) {
    struct json_object* value;
    value = json_parse((json_char*)json, strlen(json));
    if (value) {
        process_json_generic(value, request);
        json_object_free(value);
    }
}

void handle_request(struct service_request *request) {
    if (strcmp(request->action, s_configureRouting) == 0) {
        configure_routing(request->routerIp, request->proxyIp, request->isAutoConnnect);
    }
    if (strcmp(request->action, s_resetRouting) == 0) {
        reset_routing(NULL, NULL);
    }
}

void configure_routing(const char *routerIp, const char *proxyIp, int isAutoConnect) {
    if (routerIp==NULL || strlen(routerIp)==0 || proxyIp==NULL || strlen(proxyIp)==0) {
        return;
    }
    run_command_wrapper(CMD_NETSH, "interface ip set interface %s metric=0", TAP_DEVICE_NAME);

    enum_adapter_info(AF_UNSPEC, pick_live_adapter, &g_router_inst);

    strcpy(g_router_inst.proxyIp, proxyIp);
    strcpy(g_router_inst.routerIp, routerIp);
}

void reset_routing(const char *proxyIp, const char *proxyInterfaceName) {
    if (proxyIp && strlen(proxyIp)) {
        if (delete_proxy_route(proxyIp, proxyInterfaceName) == 0) {
            // log("failed to remove route to the proxy server: {e.Message}")
        }
    } else {
        // log("cannot remove route to proxy server, have not previously set")
    }
    set_gateway_properties(NULL);

    remove_ipv4_redirect();
    start_routing_ipv6();
}

int run_command_wrapper(const char *cmd, const char *argv_fmt, ...) {
    char buffer[MAX_PATH * 2] = { 0 };
    va_list arg;
    va_start(arg, argv_fmt);
    vsprintf(buffer, argv_fmt, arg);
    va_end(arg);
    return run_command(cmd, buffer);
}

int delete_proxy_route(const char *proxyIp, const char *proxyInterfaceName) {
    static const char *fmt = "interface ipv4 delete route %s/32 interface=\"%s\"";
    return run_command_wrapper(CMD_NETSH, fmt, proxyIp, proxyInterfaceName);
}

void set_gateway_properties(const char *p) {
}

void remove_ipv4_redirect(void) {
}

void start_routing_ipv6(void) {
}

void build_response(int code, const char *msg, char *out_buf, size_t size) {
    sprintf(out_buf, "{ statusCode: %d, errorMessage: \"%s\" }", code, msg);
}

int run_command(const char *cmd, const char *args) {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD exit_code = 0;
    size_t len = 0;
    char *buffer = NULL;

    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    ZeroMemory( &pi, sizeof(pi) );

    len = strlen(cmd) + strlen(args) + 10;
    buffer = (char *) calloc(len, sizeof(char));
    if (cmd[0] == '"') {
        sprintf(buffer, "%s %s", cmd, args);
    } else {
        sprintf(buffer, "\"%s\" %s", cmd, args);
    }

    // Start the child process. 
    if( !CreateProcessA( NULL,   // No module name (use command line)
        buffer,        // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        0,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi )           // Pointer to PROCESS_INFORMATION structure
        ) 
    {
        printf( "CreateProcess failed (%d).\n", GetLastError() );
        return -1;
    }

    // Wait until child process exits.
    WaitForSingleObject( pi.hProcess, INFINITE );

    GetExitCodeProcess(pi.hProcess, &exit_code);

    // Close process and thread handles. 
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );

    free(buffer);

    return (int) exit_code;
}


void interfacesWithIpv4Gateways(PIP_ADAPTER_ADDRESSES pAddresses, void *p);

BOOL pick_live_adapter(PIP_ADAPTER_ADDRESSES pCurrAddresses, void *p) {
    unsigned int i = 0;
    PIP_ADAPTER_GATEWAY_ADDRESS_LH gateway = NULL;
    char friendly_name[MAX_PATH] = { 0 };

    if (pCurrAddresses->IfType==IF_TYPE_SOFTWARE_LOOPBACK) {
        return TRUE;
    }
    wchar_string_to_utf8(pCurrAddresses->FriendlyName, friendly_name, sizeof(friendly_name));
    if (strcmp(friendly_name, TAP_DEVICE_NAME) == 0) {
        return TRUE;
    }
    if (pCurrAddresses->OperStatus != IfOperStatusUp) {
        return TRUE;
    }

    i = 0;
    gateway = pCurrAddresses->FirstGatewayAddress;
    while (gateway) {
        ++i;
        if (gateway->Address.lpSockaddr->sa_family == AF_INET) {
            interfacesWithIpv4Gateways(pCurrAddresses, p);
        }
        gateway = gateway->Next;
    }

    return TRUE;
}

void interfacesWithIpv4Gateways(PIP_ADAPTER_ADDRESSES pCurrAddresses, void *p) {
    PIP_ADAPTER_UNICAST_ADDRESS pUnicast = NULL;
    PIP_ADAPTER_ANYCAST_ADDRESS pAnycast = NULL;
    PIP_ADAPTER_MULTICAST_ADDRESS pMulticast = NULL;
    IP_ADAPTER_DNS_SERVER_ADDRESS *pDnServer = NULL;
    PIP_ADAPTER_GATEWAY_ADDRESS_LH gateway = NULL;
    IP_ADAPTER_PREFIX *pPrefix = NULL;
    char s[INET6_ADDRSTRLEN] = { 0 };
    int i;

    struct router_info *info = (struct router_info *)p;

    lstrcpyW(info->gatewayInterfaceName, pCurrAddresses->FriendlyName);

    printf("\tIfIndex (IPv4 interface): %u\n", pCurrAddresses->IfIndex);
    printf("\tIpv6IfIndex (IPv6 interface): %u\n", pCurrAddresses->IfIndex);
    printf("\tAdapter name: %s\n", pCurrAddresses->AdapterName);
    printf("\tDescription: %wS\n", pCurrAddresses->Description);
    printf("\tFriendly name: %wS\n", pCurrAddresses->FriendlyName);
    printf("\tFlags: %ld\n", pCurrAddresses->Flags);

    //draft_inet_ntop(gateWay->Address.lpSockaddr, s, sizeof(s));

    i = 0;
    pUnicast = pCurrAddresses->FirstUnicastAddress;
    while (pUnicast != NULL) {
        printf("\t\tUnicast Addresses %d: %s\n", i, 
            draft_inet_ntop(pUnicast->Address.lpSockaddr, s, sizeof(s)));
        pUnicast = pUnicast->Next;
        ++i;
    }
}
