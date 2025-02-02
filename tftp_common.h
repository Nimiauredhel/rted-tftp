
#ifndef TFTP_COMMON_H
#define TFTP_COMMON_H

#include "common.h"
#include "networking_common.h"

#define STORAGE_PATH "./storage/"

#define OPERATION_MODES_COUNT 4
#define OPERATION_MODES_MAXLENGTH 8
#define TFTP_MODES_COUNT 2
#define TFTP_MODES_STRING_LENGTH 9
#define TFTP_BLKSIZE_STRING "blksize"
#define TFTP_FILENAME_MAX 255

typedef enum TFTPOpcode
{
    TFTP_RRQ = 1, // read request
    TFTP_WRQ = 2, // write request
    TFTP_DATA = 3,
    TFTP_ACK = 4,
    TFTP_ERROR = 5,
    TFTP_DRQ = 6, // delete request
} TFTPOpcode_t;

typedef enum TFTPTransferMode
{
    TFTP_MODE_UNDEFINED = -1,
    TFTP_MODE_NETASCII = 0,
    TFTP_MODE_OCTET = 1,
} TFTPTransferMode_t;

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

typedef struct OperationMode
{
    const uint8_t min_argument_count;
    const char input_string[OPERATION_MODES_MAXLENGTH];
    const char description_string[32];
    const char usage_format_string[32];

} OperationMode_t;

typedef struct OperationData
{
    TFTPOpcode_t request_opcode;
    TFTPTransferMode_t mode;
    uint16_t blocksize;
    uint16_t path_len;
    int data_socket;
    struct sockaddr_in local_address;
    struct sockaddr_in peer_address;
    socklen_t peer_address_length;
    char request_description[8];
    char path[];
} OperationData_t;

extern const uint8_t tftp_max_retransmit_count;
extern const uint32_t tftp_ack_timeout;
extern const uint32_t tftp_data_timeout;
extern const char tftp_mode_strings[TFTP_MODES_COUNT][TFTP_MODES_STRING_LENGTH];

extern const OperationMode_t tftp_operation_modes[];

void tftp_init_storage(void);
FILE *tftp_acquire_fd(char *path, char *mode);
void tftp_transmit_file(FILE *file, TFTPTransferMode_t mode, uint16_t block_size, int data_socket, struct sockaddr_in peer_address);
void tftp_receive_file(FILE *file, TFTPTransferMode_t mode, uint16_t block_size, int data_socket, struct sockaddr_in peer_address);
OperationData_t *tftp_init_operation_data(TFTPOpcode_t operation, char *peer_address_string, char *filename, char *mode_string, char *blocksize_string);
void tftp_init_bound_data_socket(int *socket_ptr, struct sockaddr_in *address_ptr);

#endif
