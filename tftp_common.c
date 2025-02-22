#include "tftp_common.h"

/**
 * This macro is used to check for a user termination signal at multiple points during file transfer,
 * to ensure a timely response to user input without adding too much clutter.
 */
#define CHECK_SIGTERM_DURING_TRANSFER \
        if (should_terminate) { \
        printf("User requested termination - aborting.\n"); \
        tftp_send_error(TFTP_ERROR_UNDEFINED, \
        tftp_common.is_server ? "Server program terminated" : "Client program terminated", \
        NULL, op_data->data_socket, &op_data->peer_address, op_data->peer_address_length); \
        return false; } \

/**
 * This struct holds data common to TFTP client and server operations.
 * The 'is_server' flag is the only non-const field, as it relies on user input.
 */
TFTPCommonData_t tftp_common =
{
    .is_server = false,
    .max_retry_count = 5,
    .operation_modes =
    {
        { 2, "serve", "Serve storage folder to clients", "%s %s" },
        { 4, "write", "Write named file to server", "%s %s <server ip> <filename> [transfer mode] [block size]" },
        { 4, "read", "Read named file from server", "%s %s <server ip> <filename> [transfer mode] [block size]" },
        { 4, "delete", "Erase named file from server", "%s %s <server ip> <filename>" },
    },
    .transfer_mode_strings =
    {
        "octet",
        "netascii",
        "mail"
    },
    .opcode_strings =
    {
        "NONE",
        "RRQ",
        "WRQ",
        "DATA",
        "ACK",
        "ERROR",
        "DRQ",
    },
};

/**
 * Converts the input address and port to an IPv4 socket address struct.
 */
struct sockaddr_in init_peer_socket_address(struct in_addr peer_address_bin, in_port_t peer_port_bin)
{
    struct sockaddr_in socket_address =
    {
        .sin_family = AF_INET,
        .sin_port = peer_port_bin,
        .sin_addr = peer_address_bin
    };

    return socket_address;
}

/**
 * initializes a socket for TFTP data operations, binds it to a random ephemeral port,
 * and sets some convenient flags for consistent operation.
 */
void tftp_init_bound_data_socket(int *socket_ptr, struct sockaddr_in *address_ptr)
{
    static const int reuse_flag = 1;
    static const struct timeval socket_timeout = { .tv_sec = 1, .tv_usec = 0 };

    *socket_ptr = socket(AF_INET, SOCK_DGRAM, 0);

    if (*socket_ptr < 0)
    {
        perror("Failed to create data socket");
        exit(EXIT_FAILURE);
    }

    if(0 > setsockopt(*socket_ptr, SOL_SOCKET, SO_REUSEADDR,  &reuse_flag, sizeof(reuse_flag)))
    {
        perror("Failed to set socket 'reuse address' option");
        exit(EXIT_FAILURE);
    }

    if(0 > setsockopt(*socket_ptr, SOL_SOCKET, SO_REUSEPORT,  &reuse_flag, sizeof(reuse_flag)))
    {
        perror("Failed to set socket 'reuse port' option");
        exit(EXIT_FAILURE);
    }

    if(0 > setsockopt(*socket_ptr, SOL_SOCKET, SO_RCVTIMEO,  &socket_timeout, sizeof(socket_timeout)))
    {
        perror("Failed to set socket timeout");
        exit(EXIT_FAILURE);
    }

    uint16_t rx_port;
    int bind_result = -1;

    while (bind_result < 0)
    {
        rx_port = random_range(tftp_common.is_server ? SERVER_DATA_PORT_MIN : CLIENT_DATA_PORT_MIN,
                tftp_common.is_server ? SERVER_DATA_PORT_MAX : CLIENT_DATA_PORT_MAX);
        address_ptr->sin_port = htons(rx_port);
        bind_result = bind(*socket_ptr,
                (struct sockaddr*)address_ptr,
                sizeof(*address_ptr));
    }

    if (bind_result < 0)
    {
        perror("Somehow failed to bind to an ephemeral socket");
        exit(EXIT_FAILURE);
    }

    printf("Created data socket and randomly bound to port %u.\n", rx_port);
}

/**
 * This function allocates and initializes an OperationData_t struct which is used to define all TFTP operations,
 * whether they eventually involve a file transfer or not.
 */
