#pragma once

#include <winsock2.h>
#include <WS2tcpip.h>
#include <iphlpapi.h>

char * wchar_string_to_utf8(const wchar_t *wstr, char *receiver, size_t size);
wchar_t * utf8_to_wchar_string(const char *str, wchar_t *receiver, size_t size);

const char * draft_inet_ntop(struct sockaddr *sa, char *buf, size_t buf_size);

typedef BOOL (* fn_iterate_adapter)(PIP_ADAPTER_ADDRESSES pCurrAddresses, void *p);
void enum_adapter_info(ULONG family, fn_iterate_adapter pfn, void *p);

