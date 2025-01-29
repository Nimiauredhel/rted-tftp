#include "tftp_common.h"

const uint8_t tftp_max_retransmit_count = 5;
const uint32_t tftp_ack_timeout = 1000000;
const uint32_t tftp_data_timeout = 1000000;

const char tftp_mode_strings[TFTP_MODES_COUNT][TFTP_MODES_STRING_LENGTH] =
{
    "netascii",
    "octet"
};

CommonData_t tftp_common_data =
{
    .local_address =
    {
        .sin_addr = INADDR_ANY,
        .sin_family = AF_INET,
    }
};

static void tftp_init_bound_data_socket(int *socket_ptr, struct sockaddr_in *address_ptr)
{
    struct timeval socket_timeout;
    socket_timeout.tv_sec = 1;
    socket_timeout.tv_usec = 0;

    *socket_ptr = socket(AF_INET, SOCK_DGRAM, 0);
    // TODO: handle errors here
    setsockopt(*socket_ptr, SOL_SOCKET, SO_RCVTIMEO,  &socket_timeout, sizeof(socket_timeout));

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

void init_storage(void)
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

void transmit_file(FILE *file, TFTPMode_t mode, uint16_t block_size, struct sockaddr_in peer_address)
{
    bool acknowledged = false;
    uint8_t resend_counter = 0;
    int block_number = 0;
    int bytes_sent = block_size;
    int data_socket;
    struct sockaddr_in local_address = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY };
    Packet_t ack_packet;
    Packet_t *data_packet_ptr;
    socklen_t peer_address_length = sizeof(peer_address);

    tftp_init_bound_data_socket(&data_socket, &local_address);

    data_packet_ptr = malloc(sizeof(Packet_t) + block_size);
    data_packet_ptr->data.opcode = TFTP_DATA;

    printf("Beginning file transmission.\n");

    while (bytes_sent == block_size)
    {
        block_number++;
        resend_counter = 0;
        acknowledged = false;
        data_packet_ptr->data.block_number = block_number;
        bytes_sent = fread(data_packet_ptr->data.data, 1, block_size, file);

        while (!acknowledged)
        {
            sendto(data_socket, data_packet_ptr, bytes_sent, 0, (struct sockaddr *)&(peer_address), peer_address_length);
            // TODO: handle errors
            recvfrom(data_socket, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)&(peer_address), &peer_address_length);

            if (ack_packet.opcode == TFTP_ACK && ack_packet.ack.block_number == block_number)
            {
                acknowledged = true;
            }
            else if (resend_counter < tftp_max_retransmit_count)
            {
                resend_counter++;
                printf ("Block #%u still unacknowledged, resending.\n", block_number);
            }
            else
            {
                printf ("Block #%u still unacknowledged, max retransmission limit reached. Aborting.\n", block_number);
                free(data_packet_ptr);
                return;
            }
        }
    }

    free(data_packet_ptr);
    printf("File transmission complete.\n");
}

void receive_file(FILE *file, TFTPMode_t mode, uint16_t block_size, struct sockaddr_in peer_address)
{
    bool finished = false;
    uint8_t resend_counter = 0;
    int prev_block_number = 0;
    int next_block_number = 1;
    int bytes_received = block_size;
    int data_socket;
    struct sockaddr_in local_address = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY };
    Packet_t ack_packet = { .ack.opcode = TFTP_ACK, .ack.block_number = prev_block_number } ;
    Packet_t *data_packet_ptr;
    size_t data_packet_size = sizeof(Packet_t) + block_size;
    socklen_t peer_address_length = sizeof(peer_address);

    tftp_init_bound_data_socket(&data_socket, &local_address);

    data_packet_ptr = malloc(data_packet_size);

    printf("Beginning file reception.\n");

    while (!finished)
    {
        bytes_received = recvfrom(data_socket, data_packet_ptr, data_packet_size, 0, (struct sockaddr *)&(peer_address), &peer_address_length);

        if (bytes_received > 0 || data_packet_ptr->data.block_number == next_block_number)
        {
            if (bytes_received < block_size)
            {
                finished = true;
            }
            else
            {
                resend_counter = 0;
                prev_block_number++;
                next_block_number++;
            }

            fputs(data_packet_ptr->data.data, file);
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
            return;
        }

        sendto(data_socket, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)&(peer_address), peer_address_length);
    }

    free(data_packet_ptr);
    printf("File reception complete.\n");
}
