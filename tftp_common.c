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
    int block_number = 1;
    int bytes_read = block_size;
    int data_socket;
    struct sockaddr_in local_address = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY };
    Packet_t ack_packet;
    Packet_t *data_packet_ptr = malloc(sizeof(Packet_t) + block_size);
    socklen_t peer_address_length = sizeof(peer_address);

    tftp_init_bound_data_socket(&data_socket, &local_address);

    data_packet_ptr->data.opcode = TFTP_DATA;

    printf("Beginning file transmission.\n");

    while (bytes_read == block_size)
    {
        acknowledged = false;
        data_packet_ptr->data.block_number = block_number;
        bytes_read = fread(data_packet_ptr->data.data, 1, block_size, file);

        while (!acknowledged)
        {
            sendto(data_socket, data_packet_ptr, bytes_read, 0, (struct sockaddr *)&(peer_address), peer_address_length);
            // TODO: handle errors
            recvfrom(data_socket, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)&(peer_address), &peer_address_length);

            if (ack_packet.opcode == TFTP_ACK && ack_packet.ack.block_number == block_number)
            {
                acknowledged = true;
            }
            else
            {
                printf ("Block #%u still unacknowledged, resending.\n", block_number);
            }
        }

        if (bytes_read < block_size) break;
        block_number++;
    }

    printf("File transmission complete.\n");
}

void receive_file(FILE *file, TFTPMode_t mode, uint16_t block_size, struct sockaddr_in peer_address)
{
    int data_socket;
    struct sockaddr_in data_address;
    Packet_t ack_packet;
    Packet_t *receive_buffer = malloc(block_size);

    tftp_init_bound_data_socket(&data_socket, &data_address);

    ack_packet.opcode = TFTP_ACK;
}
