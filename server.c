#include "server.h"

static bool server_init_storage_location(void)
{
    if (mkdir(SERVER_STORAGE_PATH, 0777) == 0)
    {
        printf("Initialized storage directory at path '%s'.\n", SERVER_STORAGE_PATH);
    }
    else
    {
        if (errno == EEXIST)
        {
            printf("Existing storage directory detected at path '%s'.\n", SERVER_STORAGE_PATH);
        }
        else
        {
            perror("Error creating/finding storage directory");
            return false;
        }
    }

    return true;
}

static bool server_delete_file(OperationData_t *op_data)
{
    // acknowledge request
    tftp_send_ack(0, op_data->data_socket, &op_data->peer_address, op_data->peer_address_length);

    FILE *file = fopen(op_data->path, "r");

    if (file == NULL)
    {
        printf("Requested file not found: %s\n", op_data->path);
        tftp_send_error(TFTP_ERROR_FILE_NOT_FOUND, "file not found: ", &op_data->path[strlen(SERVER_STORAGE_PATH)], op_data->data_socket, &op_data->peer_address, op_data->peer_address_length); 
        return false;
    }

    printf("File exists and will be deleted: %s\n", op_data->path);
    fclose(file);

    if (0 > remove(op_data->path))
    {
        perror("Failed to delete file");
        tftp_send_error(TFTP_ERROR_UNDEFINED, "failed to delete, server error: ", strerror(errno), op_data->data_socket, &op_data->peer_address, op_data->peer_address_length); 
        return false;
    }

    // confirm deletion
    tftp_send_ack(1, op_data->data_socket, &op_data->peer_address, op_data->peer_address_length);
    printf("File deleted successfully: %s\n", op_data->path);
    return true;
}

static int server_acquire_connection_slot(ServerSlots_t *data)
{
    int retval = -1;
    pthread_mutex_lock(&data->slots_mutex);

    if (data->free_slots_count > 0)
    {
        for (int i = 0; i < SERVER_MAX_CONNECTIONS; i++)
        {
            if (!data->slot_occupied_flags[i])
            {
                data->slot_occupied_flags[i] = true;
                data->free_slots_count--;
                retval = i;
                break;
            }
        }
    }

    pthread_mutex_unlock(&data->slots_mutex);
    return retval;
}

static void server_release_connection_slot(ServerSlots_t *data, int slot_index)
{
    printf("[Slot #%d] Releasing connection slot...\n", slot_index);
    pthread_mutex_lock(&data->slots_mutex);
    data->slot_occupied_flags[slot_index] = false;
    data->free_slots_count++;
    pthread_mutex_unlock(&data->slots_mutex);
}

static void server_task_release(ServerTaskArgs_t *task_args)
{
    printf("[Slot #%d] Terminating thread...\n", task_args->task_slot_idx);
    pthread_detach(pthread_self());
    server_release_connection_slot(task_args->slots, task_args->task_slot_idx);
    free(task_args);
}

static void* server_task_start(void *args)
{
    ServerTaskArgs_t *task_args = (ServerTaskArgs_t *)args;
    OperationData_t *op_data = task_args->slots->slot_data[task_args->task_slot_idx].op_data_ptr;
    TransferData_t *tx_data = task_args->slots->slot_data[task_args->task_slot_idx].tx_data_ptr;

    if (should_terminate)
    {
        printf("[Slot #%d] User requested termination - aborting.\n", task_args->task_slot_idx);
        server_task_release(task_args);
        pthread_exit(NULL);
    }

    printf("[Slot #%d] Operation task started.\n", task_args->task_slot_idx);

    switch(op_data->operation_id)
    {
        case TFTP_OPERATION_RECEIVE:
            tx_data = malloc(sizeof(TransferData_t));
            if (tftp_fill_transfer_data(op_data, tx_data, true)
                // acknowledge request
                && tftp_send_ack(0, op_data->data_socket, &op_data->peer_address, op_data->peer_address_length))
            {
                // receive file
                if (false == tftp_receive_file(op_data, tx_data))
                {
                    // if failed during transfer, nullify file handle and delete incomplete file
                    printf("[Slot #%d] Deleting partial download.\n", task_args->task_slot_idx);
                    fclose(tx_data->file);
                    tx_data->file = NULL;
                    remove(op_data->path);
                }
            }
            tftp_free_transfer_data(tx_data);
            break;
        case TFTP_OPERATION_SEND:
            tx_data = malloc(sizeof(TransferData_t));
            if(tftp_fill_transfer_data(op_data, tx_data, false))
            {
                // send file
                tftp_transmit_file(op_data, tx_data);
            }
            tftp_free_transfer_data(tx_data);
            break;
        case TFTP_OPERATION_HANDLE_DELETE:
            server_delete_file(op_data);
            break;
        case TFTP_OPERATION_REQUEST_DELETE:
        case TFTP_OPERATION_UNDEFINED:
            break;
    }

    tftp_free_operation_data(op_data);
    server_task_release(task_args);
    pthread_exit(NULL);
}

