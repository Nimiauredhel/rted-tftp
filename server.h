#ifndef SERVER_H
#define SERVER_H

#include "common.h"
#include "networking_common.h"
#include "tftp_common.h"

typedef struct ServerData
{
    int requests_socket;
    struct sockaddr_in requests_address;
    struct sockaddr_in client_address;
    socklen_t client_address_length;
    ssize_t bytes_received;
    size_t buffer_size;
} ServerData_t;



void server_start(void);

#endif
