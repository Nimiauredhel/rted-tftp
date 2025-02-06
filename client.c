#include "client.h"

static bool send_request_packet(OperationData_t *data)
{
    static const uint8_t blocksize_blksize_str_len = 8;
    static const uint8_t blocksize_octets_str_len = 6;

    uint16_t contents_idx = 0;
    char *filename_in_path;
    size_t filename_len;
    size_t contents_size;
    size_t full_packet_size;
    size_t transfer_mode_len;
    char blocksize_octets_str[6] = {0};

    filename_in_path = strrchr(data->path, '/');

    if (filename_in_path == NULL)
    {
        filename_in_path = data->path;
        filename_len = data->path_len;
    }
    else
    {
        filename_in_path += 1;
        filename_len = strlen(filename_in_path) + 1;
    }

    // the minimal (DRQ) contents size consists of a filename field + terminator
    contents_size = filename_len;

    // if this is not a delete request, additional fields must be taken into account
    if (data->operation_id != TFTP_OPERATION_REQUEST_DELETE)
    {
        if (data->transfer_mode < 0)
        {
            perror("Invalid transfer mode");
            exit(EXIT_FAILURE);
        }

         transfer_mode_len = strlen(tftp_mode_strings[data->transfer_mode]);

        if (data->block_size > 0 && data->block_size != TFTP_BLKSIZE_DEFAULT)
        {
            sprintf(blocksize_octets_str, "%05u", data->block_size);
        }

        // calculating required space for the additional data fields
        contents_size +=
            // space for transfer mode + terminator
            + transfer_mode_len 
            // optional space for blksize & octet fields + terminators
            + ((data->block_size > 0 && data->block_size != TFTP_BLKSIZE_DEFAULT)
                ? blocksize_blksize_str_len + blocksize_octets_str_len : 0);
    }

    // summing up the packet size
    full_packet_size = sizeof(Packet_t) + contents_size;
    // actually allocating for the packet
    Packet_t *request_packet_ptr = malloc(full_packet_size);

    // zeroing packet memory and setting the opcode field
    explicit_bzero(request_packet_ptr, full_packet_size);

    switch(data->operation_id)
    {
        case TFTP_OPERATION_RECEIVE:
            request_packet_ptr->request.opcode = TFTP_RRQ;
            break;
        case TFTP_OPERATION_SEND:
            request_packet_ptr->request.opcode = TFTP_WRQ;
            break;
        case TFTP_OPERATION_REQUEST_DELETE:
            request_packet_ptr->request.opcode = TFTP_DRQ;
            break;
        case TFTP_OPERATION_UNDEFINED:
        case TFTP_OPERATION_HANDLE_DELETE:
            printf("Invalid operation id: %d", data->operation_id);
            tftp_free_operation_data(data);
            exit(EXIT_FAILURE);
            break;
    }

    // writing filename + terminating 0
    memcpy(request_packet_ptr->request.contents, filename_in_path, filename_len); 
    contents_idx += filename_len;

    // if this is a delete request, no additional fields are required
    if (data->operation_id != TFTP_OPERATION_REQUEST_DELETE)
    {
        // writing transfer mode + terminating 0
        memcpy(request_packet_ptr->request.contents + contents_idx, tftp_mode_strings[data->transfer_mode], transfer_mode_len); 
        contents_idx += transfer_mode_len;

        // if custom blocksize specified, we must add those fields as well
        if (data->block_size > 0 && data->block_size != TFTP_BLKSIZE_DEFAULT)
        {
            contents_idx++;
            // identifying "blksize" string field + terminator
            memcpy(request_packet_ptr->request.contents + contents_idx, TFTP_BLKSIZE_STRING, blocksize_blksize_str_len); 
            contents_idx += blocksize_blksize_str_len;
            // ascii representation of the requested blocksize + terminator
            memcpy(request_packet_ptr->request.contents + contents_idx, blocksize_octets_str, blocksize_octets_str_len); 
            contents_idx += blocksize_octets_str_len;
        }
    }

    // TODO: remove this debug code
    /*FILE *request_log = fopen("last_request_contents", "w");
    printf("Sending %s request. Contents:\n", data->request_description);
    fwrite(request_packet_ptr->request.contents, sizeof(char), contents_size, stdout);
    fwrite(request_packet_ptr->request.contents, sizeof(char), contents_size, request_log);
    fclose(request_log);
    printf("\n");*/

    ssize_t bytes_sent = sendto(data->data_socket, request_packet_ptr, sizeof(Packet_t) + contents_size, 0, (struct sockaddr *)&(data->peer_address), data->peer_address_length);

    if (bytes_sent <= 0)
    {
        perror("Failed to send request");
        return false;
    }

    return true;
}

bool client_start_operation(OperationData_t *op_data)
{
    bool operation_outcome = false;

    // send operation request and await acknowledgement
    if (send_request_packet(op_data))
    {
        printf("Sent %s request.\n", op_data->request_description);
    }
    else
    {
        printf("Failed to send %s request.\n", op_data->request_description);
        return operation_outcome;
    }

    // WRITE and DELETE operations must await an ACK response here;
    // READ operations skip ahead and await the first DATA packet
    if (op_data->operation_id != TFTP_OPERATION_RECEIVE
        && tftp_await_acknowledgement(0, op_data) == false)
    {
        printf("%s request unacknowledged.\n", op_data->request_description);
        return operation_outcome;
    }

    printf("%s request acknowledged!\n", op_data->request_description);

    if (op_data->operation_id == TFTP_OPERATION_REQUEST_DELETE)
    {
        printf("Awaiting final confirmation of file deletion.\n");

        if(tftp_await_acknowledgement(1, op_data))
        {
            printf("Deletion confirmed.\n");
            operation_outcome = true;
        }
        else
        {
            printf("Deletion not confirmed.\n");
        }
    }
    else
    {
        TransferData_t *transfer_data = malloc(sizeof(TransferData_t));

        switch (op_data->operation_id)
        {
            case TFTP_OPERATION_RECEIVE:
                if(tftp_fill_transfer_data(op_data, transfer_data, true))
                {
                    operation_outcome = tftp_receive_file(op_data, transfer_data);
                }
                break;
            case TFTP_OPERATION_SEND:
                if(tftp_fill_transfer_data(op_data, transfer_data, false))
                {
                    operation_outcome = tftp_transmit_file(op_data, transfer_data);
                }
                break;
            default:
              break;
        }

        tftp_free_transfer_data(transfer_data);
    }

    return operation_outcome;
}
