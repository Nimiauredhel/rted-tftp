
#ifndef TFTP_COMMON_H
#define TFTP_COMMON_H

#include "common.h"
#include "networking_common.h"

#define STORAGE_PATH "./storage/"

#define TFTP_MODES_COUNT 2
#define TFTP_MODES_STRING_LENGTH 8
#define TFTP_BLKSIZE_STRING "blksize"
#define TFTP_FILENAME_MAX 255

typedef enum TFTPOpcode
{
    TFTP_RRQ = 1, // read request
    TFTP_WRQ = 2, // write request
    TFTP_DATA = 3,
    TFTP_ACK = 4,
    TFTP_ERROR = 5
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
        uint16_t opcode; // RRQ or WRQ
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
} CommonData_t;

extern CommonData_t tftp_common_data;
const extern char tftp_mode_strings[TFTP_MODES_COUNT][TFTP_MODES_STRING_LENGTH];

void init_storage(void);

void fill_request_packet(Packet_t *buffer, TFTPOpcode_t opcode, const char *file_name, TFTPMode_t mode, uint16_t block_size);

void transmit_file(FILE *file, TFTPMode_t mode, uint16_t block_size, struct sockaddr_in peer_address);
void receive_file(FILE *file, TFTPMode_t mode, uint16_t block_size, struct sockaddr_in peer_address);

#endif
