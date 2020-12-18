//
//  server_connectivity.c
//

#include "server_connectivity.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

#if defined(WIN32) || defined(_WIN32)
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif

#define MAXDATASIZE 100

int server_connectivity(const char *host, int port) {
    int cliSock = -1;

    int result = -1;
    struct addrinfo *addr = NULL;

    do {
        struct addrinfo hints = { 0 };
        struct hostent *he = NULL;
        struct sockaddr_in dest_addr = { 0 };
        int c;

        if (host==NULL || port==0) {
            break;
        }
        hints.ai_flags = AI_NUMERICHOST;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        if (getaddrinfo(host, NULL, &hints, &addr) != 0) {
            if ((he = gethostbyname(host)) == NULL) {
                break;
            }
            
            dest_addr.sin_family = AF_INET;
            dest_addr.sin_port = htons(port);
            dest_addr.sin_addr = *((struct in_addr *)he->h_addr);
        }

        if (addr) {
        if ((cliSock = (int)socket(addr->ai_family, addr->ai_socktype, 0)) == -1) {
            printf("Socket Error: %d\n", errno);
            break;
        } else {
            printf("Client Socket %d created\n", cliSock);
        }

        if (addr->ai_family == AF_INET) {
            ((struct sockaddr_in *)addr->ai_addr)->sin_port = htons(port);
        } else if (addr->ai_family == AF_INET6) {
            ((struct sockaddr_in6 *)addr->ai_addr)->sin6_port = htons(port);
        }

        c = connect(cliSock, addr->ai_addr, addr->ai_addrlen);
        if (c != 0) {
            printf("Connect Error: %d\n", errno);
            break;
        } else {
            printf("Client Connection created\n");
        }
        } else {
            if ((cliSock = (int)socket(AF_INET, SOCK_STREAM, 0)) == -1) {
                break;
            }
            c = connect(cliSock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr));
            if (c != 0) {
                break;
            }
        }

        /*
        {
            int numbytes;
            char buf[MAXDATASIZE], msg[MAXDATASIZE];

            sprintf(msg, "4 8 15 16 23 42");
            send(cliSock, msg, MAXDATASIZE, 0);
            printf("Client sent %s to %s\n", msg, inet_ntoa(dest_addr.sin_addr));

            numbytes = recv(cliSock, buf, MAXDATASIZE, 0);
            buf[numbytes] = '\0';
            printf("Received Message: %s\n", buf);
        }
        // */

        result = 0;
    } while (0);

    if (addr) {
        freeaddrinfo(addr);
    }

    if (cliSock != -1) {
#if defined(WIN32) || defined(_WIN32)
        closesocket(cliSock);
#else
        close(cliSock);
#endif
        printf("Client Sockets closed\n");
    }

    return result;
}

int convert_host_name_to_ip_string (const char *host, char *ipString, size_t len) {
    int result = -1;
    do {
        struct hostent *he;
        char *ip;
        if (host==NULL || ipString==NULL || len<16) {
            break;
        }

        if ((he = gethostbyname(host)) == NULL) {
            printf("Couldn't get hostname\n");
            break;
        }

        ip = inet_ntoa( *((struct in_addr *)he->h_addr) );
        if (ip == NULL) {
            break;
        }
        strncpy(ipString, ip, len);
        result = 0;
    } while (0);
    return result;
}

bool tcp_port_is_occupied(const char* listen_addr, int port) {
    bool result = false;
    char tmp[256] = { 0 };
    struct sockaddr_in *service = (struct sockaddr_in *)tmp;
    int sd;

#if defined(WIN32) || defined(_WIN32)
    static bool flag = false;
    if (!flag) {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
        flag = true;
    }
#endif
    sd = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sd == -1) {
        return true;
    }
    service->sin_family = AF_INET;
    service->sin_addr.s_addr = inet_addr(listen_addr);
    service->sin_port = htons((unsigned short)port);
    if (bind(sd, (struct sockaddr*)service, sizeof(tmp)) != 0) {
        result = true;
    }
#if defined(WIN32) || defined(_WIN32)
    closesocket(sd);
#else
    close(sd);
#endif
    return result;
}
