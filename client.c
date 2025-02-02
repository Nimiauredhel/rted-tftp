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
    static const uint8_t blocksize_blksize_str_len = 7;
    static const uint8_t blocksize_octets_str_len = 5;
    static const char divider = '\0'; // standard specifies 0-terminated strings, only change for debugging

    uint16_t contents_idx = 0;
    char *filename_in_path;
    size_t filename_len;
    size_t transfer_mode_len = strlen(tftp_mode_strings[data->mode]);
    char blocksize_octets_str[6] = {0};

    filename_in_path = strrchr(data->path, '/');

    if (filename_in_path == NULL)
    {
        filename_in_path = data->path;
        filename_len = data->path_len;
    }
    else
    {
        filename_len = strlen(filename_in_path);
    }

    if (data->blocksize > 0 && data->blocksize != TFTP_BLKSIZE_DEFAULT)
    {
        sprintf(blocksize_octets_str, "%05u", data->blocksize);
    }

    size_t contents_size = filename_len + 1 // space for filename + terminator
        + transfer_mode_len + 1 // space for transfer mode + terminator
        + (data->blocksize > 0 ? blocksize_blksize_str_len + blocksize_octets_str_len + 2 : 0); // optional space for blksize & octet fields and terminators
    Packet_t *request_packet_ptr = malloc(sizeof(Packet_t) + contents_size);

    // clearing packet memory to zero
    request_packet_ptr->request.opcode = data->request_opcode;
    memset(request_packet_ptr->request.contents, divider, contents_size);

    // writing filename + terminating 0
    memcpy(request_packet_ptr->request.contents, filename_in_path, filename_len); 
    contents_idx += filename_len;
    memset(request_packet_ptr->request.contents + contents_idx, divider, 1); 
    contents_idx++;

    // writing transfer mode + terminating 0
    memcpy(request_packet_ptr->request.contents + contents_idx, tftp_mode_strings[data->mode], transfer_mode_len); 
    contents_idx += transfer_mode_len;
    memset(request_packet_ptr->request.contents + contents_idx, divider, 1); 

    // if custom blocksize specified, we must add those fields as well
    if (data->blocksize > 0)
    {
        contents_idx++;
        // identifying "blksize" string field + terminator
        memcpy(request_packet_ptr->request.contents + contents_idx, TFTP_BLKSIZE_STRING, blocksize_blksize_str_len); 
        contents_idx += blocksize_blksize_str_len;
        memset(request_packet_ptr->request.contents + contents_idx, divider, 1); 
        contents_idx++;
        // ascii representation of the requested blocksize + terminator
        memcpy(request_packet_ptr->request.contents + contents_idx, blocksize_octets_str, blocksize_octets_str_len); 
        contents_idx += blocksize_octets_str_len;
        memset(request_packet_ptr->request.contents + contents_idx, divider, 1); 
    }

    printf("Sending %s request. Contents:\n", data->request_description);
    fwrite(request_packet_ptr->request.contents, sizeof(char), contents_size, stdout);
    printf("\n");

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
                file = tftp_acquire_fd(data->path, "w");
                if (file == NULL) break;
                tftp_receive_file(file, data->mode, data->blocksize, data->data_socket, data->peer_address);
                operation_success = true;
                break;
            case TFTP_WRQ:
                file = tftp_acquire_fd(data->path, "r");
                if (file == NULL) break;
                tftp_transmit_file(file, data->mode, data->blocksize, data->data_socket, data->peer_address);
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
