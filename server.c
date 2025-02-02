#include "server.h"

static void server_process_rrq(Packet_t *request, ssize_t request_length, struct sockaddr_in peer_address)
{
    int contents_index = 0;
    char file_path[TFTP_FILENAME_MAX] = STORAGE_PATH;
    char temp_buff[16];
    TFTPTransferMode_t transfer_mode = TFTP_MODE_UNDEFINED;
    uint16_t block_size = 0;

    strncat(file_path, request->request.contents, TFTP_FILENAME_MAX);

    contents_index += strlen(file_path + 1);
    strncpy(temp_buff, request->request.contents + contents_index, 16);

    for (int8_t idx = 0; idx < TFTP_MODES_COUNT; idx++)
    {
        if (strcmp(temp_buff, tftp_mode_strings[idx]) == 0)
        {
            transfer_mode = (TFTPTransferMode_t)idx;
        }
    }

    if (transfer_mode == TFTP_MODE_UNDEFINED)
    {
        // TODO: handle invalid mode string
        printf("Invalid transfer mode specified, defaulting to Octet.\n");
        transfer_mode = TFTP_MODE_OCTET;
    }

    if ((contents_index + 4) < request_length)
    {
        contents_index += strlen(temp_buff + 1);
        strncpy(temp_buff, request->request.contents + contents_index, 16);

        if (strcmp(temp_buff, TFTP_BLKSIZE_STRING) == 0)
        {
            contents_index += strlen(temp_buff + 1);
            strncpy(temp_buff, request->request.contents + contents_index, 16);
            block_size = atoi(temp_buff);
        }
    }

    if (block_size == 0)
    {
        printf("Block size unspecified, defaulting to %d.\n", TFTP_BLKSIZE_DEFAULT);
        block_size = TFTP_BLKSIZE_DEFAULT;
    }
    else if (block_size < TFTP_BLKSIZE_MIN || block_size > TFTP_BLKSIZE_MAX)
    {
        printf("Requested block size not supported, aborting.\n");
        // TODO: send error packet
        return;
    }

    FILE *file = fopen(file_path, "r");

    if (file == NULL)
    {
        printf("Requested file not found: %s\n", file_path);
        // TODO: send error packet
        return;
    }

    printf("Requested file found: %s\n", file_path);

    int data_socket;
    struct sockaddr_in data_address = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY };
    socklen_t peer_address_length = sizeof(peer_address);

    tftp_init_bound_data_socket(&data_socket, &data_address);

    // acknowledge request
    Packet_t ack_packet = { .ack.opcode = TFTP_ACK, .ack.block_number = 0 };
    sendto(data_socket, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)&(peer_address), peer_address_length);

    // send file
    tftp_transmit_file(file, transfer_mode, 512, data_socket, peer_address);
}

static void server_process_wrq(Packet_t *request, ssize_t request_length, struct sockaddr_in peer_address)
{
    int contents_index = 0;
    char file_path[TFTP_FILENAME_MAX] = STORAGE_PATH;
    char temp_buff[16];
    TFTPTransferMode_t mode = TFTP_MODE_UNDEFINED;
    uint16_t block_size = 0;

    strncat(file_path, request->request.contents, TFTP_FILENAME_MAX - strlen(STORAGE_PATH));

    contents_index += strlen(file_path + 1);
    strncpy(temp_buff, request->request.contents + contents_index, 16);

    for (int8_t idx = 0; idx < TFTP_MODES_COUNT; idx++)
    {
        if (strcmp(temp_buff, tftp_mode_strings[idx]) == 0)
        {
            mode = (TFTPTransferMode_t)idx;
        }
    }

    if (mode == TFTP_MODE_UNDEFINED)
    {
        // TODO: handle invalid mode string
    }

    if ((contents_index + 4) < request_length)
    {
        contents_index += strlen(temp_buff + 1);
        strncpy(temp_buff, request->request.contents + contents_index, 16);

        if (strcmp(temp_buff, TFTP_BLKSIZE_STRING) == 0)
        {
            contents_index += strlen(temp_buff + 1);
            strncpy(temp_buff, request->request.contents + contents_index, 16);
            block_size = atoi(temp_buff);
        }
    }

    if (block_size < TFTP_BLKSIZE_MIN || block_size > TFTP_BLKSIZE_MAX)
    {
        block_size = TFTP_BLKSIZE_DEFAULT;
    }

    FILE *file = fopen(file_path, "r");

    if (file == NULL)
    {
        printf("New file will be written: %s\n", file_path);
    }
    else
    {
        printf("File already exists and will be overwritten: %s\n", file_path);
        fclose(file);
    }

    file = fopen(file_path, "w");

    if (file == NULL)
    {
        perror("Failed to open file for writing");
        return;
    }

    int data_socket;
    struct sockaddr_in data_address = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY };
    socklen_t peer_address_length = sizeof(peer_address);

    tftp_init_bound_data_socket(&data_socket, &data_address);

    // acknowledge request
    Packet_t ack_packet = { .ack.opcode = TFTP_ACK, .ack.block_number = 0 };
    sendto(data_socket, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)&(peer_address), peer_address_length);

    // receive file
    tftp_receive_file(file, TFTP_MODE_NETASCII, 512, data_socket, peer_address);
}

