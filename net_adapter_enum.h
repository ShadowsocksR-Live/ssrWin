#pragma once

#include <winsock2.h>
#include <WS2tcpip.h>
#include <iphlpapi.h>

char * wchar_string_to_utf8(const wchar_t *wstr, char *receiver, size_t size);
wchar_t * utf8_to_wchar_string(const char *str, wchar_t *receiver, size_t size);

const wchar_t * draft_inet_ntop(struct sockaddr *sa, wchar_t *buf, size_t size);

int LoopbackInterfaceIndex(void);
int IPv6LoopbackInterfaceIndex(void);

typedef void (* fn_iterate_adapter)(int *stop, PIP_ADAPTER_ADDRESSES pCurrAddresses, void *p);
void enum_adapter_info(ULONG family, fn_iterate_adapter pfn, void *p);

