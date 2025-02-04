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

#define PORT_MIN 49152
#define PORT_MAX 65535

bool parse_address(char *addr_str, struct in_addr *addr_bin);

#endif
