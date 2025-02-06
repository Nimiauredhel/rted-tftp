#include "server.h"

static void server_init_storage(void)
{
    if (mkdir(STORAGE_PATH, 0777) == 0)
    {
        printf("Initialized storage directory at path '%s'.\n", STORAGE_PATH);
    }
    else
    {
        if (errno == EEXIST)
        {
            printf("Existing storage directory detected at path '%s'.\n", STORAGE_PATH);
        }
        else
        {
            perror("Error creating/finding storage directory");
            exit(EXIT_FAILURE);
        }
    }
}

static bool server_delete_file(OperationData_t *op_data)
{
    // acknowledge request
    tftp_send_ack(0, op_data->data_socket, &op_data->peer_address, op_data->peer_address_length);

    FILE *file = fopen(op_data->path, "r");

    if (file == NULL)
    {
        printf("Requested file not found: %s\n", op_data->path);
        tftp_send_error(TFTP_ERROR_FILE_NOT_FOUND, "file not found: ", &op_data->path[strlen(STORAGE_PATH)], op_data->data_socket, &op_data->peer_address, op_data->peer_address_length); 
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

static void server_start_operation(OperationData_t *op_data)
{
    TransferData_t *tx_data;

    switch(op_data->operation_id)
    {
        case TFTP_OPERATION_RECEIVE:
            tx_data = malloc(sizeof(TransferData_t));
            if (tftp_fill_transfer_data(op_data, tx_data, true)
                // acknowledge request
                && tftp_send_ack(0, op_data->data_socket, &op_data->peer_address, op_data->peer_address_length))
            {
                // receive file
                tftp_receive_file(op_data, tx_data);
            }
            tftp_free_transfer_data(tx_data);
            break;
        case TFTP_OPERATION_SEND:
            tx_data = malloc(sizeof(TransferData_t));
            if( tftp_fill_transfer_data(op_data, tx_data, false))
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
}

static OperationData_t* server_parse_request_data(Packet_t *request, ssize_t request_length, struct sockaddr_in peer_address)
{
    char file_path[TFTP_FILENAME_MAX * 2] = STORAGE_PATH;
    char *mode_string = NULL;
    char *blksize_string = NULL;
    char *blksize_octets_string = NULL;
    OperationId_t op_id = TFTP_OPERATION_UNDEFINED;

    // extract request strings
    strncat(file_path, request->request.contents, TFTP_FILENAME_MAX);

    switch(request->opcode)
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
        int contents_index = strlen(request->request.contents) + 1;
        mode_string = request->request.contents + contents_index;

        if ((contents_index + 4) < request_length)
        {
            contents_index += strlen(mode_string) + 1;
            blksize_string = request->request.contents + contents_index;

            if ((strcmp(blksize_string, TFTP_BLKSIZE_STRING) == 0)
                && (contents_index + 4 < request_length))
            {
                contents_index += strlen(blksize_string) + 1;
                blksize_octets_string = request->request.contents + contents_index;
            }
        }
    }

    return tftp_init_operation_data(op_id, peer_address, file_path, mode_string, blksize_octets_string);
}

static void init_server_listener_data(ServerListenerData_t *data)
{
    static const int reuse_flag = 1;

    data->requests_socket = socket(AF_INET, SOCK_DGRAM, 0);

    if (data->requests_socket < 0)
    {
        perror("Failed to create requests socket");
        exit(EXIT_FAILURE);
    }

    if(0 > setsockopt(data->requests_socket, SOL_SOCKET, SO_REUSEADDR,  &reuse_flag, sizeof(reuse_flag)))
    {
        perror("Failed to set socket 'reuse address' option");
        exit(EXIT_FAILURE);
    }

    if(0 > setsockopt(data->requests_socket, SOL_SOCKET, SO_REUSEPORT,  &reuse_flag, sizeof(reuse_flag)))
    {
        perror("Failed to set socket 'reuse port' option");
        exit(EXIT_FAILURE);
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
        exit(EXIT_FAILURE);
    }

    data->buffer_size = sizeof(Packet_t) + TFTP_FILENAME_MAX * 2;
    data->request_buffer = malloc(data->buffer_size);

    if (data->request_buffer == NULL)
    {
        perror("Failed to allocate buffer for incoming requests");
        exit(EXIT_FAILURE);
    }
}

static void server_listener_loop(void)
{
    static const char *received_packet_message_format = "Received %s packet in requests socket.\n";

    ServerListenerData_t data;

    init_server_listener_data(&data);

    while(!should_terminate)
    {
        printf("Awaiting requests.\n");

        explicit_bzero(data.request_buffer, data.buffer_size);
        data.bytes_received = recvfrom(data.requests_socket, data.request_buffer, data.buffer_size, 0, (struct sockaddr*)&(data.client_address), &(data.client_address_length));

        if (should_terminate) break;

        if(data.bytes_received < 0)
        {
            // TODO: extract error handling function plz
            perror("Failed to receive bytes");
            continue;
        }
        else if (data.bytes_received == 0)
        {
            printf("Received zero bytes.\n");
            continue;
        }

        printf("Received %lu bytes.\n", data.bytes_received);
        fwrite(data.request_buffer->request.contents, sizeof(char), data.bytes_received, stdout);

        switch (data.request_buffer->opcode)
        {
            // *** Invalid (non-request) opcodes: send an error and move on
            case TFTP_DATA:
            case TFTP_ACK:
            case TFTP_ERROR:
            default:
                fprintf(stderr, received_packet_message_format, tftp_opcode_strings[data.request_buffer->opcode]);
                tftp_send_error(TFTP_ERROR_ILLEGAL_OPERATION, "received packet in requests socket with opcode ", tftp_opcode_strings[data.request_buffer->opcode], data.requests_socket, &data.client_address, data.client_address_length); 
                break;
            // *** Standard request opcodes: parse and begin operation
            case TFTP_RRQ:
            case TFTP_WRQ:
            case TFTP_DRQ:
                printf(received_packet_message_format, tftp_opcode_strings[data.request_buffer->opcode]);
                server_start_operation(server_parse_request_data(data.request_buffer, data.bytes_received, data.client_address));
                break;
        }
    }
}

void server_start(void)
{
    server_init_storage();
    server_listener_loop();
}
