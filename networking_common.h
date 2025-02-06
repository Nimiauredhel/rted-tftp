#ifndef NETWORKING_COMMON_H
#define NETWORKING_COMMON_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <stdbool.h>

#define PORT_BUFF_LENGTH 8
#define ADDRESS_BUFF_LENGTH 40
#define NAME_BUFF_LENGTH 32

#define SERVER_DATA_PORT_MIN 49152
#define SERVER_DATA_PORT_MAX 49999
#define CLIENT_DATA_PORT_MIN 50000
#define CLIENT_DATA_PORT_MAX 59999

bool parse_address(char *addr_str, struct in_addr *addr_bin);

#endif