OperationData_t *tftp_init_operation_data(OperationId_t operation, struct sockaddr_in peer_address, char *filename, char *mode_string, char *blocksize_string)
{
    bool is_delete = false;
    uint16_t filename_length = strlen(filename) + 1;

    OperationData_t *data = malloc(sizeof(OperationData_t) + filename_length);

    explicit_bzero(data, sizeof(OperationData_t) + filename_length);

    // initializing addresses and binding the socket first,
    // so that we can send an error later if required
    data->local_address.sin_family = AF_INET;
    data->local_address.sin_addr.s_addr = INADDR_ANY;

    data->peer_address = peer_address;
    data->peer_address_length = sizeof(data->peer_address);

    tftp_init_bound_data_socket(&data->data_socket, &data->local_address);

    // filling in the rest of the data
    data->operation_id = operation;

    switch(data->operation_id)
    {
        case TFTP_OPERATION_RECEIVE:
            strcpy(data->request_description, "READ");
            break;
        case TFTP_OPERATION_SEND:
            strcpy(data->request_description, "WRITE");
            break;
        case TFTP_OPERATION_REQUEST_DELETE:
        case TFTP_OPERATION_HANDLE_DELETE:
            strcpy(data->request_description, "DELETE");
            is_delete = true;
            break;
        case TFTP_OPERATION_UNDEFINED:
            fputs("Attempted to parse undefined operation.\n", stderr);
            tftp_send_error(TFTP_ERROR_ILLEGAL_OPERATION, "Invalid operation ID", NULL, data->data_socket, &data->peer_address, data->peer_address_length); 
            tftp_free_operation_data(data);
            return NULL;
    }

    strcpy(data->path, filename);
    data->path_len = filename_length;

    if (!is_delete)
    {
        // if no transfer mode is specified, we default to binary
        if (mode_string == NULL || strlen(mode_string) == 0)
        {
            data->transfer_mode = TFTP_MODE_OCTET;
            printf("Transfer mode unspecified - defaulting to octet (binary) mode.\n");
        }
        // if the transfer mode IS specified, we try to match it to one we recognize
        else
        {
            data->transfer_mode = TFTP_MODE_UNSPECIFIED;

            for (int8_t idx = 0; idx < TFTP_TRANSFER_MODES_COUNT; idx++)
            {
                if (strcasecmp(mode_string, tftp_common.transfer_mode_strings[idx]) == 0)
                {
                    data->transfer_mode = (TFTPTransferMode_t)idx;
                    break;
                }
            }
        }

        // filtering unsupported transfer modes (only octet is supported)
        // you: "this switch case could have been a single if statement!"
        // me: "some day I shall implement netascii and mail and you will regret your words and deeds"
        switch(data->transfer_mode)
        {
            // *** unsupported transfer modes ***
            // if the transfer mode is STILL unspecified, it means that the client
            // likely requested one that we don't know and cannot support, therefore invalid
            case TFTP_MODE_UNSPECIFIED:
            // this program does not implement netascii conversion yet,
            // so netascii requests will be sadly rejected as well.
            // TODO: support netascii some day, maybe? to be determined
            case TFTP_MODE_NETASCII:
            // this program will NEVER support TFTP e-mail forwarding. unless I get paid to implement it.
            // please contact me ASAP if you would like to pay me to implement TFTP e-mail forwarding in the current year.
            case TFTP_MODE_MAIL:
                printf("Invalid transfer mode (%s) specified! Aborting.\n", mode_string == NULL ? "NULL" : mode_string);
                tftp_send_error(TFTP_ERROR_ILLEGAL_OPERATION, "invalid transfer mode: ", mode_string, data->data_socket, &data->peer_address, data->peer_address_length); 
                tftp_free_operation_data(data);
                return NULL;
            // *** supported transfer modes ***
            case TFTP_MODE_OCTET:
                break;
        }

        printf("Transfer mode: (%s).\n", mode_string);

        // determine requested block size
        data->block_size = 0;

        if (blocksize_string != NULL)
        {
            data->block_size = atoi(blocksize_string);
        }

        if (data->block_size == 0)
        {
            printf("Block size unspecified, defaulting to %d (this is normal!).\n", TFTP_BLKSIZE_DEFAULT);
            data->block_size = TFTP_BLKSIZE_DEFAULT;
        }
        else if (data->block_size < TFTP_BLKSIZE_MIN || data->block_size > TFTP_BLKSIZE_MAX)
        {
            printf("Requested block size (%u bytes) not supported! aborting.\n", data->block_size);
            char range_str[16];
            sprintf(range_str, "%d-%d", TFTP_BLKSIZE_MIN, TFTP_BLKSIZE_MAX);
            tftp_send_error(TFTP_ERROR_ILLEGAL_OPERATION, "unsupported block size. Valid range is ", range_str, data->data_socket, &data->peer_address, data->peer_address_length); 
            tftp_free_operation_data(data);
            return NULL;
        }
        else
        {
            printf("Transfer block size: %u bytes.\n", data->block_size);
        }
    }

    printf("Initialized operation data: %s | %s | %s\n",
            data->request_description, inet_ntoa(data->peer_address.sin_addr), data->path);

    return data;
}