static OperationData_t* server_parse_request_data(ServerListenerData_t *data)
{
    char file_path[TFTP_FILENAME_MAX * 2] = SERVER_STORAGE_PATH;
    char *mode_string = NULL;
    char *blksize_string = NULL;
    char *blksize_octets_string = NULL;
    OperationId_t op_id = TFTP_OPERATION_UNDEFINED;

    // extract request strings
    strncat(file_path, data->request_buffer->request.contents, TFTP_FILENAME_MAX);

    switch(ntohs(data->request_buffer->opcode))
    {
        case TFTP_RRQ:
            op_id = TFTP_OPERATION_SEND;
            break;
        case TFTP_WRQ:
            op_id = TFTP_OPERATION_RECEIVE;
            break;
        case TFTP_DRQ:
            op_id = TFTP_OPERATION_HANDLE_DELETE;
            break;
        default:
            return NULL;
    }

    if (op_id != TFTP_OPERATION_HANDLE_DELETE)
    {
        int contents_index = strlen(data->request_buffer->request.contents) + 1;
        mode_string = data->request_buffer->request.contents + contents_index;

        if ((contents_index + 4) < data->bytes_received)
        {
            contents_index += strlen(mode_string) + 1;
            blksize_string = data->request_buffer->request.contents + contents_index;

            if ((strcasecmp(blksize_string, TFTP_BLKSIZE_STRING) == 0)
                && (contents_index + 4 < data->bytes_received))
            {
                contents_index += strlen(blksize_string) + 1;
                blksize_octets_string = data->request_buffer->request.contents + contents_index;
            }
        }
    }

    return tftp_init_operation_data(op_id, data->client_address, file_path, mode_string, blksize_octets_string);
}

static bool server_init_listener_data(ServerListenerData_t *data)
{
    static const int reuse_flag = 1;

    data->requests_socket = socket(AF_INET, SOCK_DGRAM, 0);

    if (data->requests_socket < 0)
    {
        perror("Failed to create requests socket");
        return false;
    }

    if(0 > setsockopt(data->requests_socket, SOL_SOCKET, SO_REUSEADDR,  &reuse_flag, sizeof(reuse_flag)))
    {
        perror("Failed to set socket 'reuse address' option");
        return false;
    }

    if(0 > setsockopt(data->requests_socket, SOL_SOCKET, SO_REUSEPORT,  &reuse_flag, sizeof(reuse_flag)))
    {
        perror("Failed to set socket 'reuse port' option");
        return false;
    }

    data->requests_address.sin_family = AF_INET;
    data->requests_address.sin_addr.s_addr = INADDR_ANY;
    data->requests_address.sin_port = htons(69);

    data->client_address.sin_family = AF_INET;
    data->client_address_length = sizeof(data->client_address);

    int bind_result = bind(data->requests_socket, (struct sockaddr *)&(data->requests_address), sizeof(data->requests_address));

    if (bind_result < 0)
    {
        perror("Could not bind requests socket");
        return false;
    }

    data->buffer_size = sizeof(Packet_t) + TFTP_FILENAME_MAX * 2;
    data->request_buffer = malloc(data->buffer_size);

    if (data->request_buffer == NULL)
    {
        perror("Failed to allocate buffer for incoming requests");
        return false;
    }

    explicit_bzero(data->request_buffer, data->buffer_size);
    return true;
}

static void server_deinit_listener_data(ServerListenerData_t *data)
{
    close(data->requests_socket);
    explicit_bzero(data->request_buffer, data->buffer_size);
    free(data->request_buffer);
    explicit_bzero(data, sizeof(ServerListenerData_t));
}

static void server_init_slots_data(ServerSlots_t *data)
{
    // initialize client slots data
    data->free_slots_count = SERVER_MAX_CONNECTIONS;
    pthread_mutex_init(&data->slots_mutex, NULL);

    for (int i = 0; i < SERVER_MAX_CONNECTIONS; i++)
    {
        data->slot_thread_handles[i] = 0;
        data->slot_occupied_flags[i] = false;
        data->slot_data[i].op_data_ptr = NULL;
        data->slot_data[i].tx_data_ptr = NULL;
    }
}

static void server_deinit_slots_data(ServerSlots_t *data)
{
    pthread_mutex_destroy(&data->slots_mutex);
    explicit_bzero(data, sizeof(ServerSlots_t));
}

