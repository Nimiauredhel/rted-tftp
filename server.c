#include "server.h"

static void server_cleanup(ServerSideData_t *data)
{
    close(tftp_common_data.data_socket);
    close(data->requests_socket);
    free(data);
}

static void server_process_rrq(ServerSideData_t *data, Packet_t *request, ssize_t request_length)
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

static void server_process_wrq(ServerSideData_t *data, Packet_t *request, ssize_t request_length)
{
}

static void server_listen_loop(ServerSideData_t *data)
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
        bytes_received = recvfrom(data->requests_socket, incoming_request, buffer_size, 0, (struct sockaddr*)&(client_address), &client_address_length);

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
                server_process_rrq(data, incoming_request, bytes_received);
                break;
            case TFTP_WRQ:
                fputs("Received WRQ packet in requests socket.\n", stdout);
                server_process_wrq(data, incoming_request, bytes_received);
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

static void server_init(ServerSideData_t *data)
{
    init_storage();

    tftp_common_data.data_address.sin_family = AF_INET;
    tftp_common_data.data_address.sin_addr.s_addr = INADDR_ANY;
    tftp_common_data.data_socket = socket(AF_INET, SOCK_DGRAM, 0);

    data->requests_address.sin_family = AF_INET;
    data->requests_address.sin_addr.s_addr = INADDR_ANY;
    data->requests_address.sin_port = htons(69);
    data->requests_socket = socket(AF_INET, SOCK_DGRAM, 0);

    uint16_t rx_port;
    int bind_result = -1;

    while (bind_result < 0)
    {
        rx_port = random_range(PORT_MIN, PORT_MAX);
        data->requests_address.sin_port = htons(rx_port);
        bind_result = bind(data->requests_socket,
                (struct sockaddr*)&(data->requests_address),
                sizeof(data->requests_address));
    }

    printf("Randomly selected data port %u.\n", rx_port);
}

void server_start(void)
{
    ServerSideData_t *data = malloc(sizeof(ServerSideData_t));
    server_init(data);
    server_listen_loop(data);
    server_cleanup(data);
}
