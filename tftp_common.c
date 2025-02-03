#include "tftp_common.h"

const uint8_t tftp_max_retransmit_count = 5;
const uint32_t tftp_ack_timeout = 1000000;
const uint32_t tftp_data_timeout = 1000000;

const char *tftp_mode_strings[TFTP_MODES_COUNT] =
{
    "octet",
    "netascii"
};

const OperationMode_t tftp_operation_modes[OPERATION_MODES_COUNT] =
{
    { 2, "serve", "Serve storage folder to clients", "%s %s" },
    { 4, "write", "Write named file to server", "%s %s <server ip> <filename>" },
    { 4, "read", "Read named file from server", "%s %s <server ip> <filename>" },
    { 4, "delete", "Erase named file from server", "%s %s <server ip> <filename>" },
};

OperationData_t *tftp_init_operation_data(TFTPOpcode_t operation, char *peer_address_string, char *filename, char *mode_string, char *blocksize_string)
{
    uint16_t filename_length = strlen(filename) + 1;

    OperationData_t *data = malloc(sizeof(OperationData_t) + filename_length);

    memset(data, 0, sizeof(OperationData_t) + filename_length);

    data->request_opcode = operation;
    strncpy(data->request_description, data->request_opcode == TFTP_WRQ ? "WRITE"
        : data->request_opcode == TFTP_RRQ ? "READ" : "DELETE", 8);
    data->mode = TFTP_MODE_OCTET;
    data->blocksize = 0;
    // TODO: ^ actually extract mode and blocksize from input arguments rather than use the defaults ^
    strcpy(data->path, filename);
    data->path_len = filename_length;

    data->local_address.sin_family = AF_INET;
    data->local_address.sin_addr.s_addr = INADDR_ANY;
    data->peer_address.sin_family = AF_INET;
    data->peer_address.sin_port = htons(69);

    if (0 > inet_pton(AF_INET, peer_address_string, &(data->peer_address.sin_addr)))
    {
        perror("Bad peer address");
        exit(EXIT_FAILURE);
    }

    data->peer_address_length = sizeof(data->peer_address);

    printf("Initialized operation data: %s | %d | %s\n",
            data->path, data->path_len, inet_ntoa(data->peer_address.sin_addr));

    return data;
}

void tftp_init_bound_data_socket(int *socket_ptr, struct sockaddr_in *address_ptr)
{
    struct timeval socket_timeout;
    socket_timeout.tv_sec = 1;
    socket_timeout.tv_usec = 0;

    *socket_ptr = socket(AF_INET, SOCK_DGRAM, 0);

    if (*socket_ptr < 0)
    {
        perror("Failed to create data socket");
        exit(EXIT_FAILURE);
    }

    if( 0 > setsockopt(*socket_ptr, SOL_SOCKET, SO_RCVTIMEO,  &socket_timeout, sizeof(socket_timeout)))
    {
        perror("Failed to set socket timeout");
        exit(EXIT_FAILURE);
    }

    uint16_t rx_port;
    int bind_result = -1;

    while (bind_result < 0)
    {
        rx_port = random_range(PORT_MIN, PORT_MAX);
        address_ptr->sin_port = htons(rx_port);
        bind_result = bind(*socket_ptr,
                (struct sockaddr*)address_ptr,
                sizeof(*address_ptr));
    }

    printf("Created data socket and randomly bound to port %u.\n", rx_port);
}

FILE *tftp_acquire_fd(char *path, char *mode)
{
    // open file for operation
    FILE *file = fopen(path, mode);

    if (file == NULL)
    {
        perror("Failed to open file");
        return NULL;
    }
    else
    {
        printf("Opened/created file: %s.\n", path);
    }

    return file;
}