static void server_try_create_operation_thread(ServerListenerData_t *listener, ServerSlots_t *slots)
{
    int acquired_slot_idx = server_acquire_connection_slot(slots);

    if (acquired_slot_idx == -1)
    {
        printf("Rejecting request - exceeded max connection count.\n");
        tftp_send_error(TFTP_ERROR_OUT_OF_SPACE, "Server exceeded maximal connection count. Try again later!",
            NULL, listener->requests_socket, &(listener->client_address), listener->client_address_length);
    }
    else
    {
        printf("[Slot #%d] Accepted request and assigned connection slot.\n", acquired_slot_idx);

        OperationData_t *new_op_data_ptr = server_parse_request_data(listener);

        if (new_op_data_ptr == NULL)
        {
            printf("[Slot #%d] Operation data null - aborting.\n", acquired_slot_idx);
            server_release_connection_slot(slots, acquired_slot_idx);
        }
        else
        {
            printf("[Slot #%d] Request parsed successfully, starting operation thread.\n", acquired_slot_idx);
            slots->slot_data[acquired_slot_idx].op_data_ptr = new_op_data_ptr;

            ServerTaskArgs_t *task_args = malloc(sizeof(ServerTaskArgs_t));

            task_args->task_slot_idx = acquired_slot_idx;
            task_args->slots = slots;

            pthread_create(&(slots->slot_thread_handles[acquired_slot_idx]), NULL, server_task_start, task_args);
        }
    }
}

static void server_listener_loop(ServerListenerData_t *listener, ServerSlots_t *slots)
{
    static const char *received_packet_message_format = "Received %s packet in requests socket.\n";

    while(!should_terminate)
    {
        printf("Awaiting requests.\n");

        listener->bytes_received = recvfrom(listener->requests_socket, listener->request_buffer, listener->buffer_size, 0, (struct sockaddr*)&(listener->client_address), &(listener->client_address_length));

        if (should_terminate) break;

        if(listener->bytes_received < 0)
        {
            // TODO: extract error handling function plz
            perror("Failed to receive bytes");
            continue;
        }
        else if (listener->bytes_received == 0)
        {
            printf("Received zero bytes.\n");
            continue;
        }

        printf("Received %lu bytes. Contents:\n", listener->bytes_received);
        fwrite(listener->request_buffer->request.contents, sizeof(char), listener->bytes_received, stdout);
        printf("\n");

        listener->incoming_opcode = ntohs(listener->request_buffer->opcode);

        switch (listener->incoming_opcode)
        {
            // *** Invalid (non-request) opcodes: send an error and move on
            case TFTP_DATA:
            case TFTP_ACK:
            case TFTP_ERROR:
            default:
                fprintf(stderr, received_packet_message_format, tftp_common.opcode_strings[listener->incoming_opcode]);
                tftp_send_error(TFTP_ERROR_ILLEGAL_OPERATION, "received packet in requests socket with opcode ", tftp_common.opcode_strings[listener->incoming_opcode], listener->requests_socket, &listener->client_address, listener->client_address_length); 
                break;
            // *** Standard request opcodes: parse and begin operation
            case TFTP_RRQ:
            case TFTP_WRQ:
            case TFTP_DRQ:
                printf(received_packet_message_format, tftp_common.opcode_strings[listener->incoming_opcode]);
                server_try_create_operation_thread(listener, slots);
                break;
        }

        // buffer was used in this iteration - clear it to zero
        explicit_bzero(listener->request_buffer, listener->buffer_size);
    }

    printf("\nServer listener loop terminated.\n");
}

static ServerData_t* server_init_data(void)
{
    ServerData_t *data = malloc(sizeof(ServerData_t));;
    explicit_bzero(data, sizeof(ServerData_t));

    if (!server_init_listener_data(&data->listener))
    {
        printf("Failed to initialize server.\nDeallocating...\n");
        free(data);
        return NULL;
    }

    server_init_slots_data(&data->slots);

    return data;
}

void server_start(void)
{
    ServerData_t *data = server_init_data();

    if (data == NULL)
    {
        printf("Server initialization failed! Terminating.\n");
        return;
    }

    if (server_init_storage_location())
    {
        server_listener_loop(&data->listener, &data->slots);

        // Listener terminated - checking and waiting for any possibly lingering threads
        printf("Awaiting termination of lingering threads...\n");

        for (int i = 0; i < SERVER_MAX_CONNECTIONS; i++)
        {
            pthread_join(data->slots.slot_thread_handles[i], NULL);
        }
    }

    // Explicitly blanking and releasing all server data before returning to main.
    // Probably insignificant but seems like a good practice.
    printf("Deallocating server data.\n");
    server_deinit_listener_data(&data->listener);
    server_deinit_slots_data(&data->slots);
    explicit_bzero(data, sizeof(ServerData_t));
    free(data);

    printf("Server terminating.\n");
}
