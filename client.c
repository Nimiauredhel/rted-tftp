#include "client.h"

static bool await_acknowledgement(OperationData_t *data, uint16_t block_number)
{
    uint8_t retry_counter = 0;
    Packet_t ack_packet;
    ssize_t bytes_received = 0;

    while (bytes_received <= 0)
    {
        retry_counter++;
        bytes_received = recvfrom(data->data_socket, &ack_packet, sizeof(Packet_t), 0, (struct sockaddr *)&(data->peer_address), &(data->peer_address_length));
        if (retry_counter > tftp_max_retransmit_count) break;
    }

    if (bytes_received > 0 && ack_packet.ack.opcode == TFTP_ACK && ack_packet.ack.block_number == block_number)
    {
        return true;
    }
    else
    {
        return false;
    }
}

static bool send_request_packet(OperationData_t *data)
{
    size_t contents_size = data->filename_len + 8 + 2;
    Packet_t *request_packet_ptr = malloc(sizeof(Packet_t) + contents_size);

    request_packet_ptr->request.opcode = data->request_opcode;
    memset(request_packet_ptr->request.contents, 0, contents_size);
    memcpy(request_packet_ptr->request.contents, data->filename, data->filename_len); 
    memset(request_packet_ptr->request.contents + data->filename_len, 0, 1); 
    memcpy(request_packet_ptr->request.contents + data->filename_len + 1, tftp_mode_strings[0], strlen(tftp_mode_strings[0])); 

    printf("Sending %s request. Contents:\n%s\n", data->request_description, request_packet_ptr->request.contents);
    ssize_t bytes_sent = sendto(data->data_socket, request_packet_ptr, sizeof(Packet_t) + contents_size, 0, (struct sockaddr *)&(data->peer_address), data->peer_address_length);

    if (bytes_sent <= 0)
    {
        perror("Failed to send request");
        return false;
    }

    return true;
}

void client_start(OperationData_t *data)
{
    bool operation_success = false;
    tftp_init_bound_data_socket(&data->data_socket, &data->local_address);

    // send operation request and await acknowledgement
    if (send_request_packet(data))
    {
        printf("Sent %s request.\n", data->request_description);
    }
    else
    {
        printf("Failed to send %s request.\n", data->request_description);
        exit(EXIT_FAILURE);
    }

    if (await_acknowledgement(data, 0))
    {
        printf("%s request acknowledged!\n", data->request_description);
    }
    else
    {
        printf("%s request unacknowledged.\n", data->request_description);
        exit(EXIT_FAILURE);
    }

    if (data->request_opcode == TFTP_DRQ)
    {
        if(await_acknowledgement(data, 1))
        {
            printf("Deletion confirmed.\n");
        }
        else
        {
            printf("Deletion not confirmed.\n");
        }
    }
    else
    {
        FILE *file;

        switch (data->request_opcode)
        {
            case TFTP_RRQ:
                file = tftp_acquire_fd(data->filename, "w");
                if (file == NULL) break;
                tftp_receive_file(file, data->mode, data->blocksize, data->data_socket, data->local_address, data->peer_address);
                operation_success = true;
            case TFTP_WRQ:
                file = tftp_acquire_fd(data->filename, "r");
                if (file == NULL) break;
                tftp_transmit_file(file, data->mode, data->blocksize, data->data_socket, data->local_address, data->peer_address);
                operation_success = true;
                break;
            default:
              break;
        }
    }

    close(data->data_socket);
    free(data);
    exit(operation_success ? EXIT_SUCCESS : EXIT_FAILURE);
}
