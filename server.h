#ifndef SERVER_H
#define SERVER_H

#include "common.h"
#include "networking_common.h"
#include "tftp_common.h"

typedef struct ServerListenerData
{
    int requests_socket;
    struct sockaddr_in requests_address;
    struct sockaddr_in client_address;
    socklen_t client_address_length;
    TFTPOpcode_t incoming_opcode;
    ssize_t bytes_received;
    size_t buffer_size;
    Packet_t *request_buffer;
} ServerListenerData_t;

void server_start(void);

#endif
