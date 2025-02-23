/**
 * The TFTP-Common header declares data and functionality used by both TFTP client and server operations.
 */

#ifndef TFTP_COMMON_H
#define TFTP_COMMON_H

#include "common.h"
#include "networking_common.h"

#define TFTP_OPERATION_MODES_COUNT 4
#define TFTP_OPERATION_MODE_STRING_MAXLENGTH 8

#define TFTP_TRANSFER_MODES_COUNT 3
#define TFTP_TRANSFER_MODE_STRING_MAXLENGTH 9

#define TFTP_OPCODES_COUNT 7
#define TFTP_OPCODE_STRING_MAXLENGTH 6

#define TFTP_BLKSIZE_STRING "blksize"
#define TFTP_FILENAME_MAX 255
#define TFTP_ERROR_MESSAGE_MAX_LENGTH 128
#define TFTP_RESPONSE_PACKET_MAX_SIZE (sizeof(Packet_t) + TFTP_ERROR_MESSAGE_MAX_LENGTH)

typedef enum TFTPOpcode
{
    TFTP_NONE = 0, // none, likely erroneous
    TFTP_RRQ = 1, // read request
    TFTP_WRQ = 2, // write request
    TFTP_DATA = 3, // data packet
    TFTP_ACK = 4, // acknowledgement packet
    TFTP_ERROR = 5, // error packet
    TFTP_DRQ = 6, // delete request
} TFTPOpcode_t;

typedef enum TFTPTransferMode
{
    TFTP_MODE_UNSPECIFIED = -1,
    TFTP_MODE_OCTET = 0,
    TFTP_MODE_NETASCII = 1,
    TFTP_MODE_MAIL = 2,
} TFTPTransferMode_t;

typedef enum TFTPErrorCode
{
    TFTP_ERROR_UNDEFINED = 0,
    TFTP_ERROR_FILE_NOT_FOUND = 1,
    TFTP_ERROR_ACCESS_VIOLATION = 2,
    TFTP_ERROR_OUT_OF_SPACE = 3,
    TFTP_ERROR_ILLEGAL_OPERATION = 4,
    TFTP_ERROR_UNKNOWN_TRANSFER = 5,
    TFTP_ERROR_FILE_EXISTS = 6,
    TFTP_ERROR_UNKNOWN_USER = 7,
} TFTPErrorCode_t;

typedef enum TFTPBlocksize
{
    TFTP_BLKSIZE_UNSPECIFIED = 0,
    TFTP_BLKSIZE_MIN = 8,
    TFTP_BLKSIZE_DEFAULT = 512,
    TFTP_BLKSIZE_MAX = 65464
} TFTPBlocksize_t;

typedef union Packet
{
#pragma pack(push, 1)
    uint16_t opcode;

    struct
    {
        uint16_t opcode; // RRQ, WRQ, or DRQ
        char contents[]; // null-terminated fields: file name, transfer mode, (optional) block size
    } request;

    struct
    {
        uint16_t opcode; // DATA
        uint16_t block_number;
        char data[];
    } data;

    struct
    {
        uint16_t opcode; // ACK
        uint16_t block_number;
    } ack;

    struct
    {
        uint16_t opcode; // ERROR
        uint16_t error_code;
        char error_message[];
    } error;
#pragma pack(pop)
} Packet_t;

typedef enum OperationId
{
    TFTP_OPERATION_UNDEFINED = 0,
    TFTP_OPERATION_RECEIVE = 1,
    TFTP_OPERATION_SEND = 2,
    TFTP_OPERATION_REQUEST_DELETE = 3,
    TFTP_OPERATION_HANDLE_DELETE = 4,

} OperationId_t;

/**
 * This struct holds data that helps filter initial user input,
 * and provide usage instructions if the input is invalid.
 */
typedef struct OperationMode
{
    const uint8_t min_argument_count;
    const char input_string[TFTP_OPERATION_MODE_STRING_MAXLENGTH];
    const char description_string[32];
    const char usage_format_string[64];

} OperationMode_t;

/**
 * This struct holds data defining TFTP operations.
 */
typedef struct OperationData
{
    OperationId_t operation_id;
    TFTPTransferMode_t transfer_mode;
    uint16_t block_size;
    uint16_t path_len;
    int data_socket;
    struct sockaddr_in local_address;
    struct sockaddr_in peer_address;
    socklen_t peer_address_length;
    char request_description[8];
    char path[];
} OperationData_t;

/**
 * This struct holds data used during TFTP file transfer operations.
 * It is separate from the Operation Data struct since not every operation involves a file transfer,
 * and some that potentially do may be aborted before it occurs.
 */
typedef struct TransferData
{
    uint8_t resend_counter;
    uint8_t response_packet_max_size;
    uint16_t data_packet_max_size;
    uint16_t current_block_number;
    int32_t bytes_received;
    int32_t bytes_sent;
    int32_t latest_file_bytes_read;
    uint64_t total_file_bytes_transmitted;
    struct timespec start_clock;
    FILE *file;
    Packet_t *response_packet_ptr;
    Packet_t *data_packet_ptr;
} TransferData_t;

/**
 * This struct holds common data used by both TFTP client and server operations.
 */
typedef struct TFTPCommonData
{
    bool is_server;
    const uint8_t max_retry_count;
    const OperationMode_t operation_modes[TFTP_OPERATION_MODES_COUNT];
    const char transfer_mode_strings[TFTP_TRANSFER_MODES_COUNT][TFTP_TRANSFER_MODE_STRING_MAXLENGTH];
    const char opcode_strings[TFTP_OPCODES_COUNT][TFTP_OPCODE_STRING_MAXLENGTH];
} TFTPCommonData_t;

extern TFTPCommonData_t tftp_common;

struct sockaddr_in init_peer_socket_address(struct in_addr peer_address_bin, in_port_t peer_port_bin);
void tftp_init_bound_data_socket(int *socket_ptr, struct sockaddr_in *address_ptr);

OperationData_t *tftp_init_operation_data(OperationId_t operation, struct sockaddr_in peer_address, char *filename, char *mode_string, char *blocksize_string);
void tftp_free_operation_data(OperationData_t *data);

bool tftp_fill_transfer_data(OperationData_t *operation_data, TransferData_t *transfer_data, bool receiver);
void tftp_free_transfer_data(TransferData_t *data);

bool tftp_transmit_file(OperationData_t *operation_data, TransferData_t *transfer_data);
bool tftp_receive_file(OperationData_t *operation_data, TransferData_t *transfer_data);
bool tftp_await_acknowledgement(uint16_t block_number, OperationData_t *op_data);
bool tftp_send_ack(uint16_t block_number, int socket, const struct sockaddr_in *peer_address_ptr, socklen_t peer_address_length);
void tftp_send_error(TFTPErrorCode_t error_code, const char *error_message, const char *error_item, int data_socket, const struct sockaddr_in *peer_address_ptr, socklen_t peer_address_length);

#endif
