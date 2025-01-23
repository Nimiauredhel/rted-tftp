#ifndef NETWORKING_COMMON_H
#define NETWORKING_COMMON_H

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>

#define ENCODING_VERSION 0

#define PORT_BUFF_LENGTH 8
#define ADDRESS_BUFF_LENGTH 40
#define NAME_BUFF_LENGTH 32

#define PORT_MIN 49152
#define PORT_MAX 65535

#define TFTP_RRQ 1 // read request
#define TFTP_WRQ 2 // write request
#define TFTP_DATA 3,
#define TFTP_ACK 4,
#define TFTP_ERROR 5

#define TFTP_NETASCII netascii
#define TFTP_OCTET octet
#define TFTP_BLKSIZE blksize

#pragma pack(push, 1)

typedef union Packet
{
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

} Packet_t;

#pragma pack(pop)

#endif
