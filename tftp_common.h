
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

typedef enum TFTPMode
{
    TFTP_MODE_UNDEFINED = -1,
    TFTP_MODE_NETASCII = 0,
    TFTP_MODE_OCTET = 1,
} TFTPMode_t;

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
        char contents[]; // null-terminated fields: filename, (optional) block size, mode
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

typedef struct CommonData
{
    /*
     * every operation mode requires at least one socket.
     * in server mode this will serve as the passive requests socket.
     * in any client action (read, write, delete) this will be the socket for that action.
     */
    int primary_socket;
    struct sockaddr_in local_address;
    /*
     * every operation mode requires a path input.
     * in server mode this is the folder to be served to clients.
     * in any client action this is the file name for that action.
     */
    char path[255];
} CommonData_t;

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
    TFTPMode_t mode;
    uint16_t blocksize;
    uint16_t filename_len;
    int data_socket;
    struct sockaddr_in local_address;
    struct sockaddr_in peer_address;
    socklen_t peer_address_length;
    char filename[];
} OperationData_t;

const extern uint8_t tftp_max_retransmit_count;
const extern uint32_t tftp_ack_timeout;
const extern uint32_t tftp_data_timeout;
const extern char tftp_mode_strings[TFTP_MODES_COUNT][TFTP_MODES_STRING_LENGTH];

const extern OperationMode_t tftp_operation_modes[];
extern CommonData_t tftp_common_data;

void tftp_init_storage(void);
void tftp_transmit_file(FILE *file, TFTPMode_t mode, uint16_t block_size, int data_socket, struct sockaddr_in local_address, struct sockaddr_in peer_address);
void tftp_receive_file(FILE *file, TFTPMode_t mode, uint16_t block_size, int data_socket, struct sockaddr_in local_address, struct sockaddr_in peer_address);
OperationData_t *tftp_init_operation_data(TFTPOpcode_t operation, char *peer_address_string, char *filename, char *mode_string, char *blocksize_string);
void tftp_init_bound_data_socket(int *socket_ptr, struct sockaddr_in *address_ptr);

#endif
