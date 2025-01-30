#include "client.h"

static bool send_request_packet(OperationData_t *data)
{
    size_t contents_size = data->filename_len + 8 + 2;
    Packet_t *request_packet_ptr = malloc(sizeof(Packet_t) + contents_size);
    Packet_t ack_packet;
    char *request_description = data->request_opcode == TFTP_WRQ ? "WRITE"
        : data->request_opcode == TFTP_RRQ ? "READ"
        : "DELETE";

    request_packet_ptr->request.opcode = data->request_opcode;
    memset(request_packet_ptr->request.contents, 0, contents_size);
    memcpy(request_packet_ptr->request.contents, data->filename, data->filename_len); 
    memset(request_packet_ptr->request.contents + data->filename_len, 0, 1); 
    memcpy(request_packet_ptr->request.contents + data->filename_len + 1, tftp_mode_strings[0], strlen(tftp_mode_strings[0])); 

    printf("Sending %s request. Contents:\n%s\n", request_description, request_packet_ptr->request.contents);
    ssize_t bytes_sent = sendto(data->data_socket, request_packet_ptr, sizeof(Packet_t) + contents_size, 0, (struct sockaddr *)&(data->peer_address), data->peer_address_length);

    if (bytes_sent <= 0)
    {
        perror("Failed to send request");
        exit(EXIT_FAILURE);
    }

    ssize_t bytes_received = 0;

    while (bytes_received <= 0)
    {
        bytes_received = recvfrom(data->data_socket, &ack_packet, sizeof(Packet_t), 0, (struct sockaddr *)&(data->peer_address), &(data->peer_address_length));
    }

    if (ack_packet.ack.opcode == TFTP_ACK && ack_packet.ack.block_number == 0)
    {
        printf("%s request acknowledged!\n", request_description);
        return true;
    }
    else
    {
        printf("%s request unacknowledged.\n", request_description);
        exit(EXIT_FAILURE);
    }
}

void client_start(OperationData_t *data)
{
    // open file for operation
    FILE *file = fopen(data->filename, data->request_opcode == TFTP_WRQ ? "r" : "w");

    if (file == NULL)
    {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Opened/created file: %s.\n", data->filename);
    }

    tftp_init_bound_data_socket(&data->data_socket, &data->local_address);

    // send operation request and await acknowledgement
    send_request_packet(data);
    printf("Request acknowledged!\n");
    
    switch (data->request_opcode)
    {
        case TFTP_RRQ:
            tftp_receive_file(file, data->mode, data->blocksize, data->data_socket, data->local_address, data->peer_address);
        case TFTP_WRQ:
            tftp_transmit_file(file, data->mode, data->blocksize, data->data_socket, data->local_address, data->peer_address);
            break;
        case TFTP_DRQ:
          break;
        default:
          break;
    }

    close(data->data_socket);
    free(data);
}
