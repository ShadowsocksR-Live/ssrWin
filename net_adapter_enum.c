#include <winsock2.h>
#include <WS2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>

#include "net_adapter_enum.h"

// Link with Iphlpapi.lib
#pragma comment(lib, "IPHLPAPI.lib")
#pragma comment(lib, "Ws2_32.lib")

#define WORKING_BUFFER_SIZE 15000
#define MAX_TRIES 3

char * wchar_string_to_utf8(const wchar_t *wstr, char *receiver, size_t size) {
    int ol = lstrlenW(wstr);
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, ol, NULL, 0, NULL, NULL);
    if ((int)size < size_needed) {
        return NULL;
    }
    WideCharToMultiByte(CP_UTF8, 0, wstr, ol, receiver, size_needed, NULL, NULL);
    return receiver;
}

wchar_t * utf8_to_wchar_string(const char *str, wchar_t *receiver, size_t size) {
    int ol = lstrlenA(str);
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str, ol, NULL, 0);
    if(ol < size_needed) {
        return NULL;
    }
    MultiByteToWideChar(CP_UTF8, 0, str, ol, receiver, size_needed);
    return receiver;
}

const wchar_t * draft_inet_ntop(struct sockaddr *sa, wchar_t *buf, size_t size) {
    void *pAddr = NULL;
    if (sa->sa_family == AF_INET) {
        pAddr = &(((struct sockaddr_in*)sa)->sin_addr);
    } else {
        pAddr = &(((struct sockaddr_in6*)sa)->sin6_addr);
    }
    return InetNtopW(sa->sa_family, pAddr, buf, size);
}

void enum_adapter_info(ULONG family, fn_iterate_adapter pfn, void *p) {
    DWORD dwSize = 0;
    DWORD dwRetVal = 0;

    PIP_ADAPTER_ADDRESSES pAddresses = NULL;
    ULONG outBufLen = 0;
    ULONG Iterations = 0;

    PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;

    // Set the flags to pass to GetAdaptersAddresses
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS;

    printf("Calling GetAdaptersAddresses function with family = ");
    if (family == AF_INET)
        printf("AF_INET\n");
    if (family == AF_INET6)
        printf("AF_INET6\n");
    if (family == AF_UNSPEC)
        printf("AF_UNSPEC\n\n");

    // Allocate a 15 KB buffer to start with.
    outBufLen = WORKING_BUFFER_SIZE;

    do {
        pAddresses = (IP_ADAPTER_ADDRESSES *) malloc(outBufLen);
        if (pAddresses == NULL) {
            printf("Memory allocation failed for IP_ADAPTER_ADDRESSES struct\n");
            return;
        }

        dwRetVal = GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);

        if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
            free(pAddresses);
            pAddresses = NULL;
        } else {
            break;
        }

        Iterations++;
    } while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (Iterations < MAX_TRIES));

    if (dwRetVal == NO_ERROR) {
        // If successful, output some information from the data we received
        pCurrAddresses = pAddresses;
        while (pCurrAddresses) {
            if (pfn(pCurrAddresses, p) == FALSE) {
                break;
            }
            pCurrAddresses = pCurrAddresses->Next;
        }
    } else {
        printf("Call to GetAdaptersAddresses failed with error: %d\n", dwRetVal);
        if (dwRetVal == ERROR_NO_DATA) {
            printf("\tNo addresses were found for the requested parameters\n");
        } else {
            LPVOID lpMsgBuf = NULL;
            if (FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
                NULL, dwRetVal, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (char *) & lpMsgBuf, 0, NULL)) {
                    printf("\tError: %s", (char *)lpMsgBuf);
                    LocalFree(lpMsgBuf);
            }
        }
    }

    if (pAddresses) {
        free(pAddresses);
    }
}
