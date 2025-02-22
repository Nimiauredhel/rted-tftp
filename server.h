#ifndef SERVER_H
#define SERVER_H

#include "common.h"
#include "networking_common.h"
#include "tftp_common.h"

#define SERVER_STORAGE_PATH "storage/"
#define SERVER_MAX_CONNECTIONS 5

typedef struct ServerSlotData
{
    OperationData_t *op_data_ptr;
    TransferData_t *tx_data_ptr;
} ServerSlotData_t;

typedef struct ServerSlots
{
    int free_slots_count;
    pthread_mutex_t slots_mutex;
    bool slot_occupied_flags[SERVER_MAX_CONNECTIONS];
    pthread_t slot_thread_handles[SERVER_MAX_CONNECTIONS];
    ServerSlotData_t slot_data[SERVER_MAX_CONNECTIONS];
} ServerSlots_t;

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

typedef struct ServerData
{
    ServerListenerData_t listener;
    ServerSlots_t slots;
} ServerData_t;

typedef struct ServerTaskArgs
{
    int task_slot_idx;
    ServerSlots_t *slots;
} ServerTaskArgs_t;

void server_start(void);

#endif