// TODO: make transmit_file return success bool
void tftp_transmit_file(FILE *file, TFTPTransferMode_t mode, uint16_t block_size, int data_socket, struct sockaddr_in peer_address)
{
    bool acknowledged = false;
    uint8_t resend_counter = 0;
    uint8_t incoming_packet_max_size = sizeof(Packet_t) + 32;
    uint16_t data_packet_max_size;
    uint16_t block_number = 0;
    uint16_t total_block_count;
    int16_t bytes_received = 0;
    int32_t bytes_sent = 0;
    int32_t file_bytes_read = 0;
    uint64_t total_file_size;
    uint64_t total_file_bytes_sent = 0;
    Packet_t *incoming_packet_ptr;
    Packet_t *data_packet_ptr;
    socklen_t peer_address_length = sizeof(peer_address);

    fseek(file, 0L, SEEK_END);
    total_file_size = ftell(file);
    rewind(file);

    if (block_size == 0)
    {
        block_size = TFTP_BLKSIZE_DEFAULT;
        printf("Blocksize unspecified, defaulting to %d.\n", TFTP_BLKSIZE_DEFAULT);
    }
    else if (block_size < TFTP_BLKSIZE_MIN || block_size > TFTP_BLKSIZE_MAX)
    {
        perror("Invalid block size specified");
        //free(data_packet_ptr);
        return;
    }

    total_block_count = (total_file_size / block_size) + 1;

    data_packet_max_size = sizeof(Packet_t) + block_size;

    incoming_packet_ptr = malloc(incoming_packet_max_size);
    data_packet_ptr = malloc(data_packet_max_size);
    data_packet_ptr->data.opcode = TFTP_DATA;

    bytes_sent = data_packet_max_size;
    printf("Beginning transmission of file with total size of %lu bytes, in %u blocks.\n", total_file_size, total_block_count);

    while (bytes_sent == data_packet_max_size)
    {
        block_number++;
        resend_counter = 0;
        acknowledged = false;
        data_packet_ptr->data.block_number = block_number;
        file_bytes_read = fread(data_packet_ptr->data.data, 1, block_size, file);

        printf("Read %d bytes to transmission buffer.\n", file_bytes_read);

        if (file_bytes_read <= 0)
        {
            if (feof(file) != 0)
            {
                printf("Sending final block: %u/%u.\n", block_number, total_block_count);
                file_bytes_read = 0;
            }
            else
            {
                perror("Failed to read from file");
                free(data_packet_ptr);
                free(incoming_packet_ptr);
                close(data_socket);
                return;
            }
        }
        else if (file_bytes_read < block_size)
        {
            printf("Sending final block: %u/%u.\n", block_number, total_block_count);
        }

        while (resend_counter < tftp_max_retransmit_count)
        {
            bytes_sent = sendto(data_socket, data_packet_ptr, sizeof(Packet_t) + file_bytes_read, 0, (struct sockaddr *)&(peer_address), peer_address_length);

            if (bytes_sent < 0)
            {
                perror("Failed to send packet");
                free(data_packet_ptr);
                free(incoming_packet_ptr);
                close(data_socket);
                return;
            }

            if (resend_counter == 0)
            {
                total_file_bytes_sent += file_bytes_read;
            }

            printf("Sent %lu/%lu bytes of block %u/%u.\n", total_file_bytes_sent, total_file_size, block_number, total_block_count);
            bytes_received = recvfrom(data_socket, incoming_packet_ptr, incoming_packet_max_size, 0, (struct sockaddr *)&(peer_address), &peer_address_length);

            if (bytes_received < 0)
            {
                if (errno == ETIMEDOUT)
                {
                    printf("Socket timed out.\n");
                }
                else
                {
                    perror("Failed to receive packet");
                    free(data_packet_ptr);
                    free(incoming_packet_ptr);
                    close(data_socket);
                    return;
                }
            }
            else if (incoming_packet_ptr->opcode == TFTP_ACK && incoming_packet_ptr->ack.block_number == block_number)
            {
                acknowledged = true;
                printf ("Block #%u acknowledged!\n", block_number);
                break;
            }

            if (resend_counter > tftp_max_retransmit_count)
            {
                printf ("Block #%u unacknowledged and retry limit reached. Aborting.\n", block_number);
                free(data_packet_ptr);
                free(incoming_packet_ptr);
                close(data_socket);
                return;
            }

            resend_counter++;
            printf ("Block #%u still unacknowledged, resending (attempt #%d).\n", block_number, resend_counter);
        }
    }

    free(data_packet_ptr);
    free(incoming_packet_ptr);
    close(data_socket);
    printf("File transmission complete.\n");
}

