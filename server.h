#ifndef SERVER_H
#define SERVER_H

#include "common.h"
#include "networking_common.h"
#include "tftp_common.h"

typedef struct ServerSideData
{
    int requests_socket;
    struct sockaddr_in requests_address;
} ServerSideData_t;

void server_start(void);

#endif