/**
 * Frees an operation data struct after a TFTP operation has been either completed or aborted.
 */
void tftp_free_operation_data(OperationData_t *data)
{
    printf("Deallocating '%s' operation data.\n", data->request_description);

    if (data->data_socket > 0)
    {
        close(data->data_socket);
    }

    free(data);
}

/**
 * This function initializes a pre-allocated TransferData_t struct,
 * which is used during file transfers.
 * The same structure is used to handle both transmission and reception operations.
 */
bool tftp_fill_transfer_data(OperationData_t *operation_data, TransferData_t *transfer_data, bool receiver)
{
    if (transfer_data == NULL)
    {
        printf("Passed null TransferData_t pointer! Aborting.\n");
        tftp_send_error(TFTP_ERROR_UNDEFINED, "Internal server error", NULL, operation_data->data_socket, &operation_data->peer_address, operation_data->peer_address_length);
        return false;
    }

    explicit_bzero(transfer_data, sizeof(TransferData_t));

    // receiving-end specific checks
    if (receiver)
    {
        transfer_data->file = fopen(operation_data->path, "rb");

        // when trying to receive a file that already exists,
        // it makes sense to notify our peer of the file's last modified date,
        // so they can reason about requesting deletion to effectively overwrite it.
        if (transfer_data->file != NULL)
        {
            char timestamp[32];
            struct stat file_attr;
            stat(operation_data->path, &file_attr);

            struct tm tm;
            localtime_r(&file_attr.st_ctim.tv_sec, &tm);
            strftime(timestamp, 32, "%Y-%m-%d %H:%M:%S.", &tm);

            printf("File already exists since %s. Aborting receive operation.\n", timestamp);
            tftp_send_error(operation_data->data_socket, "File already exists! To overwrite, request deletion then try again. Creation date: ", timestamp, operation_data->data_socket, &operation_data->peer_address, operation_data->peer_address_length);
            return false;
        }
    }

    printf("%s file: '%s'\n", receiver ? "Creating" : "Opening", operation_data->path);
    transfer_data->file = fopen(operation_data->path, receiver ? "wb" : "rb");

    if (transfer_data->file == NULL)
    {
        perror("Failed to acquire file descriptor");
        tftp_send_error(TFTP_ERROR_UNDEFINED, "Failed to acquire file descriptor, details: ", strerror(errno), operation_data->data_socket, &operation_data->peer_address, operation_data->peer_address_length);
        return false;
    }

    // The TFTP default block size is 512 bytes, but we support the BLKSIZE extension.
    // A value of 0 means that no BLKSIZE field was passed, so it is interpreted as the default value.
    if (operation_data->block_size == 0)
    {
        operation_data->block_size = TFTP_BLKSIZE_DEFAULT;
        printf("Block size unspecified, defaulting to %d bytes.\n", TFTP_BLKSIZE_DEFAULT);
    }
    // Else, we apply the requested block size, given that it fits within the permitted range.
    // If it exceeds the permitted range we simply reject the request.
    else if (operation_data->block_size < TFTP_BLKSIZE_MIN || operation_data->block_size > TFTP_BLKSIZE_MAX)
    {
        printf("Invalid block size specified");
        tftp_send_error(TFTP_ERROR_ILLEGAL_OPERATION, "Invalid block size specified", NULL, operation_data->data_socket, &operation_data->peer_address, operation_data->peer_address_length);
        return false;
    }
    else
    {
        printf("Specified block size: %d bytes.\n", operation_data->block_size);
    }

    transfer_data->data_packet_max_size = sizeof(Packet_t) + operation_data->block_size;
    transfer_data->response_packet_max_size = sizeof(Packet_t) + 32;

    transfer_data->data_packet_ptr = malloc(transfer_data->data_packet_max_size);
    transfer_data->response_packet_ptr = malloc(transfer_data->response_packet_max_size);

    if (transfer_data->data_packet_ptr == NULL || transfer_data->response_packet_ptr == NULL)
    {
        perror("Failed to allocate packet buffers");
        tftp_send_error(TFTP_ERROR_OUT_OF_SPACE, "Failed to allocate packet buffers: ", strerror(errno), operation_data->data_socket, &operation_data->peer_address, operation_data->peer_address_length);
        return false;
    }

    return true;
}

