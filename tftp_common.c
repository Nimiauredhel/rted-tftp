#include "tftp_common.h"

const uint8_t tftp_max_retransmit_count = 5;
const uint32_t tftp_ack_timeout = 1000000;
const uint32_t tftp_data_timeout = 1000000;

const char tftp_mode_strings[TFTP_MODES_COUNT][TFTP_MODES_STRING_LENGTH] =
{
    "netascii\0",
    "octet\0"
};

CommonData_t tftp_common_data =
{
    .local_address =
    {
        .sin_addr = INADDR_ANY,
        .sin_family = AF_INET,
    }
};

void tftp_init_bound_data_socket(int *socket_ptr, struct sockaddr_in *address_ptr)
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

void transmit_file(FILE *file, TFTPMode_t mode, uint16_t block_size, int data_socket, struct sockaddr_in local_address, struct sockaddr_in peer_address)
{
    bool acknowledged = false;
    uint8_t resend_counter = 0;
    int block_number = 0;
    int full_packet_size = sizeof(Packet_t) + block_size;
    size_t bytes_read = block_size;
    int bytes_sent = full_packet_size;
    Packet_t ack_packet;
    Packet_t *data_packet_ptr;
    socklen_t peer_address_length = sizeof(peer_address);

    data_packet_ptr = malloc(sizeof(Packet_t) + block_size);
    data_packet_ptr->data.opcode = TFTP_DATA;

    printf("Beginning file transmission.\n");

    while (bytes_sent == full_packet_size)
    {
        block_number++;
        resend_counter = 0;
        acknowledged = false;
        data_packet_ptr->data.block_number = block_number;
        bytes_read = fread(data_packet_ptr->data.data, 1, block_size, file);

        if (bytes_read <= 0)
        {
            printf("Read %lu bytes to transmission buffer.\n", bytes_read);

            if (feof(file) != 0)
            {
                printf("Sending final block: %u.\n", block_number);
                bytes_read = 0;
            }
            else
            {
                perror("Failed to read from file");
                free(data_packet_ptr);
                return;
            }
        }
        else if (bytes_read < block_size)
        {
            printf("Sending final block: %u.\n", block_number);
        }

        while (!acknowledged)
        {
            bytes_sent = sendto(data_socket, data_packet_ptr, sizeof(Packet_t) + bytes_read, 0, (struct sockaddr *)&(peer_address), peer_address_length);
            printf("Sent %d bytes of block %d.\n", bytes_sent, block_number);
            // TODO: handle errors
            recvfrom(data_socket, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)&(peer_address), &peer_address_length);

            if (ack_packet.opcode == TFTP_ACK && ack_packet.ack.block_number == block_number)
            {
                acknowledged = true;
                printf ("Block #%u acknowledged!\n", block_number);
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

void receive_file(FILE *file, TFTPMode_t mode, uint16_t block_size, int data_socket, struct sockaddr_in local_address, struct sockaddr_in peer_address)
{
    bool finished = false;
    uint8_t resend_counter = 0;
    int prev_block_number = 0;
    int next_block_number = 1;
    int full_packet_size = block_size + sizeof(Packet_t);
    int bytes_received = full_packet_size;
    Packet_t ack_packet = { .ack.opcode = TFTP_ACK, .ack.block_number = prev_block_number } ;
    Packet_t *data_packet_ptr;
    socklen_t peer_address_length = sizeof(peer_address);

    data_packet_ptr = malloc(full_packet_size);

    printf("Beginning file reception.\n");

    while (!finished)
    {
        bytes_received = recvfrom(data_socket, data_packet_ptr, full_packet_size, 0, (struct sockaddr *)&(peer_address), &peer_address_length);

        if (bytes_received > 0 || data_packet_ptr->data.block_number == next_block_number)
        {

            if (bytes_received < full_packet_size)
            {
                finished = true;
            }
            else
            {
                resend_counter = 0;
                prev_block_number++;
                next_block_number++;
            }

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
