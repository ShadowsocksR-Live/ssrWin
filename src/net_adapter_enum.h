#pragma once

#include <WS2tcpip.h>
#include <iphlpapi.h>
#include <winsock2.h>

const wchar_t* draft_inet_ntop(struct sockaddr* sa, wchar_t* buf, size_t size);

int LoopbackInterfaceIndex(void);
int IPv6LoopbackInterfaceIndex(void);

typedef void (*fn_iterate_adapter)(int* stop, PIP_ADAPTER_ADDRESSES pCurrAddresses, void* p);
void enum_adapter_info(ULONG family, fn_iterate_adapter pfn, void* p);

typedef void (*fn_iterate_ip_forward_table)(int* stop, int total, const MIB_IPFORWARDROW* row, void* p);
void enum_ip_forward_table(fn_iterate_ip_forward_table pfn, void* p);