/**
 * Frees a TransferData_t struct after a file transfer operation was either completed or aborted.
 */
void tftp_free_transfer_data(TransferData_t *data)
{
    printf("Deallocating transfer data.\n");

    if (data->file != NULL)
    {
        fclose(data->file);
    }

    if (data->data_packet_ptr != NULL) free(data->data_packet_ptr);
    if (data->response_packet_ptr != NULL) free(data->response_packet_ptr);

    free(data);
}

/**
 * This function implements the core of a file transfer operation,
 * from the transmitting side.
 */
bool tftp_transmit_file(OperationData_t *op_data, TransferData_t *tx_data)
{
    CHECK_SIGTERM_DURING_TRANSFER

    uint64_t total_file_size;
    uint32_t total_block_count;
    uint8_t block_overflow_multiplier = 0;

    fseek(tx_data->file, 0L, SEEK_END);
    total_file_size = ftell(tx_data->file);
    rewind(tx_data->file);

    total_block_count = (total_file_size / op_data->block_size) + 1;

    tx_data->data_packet_ptr->data.opcode = htons(TFTP_DATA);
    tx_data->bytes_sent = tx_data->data_packet_max_size;
    printf("Beginning transmission of file with total size of %lu bytes, in %u blocks.\n", total_file_size, total_block_count);
    clock_gettime(CLOCK_MONOTONIC, &tx_data->start_clock);

    while (tx_data->bytes_sent == tx_data->data_packet_max_size)
    {
        CHECK_SIGTERM_DURING_TRANSFER

        tx_data->current_block_number++;
        tx_data->resend_counter = 0;
        tx_data->data_packet_ptr->data.block_number = htons(tx_data->current_block_number);
        tx_data->latest_file_bytes_read = fread(tx_data->data_packet_ptr->data.data, 1, op_data->block_size, tx_data->file);

        if (tx_data->current_block_number == 0)
        {
            block_overflow_multiplier++;
        }

        printf("\r[%.2fs] Read %d bytes to transmission buffer -> ", seconds_since_clock(tx_data->start_clock), tx_data->latest_file_bytes_read);

        if (tx_data->latest_file_bytes_read <= 0)
        {
            if (feof(tx_data->file) != 0)
            {
                printf("Sending final block: %u/%u.\n", tx_data->current_block_number + (UINT16_MAX * block_overflow_multiplier), total_block_count);
                tx_data->latest_file_bytes_read = 0;
            }
            else
            {
                perror("Failed to read from file");
                tftp_send_error(TFTP_ERROR_UNDEFINED, "File error", NULL, op_data->data_socket, &op_data->peer_address, op_data->peer_address_length);
                return false;
            }
        }
        else if (tx_data->latest_file_bytes_read < op_data->block_size)
        {
            printf("Sending final block: %u/%u.\n", tx_data->current_block_number + (UINT16_MAX * block_overflow_multiplier), total_block_count);
        }

        while (tx_data->resend_counter < tftp_common.max_retry_count)
        {
            CHECK_SIGTERM_DURING_TRANSFER

            tx_data->bytes_sent = sendto(op_data->data_socket, tx_data->data_packet_ptr, (sizeof(Packet_t) + tx_data->latest_file_bytes_read), 0, (struct sockaddr *)&(op_data->peer_address), op_data->peer_address_length);

            if (tx_data->bytes_sent < 0)
            {
                perror("Failed to send packet");
                tftp_send_error(TFTP_ERROR_UNDEFINED, "Socket tx error", NULL, op_data->data_socket, &op_data->peer_address, op_data->peer_address_length);
                return false;
            }

            if (tx_data->resend_counter == 0)
            {
                tx_data->total_file_bytes_transmitted += tx_data->latest_file_bytes_read;
            }

            printf("Sent %lu/%lu bytes of block %u/%u -> ", tx_data->total_file_bytes_transmitted, total_file_size, tx_data->current_block_number + (UINT16_MAX * block_overflow_multiplier), total_block_count);
            fflush(stdout);

            tx_data->bytes_received = recvfrom(op_data->data_socket, tx_data->response_packet_ptr, tx_data->response_packet_max_size, 0, (struct sockaddr *)&(op_data->peer_address), &(op_data->peer_address_length));

            if (tx_data->bytes_received < 0)
            {
                if (errno == ETIMEDOUT)
                {
                    printf("\nSocket timed out.\n");
                }
                else
                {
                    perror("Failed to receive packet");
                    tftp_send_error(TFTP_ERROR_UNDEFINED, "Socket rx error", NULL, op_data->data_socket, &op_data->peer_address, op_data->peer_address_length);
                    return false;
                }
            }
            else if (ntohs(tx_data->response_packet_ptr->opcode == TFTP_ERROR))
            {
                printf("\nReceived error message (code %u) from peer with message: %s\n", ntohs(tx_data->response_packet_ptr->error.error_code), tx_data->response_packet_ptr->error.error_message);
                return false;
            }
            else if (ntohs(tx_data->response_packet_ptr->opcode) == TFTP_ACK
                    && ntohs(tx_data->response_packet_ptr->ack.block_number) == tx_data->current_block_number)
            {
                printf ("Block #%u acknowledged!\r", tx_data->current_block_number);
                break;
            }

            if (tx_data->resend_counter > tftp_common.max_retry_count)
            {
                printf ("\nBlock #%u unacknowledged and retry limit reached. Aborting.\n", tx_data->current_block_number);
                tftp_send_error(TFTP_ERROR_UNDEFINED, "Timed out waiting for acknowledgement", NULL, op_data->data_socket, &op_data->peer_address, op_data->peer_address_length);
                return false;
            }

            tx_data->resend_counter++;
            printf ("Block #%u still unacknowledged, resending (attempt #%d).\n", tx_data->current_block_number, tx_data->resend_counter);
        }
    }

    printf("\nFile transmission completed in %.2fs.\n", seconds_since_clock(tx_data->start_clock));
    return true;
}

