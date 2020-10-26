//
// server_connectivity.h
//

#ifndef __server_connectivity_h__
#define __server_connectivity_h__

#include <stdio.h>

int server_connectivity(const char *host, int port);
int convert_host_name_to_ip_string(const char *host, char *ipString, size_t len);

#endif /* __server_connectivity_h__ */