static void server_process_drq(Packet_t *request, struct sockaddr_in peer_address)
{
    int data_socket;
    struct sockaddr_in data_address = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY };
    socklen_t peer_address_length = sizeof(peer_address);

    char file_path[TFTP_FILENAME_MAX] = STORAGE_PATH;

    tftp_init_bound_data_socket(&data_socket, &data_address);

    // acknowledge request
    Packet_t ack_packet = { .ack.opcode = TFTP_ACK, .ack.block_number = 0 };
    sendto(data_socket, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)&(peer_address), peer_address_length);

    strncat(file_path, request->request.contents, TFTP_FILENAME_MAX - strlen(STORAGE_PATH));

    FILE *file = fopen(file_path, "r");

    if (file == NULL)
    {
        // TODO: send ERROR packet
        printf("File not found: %s\n", file_path);
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("File exists and will be deleted: %s\n", file_path);
        fclose(file);
    }

    if (0 > remove(file_path))
    {
        // TODO: send ERROR packet
        perror("Failed to delete file");
        exit(EXIT_FAILURE);
    }

    // confirm deletion
    printf("File deleted successfully: %s\n", file_path);
    ack_packet.ack.block_number = 1;
    sendto(data_socket, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)&(peer_address), peer_address_length);
}

static void server_listen_loop(void)
{
    ServerData_t data = {0};
    Packet_t *request_buffer;

    data.requests_socket = socket(AF_INET, SOCK_DGRAM, 0);

    if (data.requests_socket < 0)
    {
        perror("Failed to create requests socket");
        exit(EXIT_FAILURE);
    }

    data.requests_address.sin_addr.s_addr = INADDR_ANY;
    data.requests_address.sin_port = htons(69);

    data.client_address.sin_family = AF_INET;
    data.client_address_length = sizeof(data.client_address);

    int bind_result = bind(data.requests_socket, (struct sockaddr *)&(data.requests_address), sizeof(data.requests_address));

    if (bind_result < 0)
    {
        perror("Could not bind requests socket");
        exit(EXIT_FAILURE);
    }

    data.buffer_size = sizeof(Packet_t) + TFTP_FILENAME_MAX * 2;
    request_buffer = malloc(data.buffer_size);

    if (request_buffer == NULL)
    {
        perror("Failed to allocate buffer for incoming requests");
        exit(EXIT_FAILURE);
    }

    while(!should_terminate)
    {
        printf("Awaiting requests.\n");

        memset(request_buffer, 0, data.buffer_size);
        data.bytes_received = recvfrom(data.requests_socket, request_buffer, data.buffer_size, 0, (struct sockaddr*)&(data.client_address), &(data.client_address_length));

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
        else
        {
            printf("Received %lu bytes.\n", data.bytes_received);
        }

        switch (request_buffer->opcode)
        {
            case TFTP_RRQ:
                fputs("Received RRQ packet in requests socket.\n", stdout);
                server_process_rrq(request_buffer, data.bytes_received, data.client_address);
                break;
            case TFTP_WRQ:
                fputs("Received WRQ packet in requests socket.\n", stdout);
                server_process_wrq(request_buffer, data.bytes_received, data.client_address);
                break;
            case TFTP_DATA:
                fputs("Received DATA packet in requests socket.\n", stderr);
                break;
            case TFTP_ACK:
                fputs("Received ACK packet in requests socket.\n", stderr);
                break;
            case TFTP_ERROR:
                fputs("Received ERROR packet in requests socket.\n", stderr);
                break;
            case TFTP_DRQ:
                fputs("Received DRQ (DELETE) packet in requests socket.\n", stdout);
                server_process_drq(request_buffer, data.client_address);
                break;
        }
    }
}

void server_start(void)
{
    tftp_init_storage();
    server_listen_loop();
}