/**
 * This function implements the core of a file transfer operation,
 * from the receiving side.
 */
bool tftp_receive_file(OperationData_t *op_data, TransferData_t *tx_data)
{
    CHECK_SIGTERM_DURING_TRANSFER

    bool received = false;
    uint16_t prev_block_number = 0;
    ssize_t bytes_written = 0;

    tx_data->current_block_number = 1;
    printf("Beginning file reception.\n");
    clock_gettime(CLOCK_MONOTONIC, &tx_data->start_clock);

    do
    {
        CHECK_SIGTERM_DURING_TRANSFER

        received = false;

        while (!received)
        {
            tx_data->bytes_received = recvfrom(op_data->data_socket, tx_data->data_packet_ptr, tx_data->data_packet_max_size, 0, (struct sockaddr *)&(op_data->peer_address), &(op_data->peer_address_length));

            if (tx_data->bytes_received > 0)
            {
                if (ntohs(tx_data->data_packet_ptr->opcode) == TFTP_ERROR)
                {
                    printf("\nReceived error message (code %u) from peer with message: %s\n", ntohs(tx_data->data_packet_ptr->error.error_code), tx_data->data_packet_ptr->error.error_message);
                    return false;
                }
                else if  (ntohs(tx_data->data_packet_ptr->opcode) == TFTP_DATA && ntohs(tx_data->data_packet_ptr->data.block_number) == tx_data->current_block_number)
                {
                    printf ("[%0.2fs] Block #%u received! -> ", seconds_since_clock(tx_data->start_clock), tx_data->current_block_number);
                    bytes_written = fwrite(tx_data->data_packet_ptr->data.data, 1, tx_data->bytes_received - sizeof(Packet_t), tx_data->file); 

                    if (bytes_written <= 0)
                    {
                        perror("Writing to file failed");
                        tftp_send_error(TFTP_ERROR_UNDEFINED, "Writing to file failed", NULL, op_data->data_socket, &op_data->peer_address, op_data->peer_address_length);
                        return false;
                    }

                    // acknowledge received block
                    tftp_send_ack(tx_data->current_block_number, op_data->data_socket, &op_data->peer_address, op_data->peer_address_length);

                    tx_data->resend_counter = 0;
                    tx_data->current_block_number++;
                    prev_block_number++;
                    received = true;
                }
            }
            else if (tx_data->resend_counter < tftp_common.max_retry_count)
            {
                perror("Receive attempt failed");
                tx_data->resend_counter++;
                printf ("[%0.2fs] Block #%u still not received, resending acknowledgement of block #%u.\n", seconds_since_clock(tx_data->start_clock), tx_data->current_block_number, prev_block_number);
            }
            else
            {
                printf ("[%0.2fs] Block #%u still not received, max retransmission limit reached. Aborting.\n", seconds_since_clock(tx_data->start_clock), tx_data->current_block_number);
                tftp_send_error(TFTP_ERROR_UNDEFINED, "Timed out waiting for data packet", NULL, op_data->data_socket, &op_data->peer_address, op_data->peer_address_length);
                return false;
            }
        }
    }
    while (tx_data->bytes_received == tx_data->data_packet_max_size);

    printf("File reception complete in %0.2fs.\n", seconds_since_clock(tx_data->start_clock));
    return true;
}

