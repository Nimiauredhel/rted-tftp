#ifndef SERVER_H
#define SERVER_H

#include "common.h"
#include "networking_common.h"
#include "tftp_common.h"

#define SERVER_STORAGE_PATH "storage/"
#define SERVER_MAX_CONNECTIONS 5

/**
 * Holds pointers to operation-relevant structs
 * used by a single operation thread at a time.
 */
typedef struct ServerSlotData
{
    OperationData_t *op_data_ptr;
    TransferData_t *tx_data_ptr;
} ServerSlotData_t;

/**
 * Shared between the listener thread and operation threads, via mutex.
 * Used to track concurrent operations.
 */
typedef struct ServerSlots
{
    int free_slots_count;
    pthread_mutex_t slots_mutex;
    bool slot_occupied_flags[SERVER_MAX_CONNECTIONS];
    pthread_t slot_thread_handles[SERVER_MAX_CONNECTIONS];
    ServerSlotData_t slot_data[SERVER_MAX_CONNECTIONS];
} ServerSlots_t;

/**
 * Used by the listener thread.
 * Holds incoming request packet data and the requests socket handle.
 */
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

/**
 * Struct encapsulating the two long-living server-side data structures.
 */
typedef struct ServerData
{
    ServerListenerData_t listener;
    ServerSlots_t slots;
} ServerData_t;

/**
 * Struct encapsulating all data required by a server-side operation thread ("task")
 * to handle an entire client-requested operation and clean after itself.
 */
typedef struct ServerTaskArgs
{
    int task_slot_idx;
    ServerSlots_t *slots;
} ServerTaskArgs_t;

/**
 * Entry point for the TFTP server.
 * Initializes the server state and launches the listener loop function.
 * When the listener loop function returns, it cleans up the server data and returns to main.
 */
void server_start(void);

#endif
