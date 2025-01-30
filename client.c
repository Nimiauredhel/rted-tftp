#include "client.h"

void client_wrq(int argc, char *argv[])
{
    // open file to send
    FILE *file = fopen(argv[3], "r");

    if (file == NULL)
    {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Opened file: %s.\n", argv[3]);
    }

    // initialize server address with initial (requests) port
    struct sockaddr_in peer_address =
    {
        .sin_family = AF_INET,
        .sin_port = htons(69),
    };

    if (0 > inet_pton(AF_INET, argv[2], &peer_address.sin_addr))
    {
        perror("Bad destination address");
        exit(EXIT_FAILURE);
    }

    socklen_t peer_address_length = sizeof(peer_address);;

    int data_socket;
    struct sockaddr_in local_address = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY } ;

    tftp_init_bound_data_socket(&data_socket, &local_address);
    // TODO: send rrq and await ack here
    size_t contents_size = strlen(argv[3]) + 8 + 2;
    Packet_t *request_packet_ptr = malloc(sizeof(Packet_t) + contents_size);
    Packet_t ack_packet;

    request_packet_ptr->request.opcode = TFTP_WRQ;
    memset(request_packet_ptr->request.contents, 0, contents_size);
    memcpy(request_packet_ptr->request.contents, argv[3], strlen(argv[3])); 
    memset(request_packet_ptr->request.contents + strlen(argv[3]), 0, 1); 
    memcpy(request_packet_ptr->request.contents + strlen(argv[3]) + 1, tftp_mode_strings[0], strlen(tftp_mode_strings[0])); 

    printf("Sending write request. Contents:\n%s\n", request_packet_ptr->request.contents);
    sendto(data_socket, request_packet_ptr, sizeof(Packet_t) + contents_size, 0, (struct sockaddr *)&(peer_address), peer_address_length);

    ssize_t bytes_received = 0;

    while (bytes_received <= 0)
    {
        bytes_received = recvfrom(data_socket, &ack_packet, sizeof(Packet_t), 0, (struct sockaddr *)&(peer_address), &peer_address_length);
    }

    if (ack_packet.ack.opcode == TFTP_ACK && ack_packet.ack.block_number == 0)
    {
        printf("Write request acknowledged!!! Server noticed me!!!\n");
        transmit_file(file, TFTP_MODE_NETASCII, 512, data_socket, local_address, peer_address);
    }
}

void client_rrq(int argc, char *argv[])
{
    init_storage();
}

void client_drq(int argc, char *argv[])
{
}
