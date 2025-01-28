#include "server.h"

static void server_process_rrq(Packet_t *request, ssize_t request_length)
{
    int contents_index = 0;
    char file_name[TFTP_FILENAME_MAX] = {0};
    char temp_buff[16];
    TFTPMode_t mode = TFTP_MODE_UNDEFINED;
    uint16_t block_size = 0;

    strncpy(file_name, request->request.contents, TFTP_FILENAME_MAX);

    contents_index += strlen(file_name + 1);
    strncpy(temp_buff, request->request.contents + contents_index, 16);

    for (int8_t idx = 0; idx < TFTP_MODES_COUNT; idx++)
    {
        if (strcmp(temp_buff, tftp_mode_strings[idx]) == 0)
        {
            mode = (TFTPMode_t)idx;
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

    FILE *file = fopen(file_name, "r");

    if (file == NULL)
    {
        printf("Requested file not found: %s\n", file_name);
        return;
    }

    printf("Requested file found: %s\n", file_name);
}

static void server_process_wrq(Packet_t *request, ssize_t request_length)
{
}

static void server_listen_loop(void)
{
    ssize_t bytes_received;
    size_t buffer_size = sizeof(Packet_t) + TFTP_FILENAME_MAX;
    Packet_t *incoming_request = malloc(buffer_size);

    struct sockaddr_in client_address;
    socklen_t client_address_length = sizeof(client_address);

    printf("Awaiting requests.\n");

    while(!should_terminate)
    {
        memset(incoming_request, 0, buffer_size);
        bytes_received = recvfrom(tftp_common_data.primary_socket, incoming_request, buffer_size, 0, (struct sockaddr*)&(client_address), &client_address_length);

        if (should_terminate) break;

        if(bytes_received <= 0)
        {
            // TODO: extract error handling function plz
            perror("Failed to receive bytes");
            break;
        }

        switch (incoming_request->opcode)
        {
            case TFTP_RRQ:
                fputs("Received RRQ packet in requests socket.\n", stdout);
                server_process_rrq(incoming_request, bytes_received);
                break;
            case TFTP_WRQ:
                fputs("Received WRQ packet in requests socket.\n", stdout);
                server_process_wrq(incoming_request, bytes_received);
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
        }
    }
}

static void server_init(void)
{
    init_storage();
    tftp_common_data.local_address.sin_port = htons(69);
    tftp_common_data.primary_socket = socket(AF_INET, SOCK_DGRAM, 0);
}

void server_start(void)
{
    server_init();
    server_listen_loop();
}