/**
 * This function sends an acknowledgement packet to the specified peer.
 * It does NOT handle receiving a response.
 * The return value is only false if an error prevented packet transmission.
 */
bool tftp_send_ack(uint16_t block_number, int socket, const struct sockaddr_in *peer_address_ptr, socklen_t peer_address_length)
{
    printf("Sending ACK with block number %u.\n", block_number);
    Packet_t ack_packet = { .ack.opcode = htons(TFTP_ACK), .ack.block_number = htons(block_number) };

    if (0 > sendto(socket, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)peer_address_ptr, peer_address_length))
    {
        perror("Failed to send ack");
        return false;
    }

    return true;
}

/**
 * This function sends an error packet to the specified peer.
 */
void tftp_send_error(TFTPErrorCode_t error_code, const char *error_message, const char *error_item, int data_socket, const struct sockaddr_in *peer_address_ptr, socklen_t peer_address_length)
{
    if (peer_address_ptr->sin_port == htons(69))
    {
        printf("Errors during request phase not forwarded to peer (no peer yet).\n");
        return;
    }

    size_t error_message_len = (error_message == NULL ? 0 : strlen(error_message) + (error_item == NULL ? 0 : strlen(error_item)));
    size_t packet_size = sizeof(Packet_t) + error_message_len + 2;
    Packet_t *error_packet = malloc(packet_size);
    explicit_bzero(error_packet, packet_size);

    error_packet->opcode = htons(TFTP_ERROR);
    error_packet->error.error_code = htons(error_code);

    if (error_message != NULL)
    {
        strcpy(error_packet->error.error_message, error_message);
    }

    if (error_item != NULL)
    {
        strcat(error_packet->error.error_message, error_item);
    }

    printf("Sending error packet with code %d, message: %s%s\n", error_code, error_message, error_item);

    ssize_t bytes_sent = sendto(data_socket, error_packet, packet_size, 0, (struct sockaddr *)peer_address_ptr, peer_address_length);

    if (bytes_sent <= 0)
    {
        perror("Failed to send error");
    }

    free(error_packet);
}

/**
 * This function handles reception of an ACK packet at the operation's given DATA socket.
 * It returns true if the expected packet with the correct block number has been received.
 * It returns false if the retry count has been exceeded, or if it receives an error packet.
 */
bool tftp_await_acknowledgement(uint16_t block_number, OperationData_t *op_data)
{
    uint8_t retry_counter = 0;
    ssize_t bytes_received = 0;
    TFTPOpcode_t incoming_opcode;

    // dynamically allocating the packet to allow extra space for error packet message field
    Packet_t *incoming_packet = malloc(TFTP_RESPONSE_PACKET_MAX_SIZE);

    while (retry_counter < tftp_common.max_retry_count)
    {
        retry_counter++;
        printf("Awaiting ACK packet (attempt #%d).\n", retry_counter);
        bytes_received = recvfrom(op_data->data_socket, incoming_packet, TFTP_RESPONSE_PACKET_MAX_SIZE, 0, (struct sockaddr *)&(op_data->peer_address), &(op_data->peer_address_length));

        if (bytes_received > 0)
        {
            incoming_opcode = ntohs(incoming_packet->opcode);

            if (incoming_opcode == TFTP_ACK && ntohs(incoming_packet->ack.block_number) == block_number)
            {
                free(incoming_packet);
                return true;
            }
            else if (incoming_opcode == TFTP_ERROR)
            {
                printf("Received error message (code %u) from peer with message: %s\n", ntohs(incoming_packet->error.error_code), incoming_packet->error.error_message);
                free(incoming_packet);
                return false;
            }
            else
            {
                printf("Received packet with opcode %d, expected %d (ACK) or %d (ERROR)!\n", incoming_opcode, TFTP_ACK, TFTP_ERROR);
            }
        }
    }

    printf("ACK reception retry limit (%u) reached.\n", tftp_common.max_retry_count);

    free(incoming_packet);
    return false;
}
