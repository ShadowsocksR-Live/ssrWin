//
// server_connectivity.h
//

#ifndef __server_connectivity_h__
#define __server_connectivity_h__

#include <stdio.h>
#include <stdbool.h>

int server_connectivity(const char *host, int port);
int convert_host_name_to_ip_string(const char *host, char *ipString, size_t len);
bool tcp_port_is_occupied(const char* listen_addr, int port);

#endif /* __server_connectivity_h__ */