void tftp_send_error(TFTPErrorCode_t error_code, char *error_message, char *error_item, int data_socket, struct sockaddr_in peer_address, socklen_t peer_address_length)
{
    size_t error_message_len = (error_message == NULL ? 0 : strlen(error_message) + (error_item == NULL ? 0 : strlen(error_item)));
    Packet_t *error_packet;

    size_t packet_size = sizeof(Packet_t) + error_message_len + 1;
    error_packet = malloc(packet_size);
    error_packet->opcode = TFTP_ERROR;
    error_packet->error.error_code = error_code;

    if (error_message != NULL)
    {
        strcpy(error_packet->error.error_message, error_message);
    }

    if (error_item != NULL)
    {
        strcat(error_packet->error.error_message, error_item);
    }

    printf("Sending error packet with code %d, message: %s\n", error_packet->error.error_code, error_packet->error.error_message);

    ssize_t bytes_sent = sendto(data_socket, error_packet, packet_size, 0, (struct sockaddr *)&(peer_address), peer_address_length);

    if (bytes_sent <= 0)
    {
        perror("Failed to send error");
    }
}

// TODO: make receive_file return success bool
void tftp_receive_file(FILE *file, TFTPTransferMode_t mode, uint16_t block_size, int data_socket, struct sockaddr_in peer_address)
{
    bool finished = false;
    uint8_t resend_counter = 0;
    int prev_block_number = 0;
    int next_block_number = 1;
    int full_packet_size;
    int bytes_received;
    Packet_t ack_packet = { .ack.opcode = TFTP_ACK, .ack.block_number = prev_block_number } ;
    Packet_t *data_packet_ptr;
    socklen_t peer_address_length = sizeof(peer_address);

    if (block_size == 0)
    {
        block_size = TFTP_BLKSIZE_DEFAULT;
        printf("Blocksize unspecified, defaulting to %d.\n", TFTP_BLKSIZE_DEFAULT);
    }
    else if (block_size < TFTP_BLKSIZE_MIN || block_size > TFTP_BLKSIZE_MAX)
    {
        perror("Invalid block size specified");
        //free(data_packet_ptr);
        return;
    }

    full_packet_size = block_size + sizeof(Packet_t);
    data_packet_ptr = malloc(full_packet_size);

    printf("Beginning file reception.\n");

    while (!finished)
    {
        bytes_received = recvfrom(data_socket, data_packet_ptr, full_packet_size, 0, (struct sockaddr *)&(peer_address), &peer_address_length);

        if (bytes_received > 0 && data_packet_ptr->data.block_number == next_block_number)
        {
            if (bytes_received < full_packet_size)
            {
                finished = true;
            }

            resend_counter = 0;
            prev_block_number++;
            next_block_number++;

            printf ("Block #%u received!\n", prev_block_number);

            fwrite(data_packet_ptr->data.data, 1, bytes_received - sizeof(Packet_t), file); 
        }
        else if (resend_counter < tftp_max_retransmit_count)
        {
            resend_counter++;
            printf ("Block #%u still not received, resending acknowledgement of block #%u.\n", next_block_number, prev_block_number);
        }
        else
        {
            printf ("Block #%u still not received, max retransmission limit reached. Aborting.\n", prev_block_number);
            free(data_packet_ptr);
            fclose(file);
            return;
        }

        ack_packet.ack.block_number = prev_block_number;
        sendto(data_socket, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)&(peer_address), peer_address_length);
    }

    free(data_packet_ptr);
    fclose(file);
    printf("File reception complete.\n");
}
